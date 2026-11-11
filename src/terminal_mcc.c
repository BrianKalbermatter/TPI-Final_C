/**
 * @file terminal_mcc.c
 * @brief Implementacion de la consola del Centro de Control de Mision.
 */
#include "terminal_mcc.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/** @brief Un mensaje del historial. */
typedef struct {
  char texto[TERMINAL_LARGO_LINEA];
  TonoTerminal tono;
} LineaTerminal;

static LineaTerminal historial[TERMINAL_MAX_LINEAS];
static int total_lineas = 0;

static ColorRGB color_de_tono(TonoTerminal tono) {
  switch (tono) {
    case TONO_TENUE: return color_rgb(60, 130, 80);
    case TONO_INFO: return color_rgb(110, 210, 240);
    case TONO_OK: return color_rgb(140, 255, 160);
    case TONO_ALERTA: return color_rgb(250, 210, 80);
    case TONO_ERROR: return color_rgb(255, 90, 70);
    case TONO_ENTRADA: return color_rgb(235, 240, 235);
    case TONO_NORMAL:
    default: return color_rgb(90, 220, 120);
  }
}

void terminal_reiniciar(void) {
  total_lineas = 0;
}

void terminal_escribir(TonoTerminal tono, const char *formato, ...) {
  // Historial lleno: se descarta el mensaje mas antiguo (como el scroll de una terminal).
  if (total_lineas == TERMINAL_MAX_LINEAS) {
    memmove(&historial[0], &historial[1], sizeof(LineaTerminal) * (TERMINAL_MAX_LINEAS - 1));
    total_lineas--;
  }

  LineaTerminal *linea = &historial[total_lineas];
  va_list argumentos;
  va_start(argumentos, formato);
  vsnprintf(linea->texto, sizeof(linea->texto), formato, argumentos);
  va_end(argumentos);
  linea->tono = tono;
  total_lineas++;
}

/** Cantidad de renglones que ocupa un texto cortado a 'ancho' columnas. */
static int renglones_de(const char *texto, int ancho) {
  int largo = (int)strlen(texto);
  if (largo == 0) return 1;
  return (largo + ancho - 1) / ancho;
}

void terminal_dibujar(Lienzo *lienzo, int x, int y, int ancho, int alto, double met_s,
                      const char *pregunta, int cuadro, int *cursor_x, int *cursor_y) {
  const ColorRGB borde = color_rgb(40, 140, 70);
  const ColorRGB tenue = color_de_tono(TONO_TENUE);
  const ColorRGB normal = color_de_tono(TONO_NORMAL);
  char texto[TERMINAL_LARGO_LINEA];

  lienzo_caja(lienzo, x, y, ancho, alto, borde);
  lienzo_texto(lienzo, x + 2, y, "[ ORION GNC TERMINAL // HOUSTON MCC ]", color_de_tono(TONO_OK));

  // Encabezado con reloj de mision y testigo de enlace parpadeante.
  snprintf(texto, sizeof texto, "ARTEMIS-OS 4.2  tty1  MET T+%06.0f s", met_s);
  lienzo_texto(lienzo, x + 2, y + 1, texto, tenue);
  lienzo_texto(lienzo, x + ancho - 9, y + 1, (cuadro / 10) % 2 ? "* LINK" : "  LINK", normal);

  for (int col = x + 1; col < x + ancho - 1; col++) {
    lienzo_poner(lienzo, col, y + 2, '-', tenue);
    lienzo_poner(lienzo, col, y + alto - 4, '-', tenue);
  }

  // Zona de mensajes: se llena de abajo hacia arriba con los mas recientes.
  int ancho_texto = ancho - 4;
  int primera_fila = y + 3;
  int ultima_fila = y + alto - 5;
  int fila_libre = ultima_fila;

  for (int i = total_lineas - 1; i >= 0 && fila_libre >= primera_fila; i--) {
    const LineaTerminal *linea = &historial[i];
    int renglones = renglones_de(linea->texto, ancho_texto);
    int largo = (int)strlen(linea->texto);
    ColorRGB color = color_de_tono(linea->tono);

    for (int r = renglones - 1; r >= 0 && fila_libre >= primera_fila; r--) {
      int inicio = r * ancho_texto;
      int cantidad = largo - inicio;
      if (cantidad > ancho_texto) cantidad = ancho_texto;
      for (int c = 0; c < cantidad; c++) {
        lienzo_poner(lienzo, x + 2 + c, fila_libre, linea->texto[inicio + c], color);
      }
      fila_libre--;
    }
  }

  // Linea de entrada.
  int fila_prompt = y + alto - 2;
  lienzo_texto(lienzo, x + 2, fila_prompt, TERMINAL_PROMPT, color_de_tono(TONO_OK));
  *cursor_x = x + 2 + (int)strlen(TERMINAL_PROMPT);
  *cursor_y = fila_prompt;

  if (pregunta != NULL) {
    lienzo_texto(lienzo, x + 2, y + alto - 3, pregunta, color_de_tono(TONO_ALERTA));
  } else if ((cuadro / 8) % 2 == 0) {
    lienzo_poner(lienzo, *cursor_x, fila_prompt, '_', normal); // cursor parpadeante
  }
}
