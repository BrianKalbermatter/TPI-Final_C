#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "utilidades.h"

// -- Registro y seguimiento --
static LOG_LEVELS nivel_min = LOG_LEVEL_TRACE;

#if LOG_DEBUG_MODE

/**
 * @brief Extrae la subruta posterior a 'src/' o 'include/'.
 * @param ruta Ruta completa o relativa del archivo (__FILE__).
 * @return Puntero a la parte posterior de 'src/' o 'include/',
 *         o el nombre del archivo si no contiene dichos prefijos.
 */
static const char *filtrar_ruta_origen(const char *ruta) {
  if (ruta == NULL) {
    return "?";
  }

  const char *pos = ruta;
  const char *mejor_candidato = NULL;

  while (*pos != '\0') {
    int inicio_valido = (pos == ruta) || (*(pos - 1) == '/' || *(pos - 1) == '\\');

    if (inicio_valido) {
      if ((strncmp(pos, "src/", 4) == 0) || (strncmp(pos, "src\\", 4) == 0)) {
        mejor_candidato = pos + 4;
      } else if ((strncmp(pos, "include/", 8) == 0) || (strncmp(pos, "include\\", 8) == 0)) {
        mejor_candidato = pos + 8;
      }
    }
    pos++;
  }

  if (mejor_candidato != NULL) {
    return mejor_candidato;
  }

  // Fallback: si no proviene de src/ o include/, obtener el nombre de archivo (basename)
  const char *sep_slash = strrchr(ruta, '/');
  const char *sep_backslash = strrchr(ruta, '\\');
  const char *ultimo_sep = (sep_slash > sep_backslash) ? sep_slash : sep_backslash;

  if (ultimo_sep != NULL && *(ultimo_sep + 1) != '\0') {
    return ultimo_sep + 1;
  }

  return ruta;
}

void _log_stdout(LOG_LEVELS nivel, const char *restrict nombre_archivo, int linea, const char *restrict funcion, const char *restrict msg, ...) {
  // Aserciones en modo depuracion
  ASSERT(nivel < LOG_LEVEL_SILENCE && nivel >= LOG_LEVEL_TRACE, "Nivel de log fuera de rango y/o invalido.");
  ASSERT(msg != NULL, "El mensaje no debe ser nulo");

  // Validacion defensiva en tiempo de ejecucion
  if (msg == NULL || nivel < LOG_LEVEL_TRACE || nivel >= LOG_LEVEL_SILENCE) {
    return;
  }

  // Filtrado por nivel minimo configurado
  if (nivel < nivel_min) {
    return;
  }

  const char *niveles_str[] = {"TRACE", "DEBUG", "INFO", "WARNING", "ERROR"};

  // Se imprime el nivel y los metadatos de depuracion (archivo, linea y funcion)
  const char *archivo = filtrar_ruta_origen(nombre_archivo);
  const char *fn = (funcion != NULL) ? funcion : "?";
  fprintf(stdout, "[%s] (%s:%d:%s) ", niveles_str[nivel], archivo, linea, fn);

  va_list args;
  va_start(args, msg);
  vfprintf(stdout, msg, args);
  va_end(args);

  fprintf(stdout, "\n");
  fflush(stdout);
}

#else // Release: no se reciben ni imprimen __FILE__, __LINE__ ni __func__

void _log_stdout(LOG_LEVELS nivel, const char *restrict msg, ...) {
  // Validacion defensiva en tiempo de ejecucion
  if (msg == NULL || nivel < LOG_LEVEL_TRACE || nivel >= LOG_LEVEL_SILENCE) {
    return;
  }

  // Filtrado por nivel minimo configurado
  if (nivel < nivel_min) {
    return;
  }

  const char *niveles_str[] = {"TRACE", "DEBUG", "INFO", "WARNING", "ERROR"};

  // Solo se imprime el nivel y el mensaje
  fprintf(stdout, "[%s] ", niveles_str[nivel]);

  va_list args;
  va_start(args, msg);
  vfprintf(stdout, msg, args);
  va_end(args);

  fprintf(stdout, "\n");
  fflush(stdout);
}

#endif // LOG_DEBUG_MODE

void log_set_min_level(LOG_LEVELS _nivel_min) {
  ASSERT(_nivel_min <= LOG_LEVEL_SILENCE && _nivel_min >= LOG_LEVEL_TRACE, "Nivel de log fuera de rango y/o invalido.");

  // Validacion defensiva en tiempo de ejecucion
  if (_nivel_min >= LOG_LEVEL_TRACE && _nivel_min <= LOG_LEVEL_SILENCE) {
    nivel_min = _nivel_min;
  }
}

LOG_LEVELS log_get_min_level(void) {
  return nivel_min;
}

#ifdef _WIN32
void limpiar_consola(void) {
  system("cls");
}
#elif defined(__linux__)
void limpiar_consola(void) {
  if (system("clear") != 0) {
    // Si 'clear' no esta disponible, usar secuencia de escape ANSI VT100
    printf("\033[2J\033[H");
    fflush(stdout);
  }
}
#else
void limpiar_consola(void) {
  // Fallback: emite secuencia ANSI y saltos de linea como respaldo
  printf("\033[2J\033[H");
  for (int i = 0; i < 40; i++) {
    putchar('\n');
  }
  fflush(stdout);
}
#endif // _WIN32

void limpiar_pantalla(void) {
  limpiar_consola();
}

void volver_atras(void) {
  printf("\nPresione ENTER para volver atras...");
  fflush(stdout);
  int c;
  while ((c = getchar()) != '\n' && c != EOF) {
    // vaciar hasta salto de linea
  }
  limpiar_pantalla();
}

