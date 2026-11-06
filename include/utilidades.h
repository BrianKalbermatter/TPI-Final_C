/**
 * @file utilidades.h
 * @brief Definiciones de macros y funciones auxiliares para aserciones y registro (logging).
 * @date 2026-11-04
 */
#ifndef UTILIDADES_H
#define UTILIDADES_H

// -- Aserciones
#include <assert.h>

/**
 * @brief Macro de aserción personalizada con mensaje descriptivo.
 * @details Evalúa una condición lógica en tiempo de ejecución (en modo Debug).
 *          Si la condición resulta falsa, se detiene la ejecución del programa
 *          y se muestra por stderr la expresión fallida junto al mensaje descriptivo.
 *          Si NDEBUG está definido (modo Release), la macro se anula por completo.
 * @param condicion Expresión evaluable como verdadero o falso.
 * @param mensaje Cadena de caracteres explicativa sobre el fallo esperado.
 */
#define ASSERT(condicion, mensaje) assert((condicion) && (mensaje))

// -- Registro y seguimiento --

/**
 * @brief Niveles de severidad disponibles para el registro de mensajes.
 */
typedef enum {
  LOG_LEVEL_TRACE = 0,               /**< Mensajes de trazabilidad detallada y bajo nivel. */
  LOG_LEVEL_DEBUG,                   /**< Mensajes útiles para depuración en desarrollo. */
  LOG_LEVEL_INFO,                    /**< Mensajes de información general del flujo de la aplicación. */
  LOG_LEVEL_WARN,                    /**< Mensajes de advertencia ante situaciones anormales pero no críticas. */
  LOG_LEVEL_WARNING = LOG_LEVEL_WARN,/**< Alias de compatibilidad para LOG_LEVEL_WARN. */
  LOG_LEVEL_ERROR,                   /**< Mensajes de error que indican fallos en una operación. */
  LOG_LEVEL_SILENCE                  /**< Nivel especial para silenciar todos los mensajes de registro. */
} LOG_LEVELS;

/**
 * @brief Configura el nivel mínimo requerido para que un mensaje sea emitido por consola.
 * @details Todos los mensajes cuyo nivel sea estrictamente menor a @p _nivel_min serán ignorados.
 *          Por ejemplo, establecer LOG_LEVEL_INFO ignorará TRACE y DEBUG.
 * @param _nivel_min Nivel mínimo de severidad permitido (LOG_LEVEL_TRACE hasta LOG_LEVEL_SILENCE).
 */
void log_set_min_level(LOG_LEVELS _nivel_min);

/**
 * @def LOG_DEBUG_MODE
 * @brief Indicador de modo depuración para el sistema de logs.
 * @details Activo (1) si DEBUG está definido o NDEBUG no está definido.
 *          En modo Release (0), __FILE__, __LINE__ y __func__ se omiten.
 */
#if defined(DEBUG) || !defined(NDEBUG)
#define LOG_DEBUG_MODE 1
#else
#define LOG_DEBUG_MODE 0
#endif

#if LOG_DEBUG_MODE

/**
 * @brief Función interna de log en modo Debug (incluye archivo, línea y función).
 * @param nivel Nivel de severidad asignado al mensaje.
 * @param nombre_archivo Nombre del archivo fuente donde se originó el log (__FILE__).
 * @param linea Número de línea en el archivo fuente (__LINE__).
 * @param funcion Nombre de la función donde se originó el log (__func__).
 * @param msg Cadena de formato compatible con printf.
 * @param ... Argumentos variables a insertar según la cadena de formato.
 */
#if defined(__GNUC__) || defined(__clang__)
void _log_stdout(LOG_LEVELS nivel, const char *restrict nombre_archivo, int linea, const char *restrict funcion, const char *restrict msg, ...)
    __attribute__((format(printf, 5, 6)));
#else
void _log_stdout(LOG_LEVELS nivel, const char *restrict nombre_archivo, int linea, const char *restrict funcion, const char *restrict msg, ...);
#endif

/**
 * @def LOG_TRACE(msg, ...)
 * @brief Emite un mensaje de registro TRACE indicando archivo, línea y función.
 */
#define LOG_TRACE(msg, ...) _log_stdout(LOG_LEVEL_TRACE, __FILE__, __LINE__, __func__, msg, ##__VA_ARGS__)

/**
 * @def LOG_DEBUG(msg, ...)
 * @brief Emite un mensaje de registro DEBUG indicando archivo, línea y función.
 */
#define LOG_DEBUG(msg, ...) _log_stdout(LOG_LEVEL_DEBUG, __FILE__, __LINE__, __func__, msg, ##__VA_ARGS__)

/**
 * @def LOG_INFO(msg, ...)
 * @brief Emite un mensaje de registro INFO indicando archivo, línea y función.
 */
#define LOG_INFO(msg, ...) _log_stdout(LOG_LEVEL_INFO, __FILE__, __LINE__, __func__, msg, ##__VA_ARGS__)

/**
 * @def LOG_WARNING(msg, ...)
 * @brief Emite un mensaje de registro WARNING indicando archivo, línea y función.
 */
#define LOG_WARNING(msg, ...) _log_stdout(LOG_LEVEL_WARNING, __FILE__, __LINE__, __func__, msg, ##__VA_ARGS__)

/**
 * @def LOG_ERROR(msg, ...)
 * @brief Emite un mensaje de registro ERROR indicando archivo, línea y función.
 */
#define LOG_ERROR(msg, ...) _log_stdout(LOG_LEVEL_ERROR, __FILE__, __LINE__, __func__, msg, ##__VA_ARGS__)

#else // Modo Release: no se capturan ni pasan metadatos de depuración

/**
 * @brief Función interna de log en modo Release (solo nivel y mensaje).
 * @param nivel Nivel de severidad asignado al mensaje.
 * @param msg Cadena de formato compatible con printf.
 * @param ... Argumentos variables a insertar según la cadena de formato.
 */
#if defined(__GNUC__) || defined(__clang__)
void _log_stdout(LOG_LEVELS nivel, const char *restrict msg, ...)
    __attribute__((format(printf, 2, 3)));
#else
void _log_stdout(LOG_LEVELS nivel, const char *restrict msg, ...);
#endif

/**
 * @def LOG_TRACE(msg, ...)
 * @brief Emite un mensaje de registro TRACE (sin metadatos de depuración).
 */
#define LOG_TRACE(msg, ...) _log_stdout(LOG_LEVEL_TRACE, msg, ##__VA_ARGS__)

/**
 * @def LOG_DEBUG(msg, ...)
 * @brief Emite un mensaje de registro DEBUG (sin metadatos de depuración).
 */
#define LOG_DEBUG(msg, ...) _log_stdout(LOG_LEVEL_DEBUG, msg, ##__VA_ARGS__)

/**
 * @def LOG_INFO(msg, ...)
 * @brief Emite un mensaje de registro INFO (sin metadatos de depuración).
 */
#define LOG_INFO(msg, ...) _log_stdout(LOG_LEVEL_INFO, msg, ##__VA_ARGS__)

/**
 * @def LOG_WARNING(msg, ...)
 * @brief Emite un mensaje de registro WARNING (sin metadatos de depuración).
 */
#define LOG_WARNING(msg, ...) _log_stdout(LOG_LEVEL_WARNING, msg, ##__VA_ARGS__)

/**
 * @def LOG_ERROR(msg, ...)
 * @brief Emite un mensaje de registro ERROR (sin metadatos de depuración).
 */
#define LOG_ERROR(msg, ...) _log_stdout(LOG_LEVEL_ERROR, msg, ##__VA_ARGS__)

#endif // LOG_DEBUG_MODE

/**
 * @brief Configura la consola para soportar la codificación UTF-8.
 * @details En sistemas Windows, establece la página de códigos de la consola
 *          a UTF-8 (código 65001 / `CP_UTF8`), permitiendo la correcta visualización
 *          y procesamiento de caracteres especiales del español, tales como
 *          vocales con tilde (á, é, í, ó, ú), la letra eñe (ñ, Ñ) y signos (¿, ¡).
 *          En sistemas tipo Unix/Linux, los emuladores de terminal operan
 *          en UTF-8 de manera predeterminada, por lo que la función no realiza ninguna acción.
 */
#ifdef _WIN32
#include <windows.h>
static inline void activar_utf8(void) {
  SetConsoleOutputCP(CP_UTF8);
  SetConsoleCP(CP_UTF8);
}
#else
static inline void activar_utf8(void) { (void)0; }
#endif // _WIN32

// -- Control de interfaz y pantalla

/**
 * @brief Limpia la pantalla de la terminal o consola.
 * @details Utiliza el comando nativo correspondiente a la plataforma:
 *          - Windows: `cls` a través del sistema.
 *          - Linux: `clear` con respaldo de secuencias de escape ANSI VT100 (`\033[2J\033[H`).
 *          - Otros sistemas: combinación de secuencias ANSI y saltos de línea como respaldo.
 */
void limpiar_consola(void);

/**
 * @brief Alias estándar para limpiar la pantalla de la consola.
 */
void limpiar_pantalla(void);

/**
 * @brief Pausa la ejecución hasta que el usuario presione ENTER y luego limpia la pantalla.
 * @details Facilita la navegación en menús de consola con opción de regreso al menú anterior.
 */
void volver_atras(void);

#endif // UTILIDADES_H
