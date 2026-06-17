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
    $<$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>,$<CXX_COMPILER_ID:GNU>>: -Wall -DGL_SILENCE_DEPRECATION>
)

find_package(OpenGL REQUIRED)


function(setup_target_resources target_name)

    if(UNIX)
        set_target_properties(${target_name} PROPERTIES OUTPUT_NAME "${target_name}.bin")
    endif()

    add_custom_command(TARGET ${target_name} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory 
            "${CMAKE_SOURCE_DIR}/Risorse" 
            "$<TARGET_FILE_DIR:${target_name}>/Risorse"
        COMMAND ${CMAKE_COMMAND} -E copy_directory 
            "${CMAKE_SOURCE_DIR}/${target_name}" 
            "$<TARGET_FILE_DIR:${target_name}>/${target_name}"
    )

    # Copia le DLL (Viene eseguito SOLO su Windows)
    if(WIN32)
        add_custom_command(TARGET ${target_name} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory 
                "${CMAKE_BINARY_DIR}/_deps/sfml-build/bin" 
                "$<TARGET_FILE_DIR:${target_name}>/"
        )
    endif()
endfunction()


add_executable(Tappa12 Tappa12/main.cc)
target_link_libraries(Tappa12 PRIVATE common SFML::Window SFML::Graphics OpenGL::GL glm::glm-header-only)
setup_target_resources(Tappa12)
