cmake_minimum_required(VERSION 3.5...4.0)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING "Minimum policy version for all subprojects")

project(ProgettoEsame_SistemaSolare LANGUAGES CXX)

# Forza CMake a mettere tutti gli eseguibili direttamente nella cartella build
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# -----------------------------------------------------------------------------
# SCARICAMENTO DIPENDENZE (SFML 3.0.2 e GLM 1.0.3)
# -----------------------------------------------------------------------------
include(FetchContent)

# Scarica SFML in modo ottimizzato
FetchContent_Declare(SFML
    GIT_REPOSITORY https://github.com/SFML/SFML.git
    GIT_TAG 3.0.2
    GIT_SHALLOW ON
    EXCLUDE_FROM_ALL
    SYSTEM)
FetchContent_MakeAvailable(SFML)

# Scarica GLM senza cronologia commit
FetchContent_Declare(glm
    GIT_REPOSITORY  https://github.com/g-truc/glm.git
    GIT_TAG   1.0.3
    GIT_SHALLOW ON
)
FetchContent_MakeAvailable(glm)

# -----------------------------------------------------------------------------
# CONFIGURAZIONE COMPILATORE E OPENGL
# -----------------------------------------------------------------------------
add_library(common INTERFACE)
target_compile_features(common INTERFACE cxx_std_17)
target_compile_options(common INTERFACE
    $<$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>,$<CXX_COMPILER_ID:GNU>>: -Wall -DGL_SILENCE_DEPRECATION>
)

# Cerca OpenGL nel sistema operativo
find_package(OpenGL REQUIRED)

# -----------------------------------------------------------------------------
# LA FUNZIONE MAGICA PER LE RISORSE
# Deve essere definita PRIMA di dichiarare gli eseguibili
# -----------------------------------------------------------------------------
function(setup_target_resources target_name)
    
    # --- IL FIX PER LINUX ---
    # Rinomina l'eseguibile per evitare che collida con il nome della cartella
    if(UNIX)
        set_target_properties(${target_name} PROPERTIES OUTPUT_NAME "${target_name}.bin")
    endif()
    # ------------------------

    # Copia le Risorse e gli Shader
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

# -----------------------------------------------------------------------------
# GESTIONE DELLE TAPPE D'ESAME
# -----------------------------------------------------------------------------

# Tappa 01
add_executable(Tappa01 Tappa01/main.cc)
target_link_libraries(Tappa01 PRIVATE common SFML::Window SFML::Graphics OpenGL::GL glm::glm-header-only)
setup_target_resources(Tappa01)

# Tappa 02
add_executable(Tappa02 Tappa02/main.cc)
target_link_libraries(Tappa02 PRIVATE common SFML::Window SFML::Graphics OpenGL::GL glm::glm-header-only)
setup_target_resources(Tappa02)

# Tappa 03
add_executable(Tappa03 Tappa03/main.cc)
target_link_libraries(Tappa03 PRIVATE common SFML::Window SFML::Graphics OpenGL::GL glm::glm-header-only)
setup_target_resources(Tappa03)

# Tappa 04
add_executable(Tappa04 Tappa04/main.cc)
target_link_libraries(Tappa04 PRIVATE common SFML::Window SFML::Graphics OpenGL::GL glm::glm-header-only)
setup_target_resources(Tappa04)

# Tappa 05
add_executable(Tappa05 Tappa05/main.cc)
target_link_libraries(Tappa05 PRIVATE common SFML::Window SFML::Graphics OpenGL::GL glm::glm-header-only)
setup_target_resources(Tappa05)

# Tappa 06
add_executable(Tappa06 Tappa06/main.cc)
target_link_libraries(Tappa06 PRIVATE common SFML::Window SFML::Graphics OpenGL::GL glm::glm-header-only)
setup_target_resources(Tappa06)

# Tappa 07
add_executable(Tappa07 Tappa07/main.cc)
target_link_libraries(Tappa07 PRIVATE common SFML::Window SFML::Graphics OpenGL::GL glm::glm-header-only)
setup_target_resources(Tappa07)

# Tappa 08
add_executable(Tappa08 Tappa08/main.cc)
target_link_libraries(Tappa08 PRIVATE common SFML::Window SFML::Graphics OpenGL::GL glm::glm-header-only)
setup_target_resources(Tappa08)

# Tappa 09
add_executable(Tappa09 Tappa09/main.cc)
target_link_libraries(Tappa09 PRIVATE common SFML::Window SFML::Graphics OpenGL::GL glm::glm-header-only)
setup_target_resources(Tappa09)

# Tappa 10
add_executable(Tappa10 Tappa10/main.cc)
target_link_libraries(Tappa10 PRIVATE common SFML::Window SFML::Graphics OpenGL::GL glm::glm-header-only)
setup_target_resources(Tappa10)


add_executable(Tappa11 Tappa11/main.cc)
target_link_libraries(Tappa11 PRIVATE common SFML::Window SFML::Graphics OpenGL::GL glm::glm-header-only)
setup_target_resources(Tappa11)


add_executable(SolarSystem SolarSystem/main.cc)
target_link_libraries(SolarSystem PRIVATE common SFML::Window SFML::Graphics OpenGL::GL glm::glm-header-only)
setup_target_resources(SolarSystem)
