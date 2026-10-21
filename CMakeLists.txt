cmake_minimum_required(VERSION 3.5...4.0)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING "Minimum policy version for all subprojects")

project(ProgettoEsame_SistemaSolare LANGUAGES CXX)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})


include(FetchContent)

FetchContent_Declare(SFML
    GIT_REPOSITORY https://github.com/SFML/SFML.git
    GIT_TAG 3.0.2
    GIT_SHALLOW ON
    EXCLUDE_FROM_ALL
    SYSTEM)
FetchContent_MakeAvailable(SFML)

FetchContent_Declare(glm
    GIT_REPOSITORY  https://github.com/g-truc/glm.git
    GIT_TAG   1.0.3
    GIT_SHALLOW ON
)
FetchContent_MakeAvailable(glm)

add_library(common INTERFACE)
target_compile_features(common INTERFACE cxx_std_17)
target_compile_options(common INTERFACE
    $<$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>,$<CXX_COMPILER_ID:GNU>>: -Wall -Wextra -DGL_SILENCE_DEPRECATION>
)

find_package(OpenGL REQUIRED)

function(setup_target_resources target_name)
    

    if(UNIX)
        set_target_properties(${target_name} PROPERTIES OUTPUT_NAME "${target_name}.bin")
    endif()

    # On every build, not only when the binary relinks: a shader or a data file can
    # change on its own.
    if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.26)
        set(copy_dir copy_directory_if_different)
    else()
        set(copy_dir copy_directory)
    endif()
    add_custom_target(${target_name}_files ALL
        COMMAND ${CMAKE_COMMAND} -E ${copy_dir}
            "${CMAKE_SOURCE_DIR}/resources" 
            "$<TARGET_FILE_DIR:${target_name}>/resources"
        COMMAND ${CMAKE_COMMAND} -E ${copy_dir}
            "${CMAKE_SOURCE_DIR}/${target_name}" 
            "$<TARGET_FILE_DIR:${target_name}>/${target_name}"
    )
    add_dependencies(${target_name}_files ${target_name})

    if(WIN32)
        add_custom_command(TARGET ${target_name} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory 
                "${CMAKE_BINARY_DIR}/_deps/sfml-build/bin" 
                "$<TARGET_FILE_DIR:${target_name}>/"
        )
    endif()
endfunction()


add_executable(SolarSystem SolarSystem/main.cc)
target_link_libraries(SolarSystem PRIVATE common SFML::Window SFML::Graphics OpenGL::GL glm::glm-header-only)
setup_target_resources(SolarSystem)
