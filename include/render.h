/**
 * @file render.h
 * @brief Mini motor de render ASCII para consola (lienzo de celdas + color de 24 bits).
 *
 * Funcionamiento:
 *  1. Cada cuadro se "pinta" en un lienzo en memoria (matriz de celdas).
 *  2. Cada celda guarda un simbolo y un color RGB.
 *  3. render_presentar() vuelca el lienzo completo de una sola vez,
 *     posicionando el cursor al inicio de cada fila (sin limpiar la pantalla),
 *     lo que evita el parpadeo entre cuadros.
 *
 * El brillo de una zona se traduce a un caracter mediante una rampa de luz
 * (" .:-=+*#%@"): cuanto mas brillante, mas "denso" es el caracter.
 *
 * El lienzo ocupa toda la terminal: su tamano se detecta en tiempo de ejecucion
 * y se limita a LIENZO_MAX_ANCHO x LIENZO_MAX_ALTO.
 */
#ifndef RENDER_H
#define RENDER_H

#define LIENZO_MAX_ANCHO 320 /**< Columnas maximas del lienzo (caracteres). */
#define LIENZO_MAX_ALTO 100  /**< Filas maximas del lienzo (caracteres). */

/** @brief Color en formato RGB de 8 bits por canal. */
typedef struct {
  unsigned char r;
  unsigned char g;
  unsigned char b;
} ColorRGB;

/** @brief Celda del lienzo: un caracter con su color. */
typedef struct {
  char simbolo;
  ColorRGB color;
} Celda;

/** @brief Lienzo (framebuffer) donde se compone cada cuadro. */
typedef struct {
  int ancho; /**< Columnas en uso. */
  int alto;  /**< Filas en uso. */
  Celda celdas[LIENZO_MAX_ALTO][LIENZO_MAX_ANCHO];
} Lienzo;

/**
 * @brief Prepara la consola: activa secuencias ANSI (en Windows), abre la pantalla
 *        alternativa y oculta el cursor.
 * @return 1 si la consola soporta el render, 0 en caso contrario.
 */
int render_iniciar(void);

/** @brief Restaura colores, cursor y la pantalla original de la consola. */
void render_finalizar(void);

/**
 * @brief Obtiene el tamano visible de la terminal.
 * @param columnas Salida: cantidad de columnas.
 * @param filas Salida: cantidad de filas.
 * @return 1 si se pudo detectar, 0 si se usaron valores por defecto.
 */
int render_tamano_terminal(int *columnas, int *filas);

/** @brief Vuelca el lienzo completo en la consola (un cuadro). */
void render_presentar(const Lienzo *lienzo);

/** @brief Borra la pantalla fisica (util cuando cambia el tamano de la terminal). */
void render_borrar_pantalla(void);

/** @brief Ubica el cursor en (x, y) del lienzo y lo hace visible (para leer datos). */
void render_mostrar_cursor(int x, int y);

/** @brief Oculta el cursor. */
void render_ocultar_cursor(void);

/** @brief Detiene la ejecucion la cantidad de milisegundos indicada. */
void render_esperar_ms(int milisegundos);

/** @brief Ajusta el tamano del lienzo y deja todas las celdas vacias. */
void lienzo_limpiar(Lienzo *lienzo, int ancho, int alto);

/** @brief Escribe un simbolo en (x, y). Las coordenadas fuera del lienzo se ignoran. */
void lienzo_poner(Lienzo *lienzo, int x, int y, char simbolo, ColorRGB color);

/**
 * @brief Pinta una celda segun su nivel de luz (0.0 a 1.0) usando la rampa de brillo.
 * @details El color se atenua junto con la luz para dar profundidad a la escena.
 */
void lienzo_luz(Lienzo *lienzo, int x, int y, double luz, ColorRGB color);

/** @brief Escribe un texto horizontal a partir de (x, y), recortado al lienzo. */
void lienzo_texto(Lienzo *lienzo, int x, int y, const char *texto, ColorRGB color);

/** @brief Dibuja un recuadro con borde ASCII y su interior vacio. */
void lienzo_caja(Lienzo *lienzo, int x, int y, int ancho, int alto, ColorRGB borde);

/** @brief Dibuja una barra de progreso "[####....]" con la fraccion indicada (0.0 a 1.0). */
void lienzo_barra(Lienzo *lienzo, int x, int y, int ancho, double fraccion, ColorRGB color);

/** @brief Construye un color a partir de sus componentes (0 a 255). */
ColorRGB color_rgb(int r, int g, int b);

/** @brief Interpola linealmente entre dos colores (t = 0 devuelve a, t = 1 devuelve b). */
ColorRGB color_mezclar(ColorRGB a, ColorRGB b, double t);

/** @brief Ruido pseudoaleatorio determinista en [0, 1) a partir de tres enteros. */
double ruido_hash(int x, int y, int semilla);

#endif // RENDER_H
