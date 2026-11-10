/**
 * @file render.c
 * @brief Implementacion del mini motor de render ASCII.
 */
#define _POSIX_C_SOURCE 200809L // nanosleep() con -std=c11

#include "render.h"
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#endif
#else
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>
#endif

/* Rampa de brillo: de menor a mayor densidad visual. */
static const char RAMPA_LUZ[] = " .:-=+*#%@";
#define LARGO_RAMPA ((int)(sizeof(RAMPA_LUZ) - 1))

/* Tamano usado si no se puede detectar la terminal. */
#define COLUMNAS_POR_DEFECTO 160
#define FILAS_POR_DEFECTO 45

/* Peor caso por celda: "\x1b[38;2;255;255;255m" (19 bytes) + el simbolo. */
#define BYTES_POR_CELDA 20
/* Posicionamiento del cursor + fondo negro + reseteo de cada fila. */
#define BYTES_POR_FILA 40

static char buffer_salida[LIENZO_MAX_ALTO * (LIENZO_MAX_ANCHO * BYTES_POR_CELDA + BYTES_POR_FILA) + 16];

static double limitar(double valor, double minimo, double maximo) {
  if (valor < minimo) return minimo;
  if (valor > maximo) return maximo;
  return valor;
}

/* Cuantiza un canal a multiplos de 8: colores casi iguales se emiten como uno solo,
 * lo que reduce mucho la cantidad de secuencias de color por cuadro. */
static int cuantizar(unsigned char canal) {
  return canal & 0xF8;
}

int render_iniciar(void) {
#ifdef _WIN32
  HANDLE salida = GetStdHandle(STD_OUTPUT_HANDLE);
  DWORD modo = 0;
  if (salida == INVALID_HANDLE_VALUE || !GetConsoleMode(salida, &modo)) return 0;
  if (!SetConsoleMode(salida, modo | ENABLE_VIRTUAL_TERMINAL_PROCESSING)) return 0;
#endif
  // Pantalla alternativa: al salir, la consola vuelve a mostrar lo que tenia antes.
  printf("\x1b[?1049h\x1b[2J\x1b[H\x1b[?25l");
  fflush(stdout);
  return 1;
}

void render_finalizar(void) {
  printf("\x1b[0m\x1b[?25h\x1b[?1049l");
  fflush(stdout);
}

int render_tamano_terminal(int *columnas, int *filas) {
  *columnas = COLUMNAS_POR_DEFECTO;
  *filas = FILAS_POR_DEFECTO;
#ifdef _WIN32
  CONSOLE_SCREEN_BUFFER_INFO info;
  if (!GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &info)) return 0;
  *columnas = info.srWindow.Right - info.srWindow.Left + 1;
  *filas = info.srWindow.Bottom - info.srWindow.Top + 1;
#else
  struct winsize ventana;
  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ventana) != 0 || ventana.ws_col == 0) return 0;
  *columnas = ventana.ws_col;
  *filas = ventana.ws_row;
#endif
  if (*columnas > LIENZO_MAX_ANCHO) *columnas = LIENZO_MAX_ANCHO;
  if (*filas > LIENZO_MAX_ALTO) *filas = LIENZO_MAX_ALTO;
  return 1;
}

void render_presentar(const Lienzo *lienzo) {
  size_t n = 0;

  for (int y = 0; y < lienzo->alto; y++) {
    // Cada fila se posiciona explicitamente: nunca se emite '\n', asi la pantalla no se desplaza.
    n += (size_t)sprintf(buffer_salida + n, "\x1b[%d;1H\x1b[48;2;0;0;0m", y + 1);
    int hay_color = 0;
    int ultimo_r = 0;
    int ultimo_g = 0;
    int ultimo_b = 0;
    // En la ultima fila no se escribe la ultima columna: evita el desplazamiento automatico.
    int columnas = (y == lienzo->alto - 1) ? lienzo->ancho - 1 : lienzo->ancho;

    for (int x = 0; x < columnas; x++) {
      const Celda *celda = &lienzo->celdas[y][x];
      int r = cuantizar(celda->color.r);
      int g = cuantizar(celda->color.g);
      int b = cuantizar(celda->color.b);
      // Solo se emite la secuencia de color cuando cambia: reduce el volumen de salida.
      if (celda->simbolo != ' ' && (!hay_color || r != ultimo_r || g != ultimo_g || b != ultimo_b)) {
        n += (size_t)sprintf(buffer_salida + n, "\x1b[38;2;%d;%d;%dm", r, g, b);
        ultimo_r = r;
        ultimo_g = g;
        ultimo_b = b;
        hay_color = 1;
      }
      buffer_salida[n++] = celda->simbolo;
    }
    n += (size_t)sprintf(buffer_salida + n, "\x1b[0m");
  }

  fwrite(buffer_salida, 1, n, stdout);
  fflush(stdout);
}

void render_borrar_pantalla(void) {
  printf("\x1b[0m\x1b[2J");
  fflush(stdout);
}

void render_mostrar_cursor(int x, int y) {
  printf("\x1b[0m\x1b[38;2;120;255;150m\x1b[%d;%dH\x1b[?25h", y + 1, x + 1);
  fflush(stdout);
}

void render_ocultar_cursor(void) {
  printf("\x1b[?25l");
  fflush(stdout);
}

void render_esperar_ms(int milisegundos) {
  if (milisegundos <= 0) return;
#ifdef _WIN32
  Sleep((DWORD)milisegundos);
#else
  struct timespec espera;
  espera.tv_sec = milisegundos / 1000;
  espera.tv_nsec = (long)(milisegundos % 1000) * 1000000L;
  nanosleep(&espera, NULL);
#endif
}

void lienzo_limpiar(Lienzo *lienzo, int ancho, int alto) {
  lienzo->ancho = (int)limitar(ancho, 1, LIENZO_MAX_ANCHO);
  lienzo->alto = (int)limitar(alto, 1, LIENZO_MAX_ALTO);
  for (int y = 0; y < lienzo->alto; y++) {
    for (int x = 0; x < lienzo->ancho; x++) {
      lienzo->celdas[y][x].simbolo = ' ';
      lienzo->celdas[y][x].color = color_rgb(0, 0, 0);
    }
  }
}

void lienzo_poner(Lienzo *lienzo, int x, int y, char simbolo, ColorRGB color) {
  if (x < 0 || x >= lienzo->ancho || y < 0 || y >= lienzo->alto) return;
  lienzo->celdas[y][x].simbolo = simbolo;
  lienzo->celdas[y][x].color = color;
}

void lienzo_luz(Lienzo *lienzo, int x, int y, double luz, ColorRGB color) {
  luz = limitar(luz, 0.0, 1.0);
  int indice = (int)(luz * (LARGO_RAMPA - 1) + 0.5);
  ColorRGB atenuado = color_mezclar(color_rgb(0, 0, 0), color, 0.35 + 0.65 * luz);
  lienzo_poner(lienzo, x, y, RAMPA_LUZ[indice], atenuado);
}

void lienzo_texto(Lienzo *lienzo, int x, int y, const char *texto, ColorRGB color) {
  for (int i = 0; texto[i] != '\0'; i++) {
    lienzo_poner(lienzo, x + i, y, texto[i], color);
  }
}

void lienzo_caja(Lienzo *lienzo, int x, int y, int ancho, int alto, ColorRGB borde) {
  for (int fila = y; fila < y + alto; fila++) {
    for (int col = x; col < x + ancho; col++) {
      int es_borde_h = (fila == y || fila == y + alto - 1);
      int es_borde_v = (col == x || col == x + ancho - 1);
      char simbolo = ' ';
      if (es_borde_h && es_borde_v) simbolo = '+';
      else if (es_borde_h) simbolo = '-';
      else if (es_borde_v) simbolo = '|';
      lienzo_poner(lienzo, col, fila, simbolo, borde);
    }
  }
}

void lienzo_barra(Lienzo *lienzo, int x, int y, int ancho, double fraccion, ColorRGB color) {
  int interior = ancho - 2;
  int llenas = (int)(limitar(fraccion, 0.0, 1.0) * interior + 0.5);
  ColorRGB gris = color_rgb(70, 70, 80);

  lienzo_poner(lienzo, x, y, '[', gris);
  for (int i = 0; i < interior; i++) {
    if (i < llenas) lienzo_poner(lienzo, x + 1 + i, y, '#', color);
    else lienzo_poner(lienzo, x + 1 + i, y, '.', gris);
  }
  lienzo_poner(lienzo, x + ancho - 1, y, ']', gris);
}

ColorRGB color_rgb(int r, int g, int b) {
  ColorRGB color;
  color.r = (unsigned char)limitar(r, 0, 255);
  color.g = (unsigned char)limitar(g, 0, 255);
  color.b = (unsigned char)limitar(b, 0, 255);
  return color;
}

ColorRGB color_mezclar(ColorRGB a, ColorRGB b, double t) {
  t = limitar(t, 0.0, 1.0);
  return color_rgb((int)(a.r + (b.r - a.r) * t), (int)(a.g + (b.g - a.g) * t),
                   (int)(a.b + (b.b - a.b) * t));
}

double ruido_hash(int x, int y, int semilla) {
  unsigned int h = (unsigned int)x * 374761393u + (unsigned int)y * 668265263u +
                   (unsigned int)semilla * 2246822519u;
  h = (h ^ (h >> 13)) * 1274126177u;
  h ^= h >> 16;
  return (double)(h & 0xFFFFFFu) / (double)0x1000000u;
}
