cmake_minimum_required(VERSION 3.20)

project(TPFinal C)

########################################
# Config. global compilacion
########################################
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)
add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
add_compile_definitions($<$<CONFIG:Debug>:DEBUG>)

########################################
# Directorio de salida para binarios
########################################
# -- Ejecutables
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/out/${CMAKE_BUILD_TYPE}/bin")
# -- Librarias dinamicas
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/out/${CMAKE_BUILD_TYPE}/lib")
# -- Librarias estaticas
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/out/${CMAKE_BUILD_TYPE}/lib")


########################################
# Compilar el ejecutable simulador
########################################
add_executable(simulador
  src/main.c
  src/simulador.c
  src/utilidades.c
  src/validaciones.c
)

target_include_directories(simulador PRIVATE include)
target_link_libraries(simulador PRIVATE m)