/**
 * @file terminal_mcc.h
 * @brief Consola del Centro de Control de Mision (MCC) dibujada dentro del lienzo.
 *
 * Mantiene un historial de mensajes (como la salida de una terminal) y lo dibuja
 * en un panel con estilo de monitor de fosforo verde. Las lineas largas se
 * cortan al ancho del panel y siempre se muestran las mas recientes.
 */
#ifndef TERMINAL_MCC_H
#define TERMINAL_MCC_H

#include "render.h"

#define TERMINAL_MAX_LINEAS 80   /**< Mensajes que se conservan en el historial. */
#define TERMINAL_LARGO_LINEA 160 /**< Largo maximo de cada mensaje. */
#define TERMINAL_PROMPT "orion@mcc:~$ "

/** @brief Tono (color) con el que se muestra un mensaje. */
typedef enum {
  TONO_NORMAL = 0, /**< Texto comun (verde). */
  TONO_TENUE,      /**< Texto secundario (verde oscuro). */
  TONO_INFO,       /**< Informacion destacada (celeste). */
  TONO_OK,         /**< Confirmaciones (verde brillante). */
  TONO_ALERTA,     /**< Advertencias (amarillo). */
  TONO_ERROR,      /**< Errores y fallos (rojo). */
  TONO_ENTRADA     /**< Lo que escribio el operador (blanco). */
} TonoTerminal;

/** @brief Borra el historial de mensajes. */
void terminal_reiniciar(void);

/** @brief Agrega un mensaje al historial (con formato estilo printf). */
void terminal_escribir(TonoTerminal tono, const char *formato, ...);

/**
 * @brief Dibuja el panel de la terminal en el rectangulo indicado del lienzo.
 * @param lienzo Lienzo destino.
 * @param x Columna del borde izquierdo.
 * @param y Fila del borde superior.
 * @param ancho Ancho total del panel (incluye bordes).
 * @param alto Alto total del panel (incluye bordes).
 * @param met_s Tiempo transcurrido de mision (s), para el encabezado.
 * @param pregunta Pregunta a mostrar sobre la linea de entrada, o NULL si no se espera dato.
 * @param cuadro Numero de cuadro (para el cursor parpadeante).
 * @param cursor_x Salida: columna donde el operador escribe la respuesta.
 * @param cursor_y Salida: fila donde el operador escribe la respuesta.
 */
void terminal_dibujar(Lienzo *lienzo, int x, int y, int ancho, int alto, double met_s,
                      const char *pregunta, int cuadro, int *cursor_x, int *cursor_y);

#endif // TERMINAL_MCC_H
