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
            "${CMAKE_SOURCE_DIR}/resources" 
            "$<TARGET_FILE_DIR:${target_name}>/resources"
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


add_executable(SolarSystem SolarSystem/main.cc)
target_link_libraries(SolarSystem PRIVATE common SFML::Window SFML::Graphics OpenGL::GL glm::glm-header-only)
setup_target_resources(SolarSystem)
