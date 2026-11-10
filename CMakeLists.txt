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
# Configuración Curses (PDCurses / ncurses)
########################################
if(WIN32)
  set(PDCURSES_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third-party/PDCurses")
  file(GLOB PDCURSES_COMMON_SRCS "${PDCURSES_DIR}/pdcurses/*.c")
  file(GLOB PDCURSES_WINCON_SRCS "${PDCURSES_DIR}/wincon/*.c")

  add_library(pdcurses STATIC
    ${PDCURSES_COMMON_SRCS}
    ${PDCURSES_WINCON_SRCS}
  )
  # Activar soporte para caracteres anchos y decodificación forzada de UTF-8 en PDCurses
  target_compile_definitions(pdcurses PUBLIC
    PDC_WIDE
    PDC_FORCE_UTF8
  )
  target_include_directories(pdcurses PUBLIC
    "${PDCURSES_DIR}"
  )
  # Silenciar advertencias en código de terceros
  if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(pdcurses PRIVATE -w)
  endif()

  set(CURSES_LIB pdcurses)
  set(CURSES_INC "${PDCURSES_DIR}")
else()
  # En Linux / macOS se utiliza ncurses del sistema (soporte UTF-8 con libncursesw)
  set(CURSES_NEED_NCURSES TRUE)
  set(CURSES_NEED_WIDE TRUE)
  find_package(Curses REQUIRED)
  set(CURSES_LIB ${CURSES_LIBRARIES})
  set(CURSES_INC ${CURSES_INCLUDE_DIRS})
endif()

########################################
# Compilar el ejecutable simulador
########################################
add_executable(simulador
  src/main.c
  src/simulador.c
  src/utilidades.c
  src/validaciones.c
  src/tui.c
)

target_include_directories(simulador PRIVATE include ${CURSES_INC})
target_link_libraries(simulador PRIVATE ${CURSES_LIB} m)
