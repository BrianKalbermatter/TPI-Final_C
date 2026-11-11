/**
 * @file visualizacion.c
 * @brief Centro de control: vista grafica del reingreso (izquierda) + terminal MCC (derecha).
 *
 * Coordenadas de la escena (locales a la vista grafica):
 *  - Horizontal: distancia recorrida (alcance) dividida por una escala fija,
 *    asi la trayectoria se puede dibujar en vivo sin conocer el final.
 *  - Vertical: altitud con escala de raiz cuadrada, para que las fases bajas
 *    (frenado y paracaidas) no queden aplastadas contra el oceano.
 */
#include "visualizacion.h"
#include "render.h"
#include "terminal_mcc.h"
#include "utilidades.h"
#include "validaciones.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define PI 3.14159265358979323846

// -- Disposicion de la pantalla
#define PANTALLA_MIN_COLUMNAS 100
#define PANTALLA_MIN_FILAS 30
#define PANEL_MIN_ANCHO 44
#define PANEL_MAX_ANCHO 72

// -- Composicion de la escena (coordenadas locales de la vista)
#define FILA_TECHO 2         /**< Fila correspondiente a 120 km. */
#define COLUMNA_INICIO 8     /**< Columna del punto de reingreso. */
#define MARGEN_DERECHO 6
#define HUD_X 7
#define HUD_ANCHO 34
#define HUD_ALTO 10

/** Alcance horizontal (m) que ocupa todo el ancho de la vista. */
#define ESCALA_ALCANCE_M 1000000.0

// -- Animacion
#define CUADROS_POR_TURNO 36
#define CUADROS_FINAL 60
#define CUADROS_ARRANQUE 6
#define MS_POR_CUADRO 30
#define MUESTRAS_ESTELA 120

/** Rapidez a partir de la cual se considera que los paracaidas estan abiertos (m/s). */
#define VELOCIDAD_CON_PARACAIDAS_MS 12.0

#define MAX_PUNTOS_TRAYECTORIA (MAX_HISTORIAL_TELEMETRIA + 1)

/** @brief Rectangulo de la pantalla (columna, fila, ancho, alto). */
typedef struct {
  int x;
  int y;
  int ancho;
  int alto;
} Rect;

/** @brief Punto de la trayectoria en coordenadas de la escena. */
typedef struct {
  double x;          /**< Alcance normalizado (0 = reingreso, 1 = borde derecho). */
  double altitud;    /**< Altitud (m). */
  double velocidad;  /**< Rapidez (m/s). */
  double carga_g;    /**< Carga G del turno. */
  double gamma;      /**< Angulo de guiado (grados). */
  int paracaidas;    /**< 1 si los paracaidas estan abiertos en este tramo. */
  const char *evento;
} PuntoEscena;

/** @brief Trayectoria completa: estado inicial + un punto por turno. */
typedef struct {
  PuntoEscena puntos[MAX_PUNTOS_TRAYECTORIA];
  int total;
} Trayectoria;

// -- Estado de la sesion grafica
static Lienzo lienzo;
static Trayectoria trayectoria;
static const MisionArtemis *mision_actual = NULL;
static Rect vista;           /**< Interior del panel grafico (coordenadas absolutas). */
static Rect panel;           /**< Panel de la terminal (con bordes). */
static int pantalla_columnas = 0;
static int pantalla_filas = 0;
static int cuadro = 0;
static double ms_ultimo_cuadro = 0.0;
static LOG_LEVELS nivel_log_previo = LOG_LEVEL_INFO;

// Que muestra la vista en este momento.
static int tramo_actual = -1;       /**< -1: nave detenida en el punto de reingreso. */
static double fraccion_actual = 1.0;
static int cuadro_desenlace = -1;   /**< >= 0 mientras se muestra el desenlace. */

// ============================================================================
// Disposicion de la pantalla y pintado recortado a la vista
// ============================================================================

static void actualizar_disposicion(void) {
  int columnas = 0;
  int filas = 0;
  render_tamano_terminal(&columnas, &filas);

  // Si el usuario cambio el tamano de la ventana se borra para no dejar restos.
  if (pantalla_columnas != 0 && (columnas != pantalla_columnas || filas != pantalla_filas)) {
    render_borrar_pantalla();
  }
  pantalla_columnas = columnas;
  pantalla_filas = filas;

  int ancho_panel = columnas * 36 / 100;
  if (ancho_panel < PANEL_MIN_ANCHO) ancho_panel = PANEL_MIN_ANCHO;
  if (ancho_panel > PANEL_MAX_ANCHO) ancho_panel = PANEL_MAX_ANCHO;

  panel.x = columnas - ancho_panel;
  panel.y = 0;
  panel.ancho = ancho_panel;
  panel.alto = filas;

  vista.x = 1;
  vista.y = 1;
  vista.ancho = panel.x - 2;
  vista.alto = filas - 2;
}

static int fila_mar(void) {
  return vista.alto - 4;
}

static int columna_fin(void) {
  return vista.ancho - MARGEN_DERECHO;
}

/* Todas las capas de la escena dibujan en coordenadas locales y se recortan a la vista,
 * para que el plasma o las particulas nunca invadan la terminal. */
static int dentro_de_vista(int x, int y) {
  return x >= 0 && y >= 0 && x < vista.ancho && y < vista.alto;
}

static void pintar(int x, int y, char simbolo, ColorRGB color) {
  if (dentro_de_vista(x, y)) lienzo_poner(&lienzo, vista.x + x, vista.y + y, simbolo, color);
}

static void pintar_luz(int x, int y, double luz, ColorRGB color) {
  if (dentro_de_vista(x, y)) lienzo_luz(&lienzo, vista.x + x, vista.y + y, luz, color);
}

static void pintar_texto(int x, int y, const char *texto, ColorRGB color) {
  for (int i = 0; texto[i] != '\0'; i++) pintar(x + i, y, texto[i], color);
}

// ============================================================================
// Conversion mundo -> pantalla
// ============================================================================

static double fila_de_altitud(double altitud) {
  double f = altitud / ALTITUD_REINGRESO_M;
  if (f < 0.0) f = 0.0;
  return (fila_mar() - 1) - sqrt(f) * ((fila_mar() - 1) - FILA_TECHO);
}

static double altitud_de_fila(int fila) {
  double f = (double)((fila_mar() - 1) - fila) / ((fila_mar() - 1) - FILA_TECHO);
  if (f < 0.0) f = 0.0;
  return f * f * ALTITUD_REINGRESO_M;
}

static double columna_de_x(double x) {
  return COLUMNA_INICIO + x * (columna_fin() - COLUMNA_INICIO);
}

static int redondear(double valor) {
  return (int)floor(valor + 0.5);
}

/** Intensidad del plasma (0 a 1): depende de la rapidez y solo existe en la alta atmosfera. */
static double calor_de(double velocidad, double altitud) {
  if (altitud < ALTITUD_FRENADO_M) return 0.0;
  double c = velocidad / VELOCIDAD_REINGRESO_MS;
  if (c > 1.0) c = 1.0;
  return sqrt(c);
}

// ============================================================================
// Trayectoria a partir del historial
// ============================================================================

static void construir_trayectoria(const MisionArtemis *mision, Trayectoria *t) {
  PuntoEscena *inicio = &t->puntos[0];
  inicio->x = 0.0;
  inicio->altitud = ALTITUD_REINGRESO_M;
  inicio->velocidad = VELOCIDAD_REINGRESO_MS;
  inicio->carga_g = 0.0;
  inicio->gamma = mision->angulo_reingreso;
  inicio->paracaidas = 0;
  inicio->evento = "Interfaz de reingreso a 120 km";

  // Alcance de cada turno: componente horizontal de la velocidad por la duracion del turno.
  double alcance = 0.0;
  for (int i = 0; i < mision->total_puntos; i++) {
    const PuntoTelemetria *p = &mision->historial[i];
    PuntoEscena *anterior = &t->puntos[i];
    PuntoEscena *actual = &t->puntos[i + 1];

    alcance += anterior->velocidad * cos(p->angulo_gamma * PI / 180.0) * DT_TURNO_S;
    actual->x = alcance / ESCALA_ALCANCE_M;
    actual->altitud = p->altitud;
    actual->velocidad = p->velocidad;
    actual->carga_g = p->aceleracion_g;
    actual->gamma = p->angulo_gamma;
    actual->paracaidas = (p->velocidad <= VELOCIDAD_CON_PARACAIDAS_MS);
    actual->evento = p->evento;
  }
  t->total = mision->total_puntos + 1;
}

static PuntoEscena interpolar(const Trayectoria *t, int tramo, double fraccion) {
  const PuntoEscena *a = &t->puntos[tramo];
  const PuntoEscena *b = &t->puntos[tramo + 1];
  PuntoEscena p = *b; // datos del turno (evento, carga G, gamma, paracaidas)
  p.x = a->x + (b->x - a->x) * fraccion;
  p.altitud = a->altitud + (b->altitud - a->altitud) * fraccion;
  p.velocidad = a->velocidad + (b->velocidad - a->velocidad) * fraccion;
  return p;
}

/** Posicion actual de la nave segun lo que muestra la vista. */
static PuntoEscena punto_actual(void) {
  if (tramo_actual < 0) return trayectoria.puntos[0];
  return interpolar(&trayectoria, tramo_actual, fraccion_actual);
}

/** Direccion de movimiento en espacio visual (normalizada) para orientar el plasma. */
static void direccion_tramo(int tramo, double *ux, double *uy) {
  *ux = 0.95; // antes del primer turno: direccion nominal de descenso
  *uy = 0.3;
  if (tramo < 0) return;

  const PuntoEscena *a = &trayectoria.puntos[tramo];
  const PuntoEscena *b = &trayectoria.puntos[tramo + 1];
  double dx = columna_de_x(b->x) - columna_de_x(a->x);
  double dy = (fila_de_altitud(b->altitud) - fila_de_altitud(a->altitud)) * 2.0;
  double largo = sqrt(dx * dx + dy * dy);
  if (largo < 1e-9) {
    *ux = 0.0;
    *uy = 1.0;
    return;
  }
  *ux = dx / largo;
  *uy = dy / largo;
}

// ============================================================================
// Capas de la escena
// ============================================================================

static void dibujar_fondo(void) {
  const ColorRGB espacio = color_rgb(10, 10, 28);
  const ColorRGB azul_alto = color_rgb(30, 70, 150);
  const ColorRGB horizonte = color_rgb(80, 140, 210);
  const ColorRGB estrella = color_rgb(210, 220, 255);
  const ColorRGB nube = color_rgb(160, 170, 195);
  const ColorRGB mar_hondo = color_rgb(8, 35, 85);
  const ColorRGB mar_cresta = color_rgb(60, 160, 210);

  for (int y = 0; y < vista.alto; y++) {
    for (int x = 0; x < vista.ancho; x++) {
      double grano = ruido_hash(x, y, 7);

      if (y >= fila_mar()) {
        // Oceano: olas que se desplazan con el tiempo.
        double ola = 0.5 + 0.5 * sin(x * 0.33 + cuadro * 0.18 + y * 1.7) *
                               cos(x * 0.07 - cuadro * 0.05);
        double luz = 0.18 + 0.45 * ola + grano * 0.1;
        if (y == fila_mar()) luz += 0.12; // linea de horizonte
        pintar_luz(x, y, luz, color_mezclar(mar_hondo, mar_cresta, ola));
        continue;
      }

      // Cielo: degradado del espacio negro a la atmosfera azul.
      double f = (double)y / fila_mar();
      ColorRGB color = color_mezclar(espacio, azul_alto, f * f);
      color = color_mezclar(color, horizonte, pow(f, 6.0));
      double luz = 0.03 + 0.22 * f * f * f + grano * 0.06;

      // Estrellas titilantes en la parte alta.
      if (f < 0.5 && ruido_hash(x, y, 3) < 0.025) {
        luz = 0.35 + 0.35 * (0.5 + 0.5 * sin(cuadro * 0.2 + grano * 20.0));
        color = estrella;
      }

      // Capa de nubes en la baja atmosfera.
      double altitud = altitud_de_fila(y);
      if (altitud > 1200.0 && altitud < 9000.0) {
        double n = 0.5 * sin(x * 0.19 + cuadro * 0.03 + y * 0.7) +
                   0.5 * sin(x * 0.071 - y * 1.3 + 1.7) + (grano - 0.5) * 0.4;
        if (n > 0.35) {
          luz = 0.22 + (n - 0.35) * 0.9;
          color = nube;
        }
      }

      pintar_luz(x, y, luz, color);
    }
  }
}

static void dibujar_eje_altitud(void) {
  static const double marcas[] = {120000.0, 60000.0, 40000.0, 10000.0, 2500.0};
  static const char *etiquetas[] = {"120km", " 60km", " 40km", " 10km", "2.5km"};
  const ColorRGB gris = color_rgb(120, 125, 140);

  for (int i = 0; i < (int)(sizeof(marcas) / sizeof(marcas[0])); i++) {
    int fila = redondear(fila_de_altitud(marcas[i]));
    pintar_texto(0, fila, etiquetas[i], gris);
    pintar(5, fila, '-', gris);
  }
}

/** Dibuja la estela desde el reingreso hasta (tramo, fraccion). */
static void dibujar_estela(int tramo, double fraccion) {
  const ColorRGB ceniza = color_rgb(110, 110, 125);
  const ColorRGB naranja = color_rgb(255, 130, 40);

  for (int i = 0; i <= tramo && i < trayectoria.total - 1; i++) {
    double hasta = (i == tramo) ? fraccion : 1.0;
    for (int k = 0; k <= MUESTRAS_ESTELA; k++) {
      double s = (double)k / MUESTRAS_ESTELA;
      if (s > hasta) break;
      PuntoEscena p = interpolar(&trayectoria, i, s);
      double calor = calor_de(p.velocidad, p.altitud);
      pintar_luz(redondear(columna_de_x(p.x)), redondear(fila_de_altitud(p.altitud)),
                 0.3 + 0.4 * calor, color_mezclar(ceniza, naranja, calor));
    }
  }
}

static void dibujar_paracaidas(int cx, int cy) {
  static const char *forma[] = {" ,-~~~-, ", "/_/_|_\\_\\", " \\  |  / "};
  const ColorRGB naranja = color_rgb(255, 140, 40);
  const ColorRGB blanco = color_rgb(235, 235, 240);
  const ColorRGB cuerdas = color_rgb(150, 150, 160);

  for (int fila = 0; fila < 3; fila++) {
    for (int col = 0; forma[fila][col] != '\0'; col++) {
      char simbolo = forma[fila][col];
      if (simbolo == ' ') continue;
      ColorRGB color = (fila == 2) ? cuerdas : ((col % 2 == 0) ? naranja : blanco);
      pintar(cx - 4 + col, cy - 6 + fila, simbolo, color);
    }
  }
}

/**
 * Sprite de la capsula Orion (7 x 4 celdas). La ultima fila es el escudo termico,
 * que apunta hacia abajo (en el reingreso la nave cae "de escudo").
 * (cx, cy) corresponde al centro del escudo.
 */
static void dibujar_sprite_orion(int cx, int cy, double calor) {
  static const char *forma[] = {
      "  /^\\  ",
      " /:o:\\ ",
      "/:::::\\",
      "\\=====/",
  };
  const ColorRGB casco_luz = color_rgb(225, 230, 240);
  const ColorRGB casco_sombra = color_rgb(120, 125, 140);
  const ColorRGB ventanilla = color_rgb(120, 220, 255);
  const ColorRGB escudo_frio = color_rgb(110, 95, 80);
  const ColorRGB escudo_caliente = color_rgb(255, 150, 40);
  const ColorRGB escudo_blanco = color_rgb(255, 245, 210);

  for (int fila = 0; fila < 4; fila++) {
    for (int col = 0; forma[fila][col] != '\0'; col++) {
      char simbolo = forma[fila][col];
      if (simbolo == ' ') continue;

      ColorRGB color;
      if (fila == 3) {
        // Escudo termico: incandescente segun el calor, con parpadeo.
        double parpadeo = 0.85 + 0.15 * ruido_hash(col, cuadro, 41);
        double t = calor * parpadeo;
        color = (t < 0.6) ? color_mezclar(escudo_frio, escudo_caliente, t / 0.6)
                          : color_mezclar(escudo_caliente, escudo_blanco, (t - 0.6) / 0.4);
      } else if (simbolo == 'o') {
        color = ventanilla;
      } else {
        // Iluminacion lateral: el lado izquierdo recibe luz, el derecho queda en sombra.
        color = color_mezclar(casco_luz, casco_sombra, (double)col / 6.0);
      }
      pintar(cx - 3 + col, cy - 3 + fila, simbolo, color);
    }
  }
}

/**
 * Capsula con plasma de reingreso: particulas que se desprenden hacia atras
 * (sentido contrario al movimiento), mas calientes cuanto mas cerca de la nave,
 * y una onda de choque en arco frente al escudo termico.
 */
static void dibujar_capsula(double cx, double cy, double ux, double uy, double calor, int paracaidas) {
  const ColorRGB blanco = color_rgb(255, 250, 230);
  const ColorRGB amarillo = color_rgb(255, 235, 170);
  const ColorRGB naranja = color_rgb(255, 120, 30);
  const ColorRGB rojo = color_rgb(190, 30, 20);

  if (calor > 0.05) {
    // Estela de plasma: nace en los bordes de la capsula y se abre hacia atras.
    for (int k = 0; k < 70; k++) {
      double r = ruido_hash(k, cuadro, 11);
      double largo = 2.0 + r * (3.0 + 12.0 * calor);
      double lateral = (ruido_hash(k, cuadro, 12) - 0.5) * (3.0 + 4.0 * r);
      // Espacio visual -> celdas: las celdas son el doble de altas que de anchas.
      double px = cx - ux * largo - uy * lateral;
      double py = cy + (-uy * largo + ux * lateral) * 0.5;
      ColorRGB color = (r < 0.3) ? color_mezclar(amarillo, naranja, r / 0.3)
                                 : color_mezclar(naranja, rojo, (r - 0.3) / 0.7);
      pintar_luz(redondear(px), redondear(py), calor * (1.0 - r) * 0.9 + 0.1, color);
    }

    // Onda de choque: arco brillante delante del escudo, perpendicular al movimiento.
    for (int t = -4; t <= 4; t++) {
      double curva = 2.0 - 0.12 * t * t; // el centro del arco va mas adelante
      double px = cx + ux * curva - uy * t;
      double py = cy + (uy * curva + ux * t) * 0.5;
      double luz = calor * (1.0 - fabs((double)t) / 5.0);
      ColorRGB color = color_mezclar(amarillo, blanco, 1.0 - fabs((double)t) / 4.0);
      pintar_luz(redondear(px), redondear(py), luz, color);
    }
  }

  int x = redondear(cx);
  int y = redondear(cy);
  if (paracaidas) dibujar_paracaidas(x, y);
  dibujar_sprite_orion(x, y, calor);
}

static const char *nombre_fase(double altitud) {
  if (altitud > ALTITUD_FIN_ENTRADA_M) return "ENTRADA";
  if (altitud > ALTITUD_PARACAIDAS_M) return "FRENADO";
  if (altitud > 0.0) return "PARACAIDAS";
  return "AMERIZAJE";
}

static ColorRGB color_segun_nivel(double fraccion) {
  if (fraccion < 0.5) return color_rgb(90, 220, 120);
  if (fraccion < 0.8) return color_rgb(240, 200, 60);
  return color_rgb(240, 70, 50);
}

static void dibujar_hud(const PuntoEscena *p, double tiempo_s, int turno) {
  const ColorRGB borde = color_rgb(90, 95, 110);
  const ColorRGB titulo = color_rgb(230, 230, 240);
  const ColorRGB etiqueta = color_rgb(150, 155, 170);
  const ColorRGB valor = color_rgb(220, 220, 230);
  const ColorRGB verde = color_rgb(120, 220, 120);
  char texto[64];

  int hud_y = fila_mar() - HUD_ALTO - 2;
  lienzo_caja(&lienzo, vista.x + HUD_X, vista.y + hud_y, HUD_ANCHO, HUD_ALTO, borde);
  int x = vista.x + HUD_X + 2;
  int y = vista.y + hud_y + 1;

  snprintf(texto, sizeof texto, "ORION MPCV      T+%5.0f s", tiempo_s);
  lienzo_texto(&lienzo, x, y++, texto, titulo);

  double f_alt = p->altitud / ALTITUD_REINGRESO_M;
  lienzo_texto(&lienzo, x, y, "ALT", etiqueta);
  lienzo_barra(&lienzo, x + 4, y, 16, f_alt, color_rgb(90, 180, 240));
  snprintf(texto, sizeof texto, "%6.0f m", p->altitud);
  lienzo_texto(&lienzo, x + 21, y++, texto, valor);

  double f_vel = p->velocidad / VELOCIDAD_REINGRESO_MS;
  lienzo_texto(&lienzo, x, y, "VEL", etiqueta);
  lienzo_barra(&lienzo, x + 4, y, 16, f_vel,
               color_mezclar(color_rgb(90, 180, 240), color_rgb(255, 130, 40), f_vel));
  snprintf(texto, sizeof texto, "%5.0f m/s", p->velocidad);
  lienzo_texto(&lienzo, x + 21, y++, texto, valor);

  double f_g = p->carga_g / CARGA_G_MAXIMA;
  lienzo_texto(&lienzo, x, y, "G", etiqueta);
  lienzo_barra(&lienzo, x + 4, y, 16, f_g, color_segun_nivel(f_g));
  snprintf(texto, sizeof texto, "%5.1f g", p->carga_g);
  lienzo_texto(&lienzo, x + 21, y++, texto, valor);

  double escudo = mision_actual->integridad_escudo;
  lienzo_texto(&lienzo, x, y, "ESC", etiqueta);
  lienzo_barra(&lienzo, x + 4, y, 16, escudo / 100.0, color_segun_nivel(1.0 - escudo / 100.0));
  snprintf(texto, sizeof texto, "%5.0f %%", escudo);
  lienzo_texto(&lienzo, x + 21, y++, texto, valor);

  snprintf(texto, sizeof texto, "GAMMA %4.1f deg  FASE %s", p->gamma, nombre_fase(p->altitud));
  lienzo_texto(&lienzo, x, y++, texto, valor);

  snprintf(texto, sizeof texto, "TURNO %d", turno);
  lienzo_texto(&lienzo, x, y++, texto, etiqueta);

  snprintf(texto, sizeof texto, "FPS %d  frame %.1f ms", 1000 / MS_POR_CUADRO, ms_ultimo_cuadro);
  lienzo_texto(&lienzo, x, y, texto, verde);
}

// ============================================================================
// Desenlace de la mision
// ============================================================================

static void dibujar_salpicadura(double cx, double progreso, int ancho, int alto) {
  const ColorRGB espuma = color_rgb(220, 240, 255);
  for (int j = 0; j < 18 + ancho; j++) {
    double h = ruido_hash(j, 1, 21);
    double altura = sin(progreso * PI) * h * alto;
    double px = cx + (ruido_hash(j, 2, 22) - 0.5) * ancho * (0.3 + progreso);
    double luz = 0.4 + 0.5 * ruido_hash(j, cuadro, 23);
    pintar_luz(redondear(px), redondear(fila_mar() - 1 - altura), luz, espuma);
  }
}

static void dibujar_explosion(double cx, double cy, double progreso) {
  const ColorRGB amarillo = color_rgb(255, 230, 140);
  const ColorRGB rojo = color_rgb(220, 50, 20);
  const ColorRGB humo = color_rgb(90, 90, 100);
  for (int j = 0; j < 70; j++) {
    double angulo = ruido_hash(j, 3, 31) * 2.0 * PI;
    double distancia = progreso * (2.0 + ruido_hash(j, 4, 32) * 12.0);
    double px = cx + cos(angulo) * distancia;
    double py = cy + sin(angulo) * distancia * 0.5;
    ColorRGB color = (progreso < 0.4) ? color_mezclar(amarillo, rojo, progreso / 0.4)
                                      : color_mezclar(rojo, humo, (progreso - 0.4) / 0.6);
    pintar_luz(redondear(px), redondear(py), 1.0 - progreso * 0.8, color);
  }
}

static const char *titulo_resultado(ResultadoMision resultado) {
  switch (resultado) {
    case MISION_EXITOSA: return "AMERIZAJE EXITOSO";
    case MISION_FALLO_IMPACTO: return "IMPACTO CRITICO";
    case MISION_FALLO_TERMICO: return "FALLO TERMICO";
    case MISION_FALLO_GS: return "SOBRECARGA G";
    case MISION_FALLO_PARACAIDAS: return "FALLO DE PARACAIDAS";
    case MISION_FALLO_REBOTE: return "REBOTE ATMOSFERICO";
    default: return "MISION FINALIZADA";
  }
}

static void dibujar_cartel(void) {
  const char *titulo = titulo_resultado(mision_actual->resultado);
  const char *causa = mision_actual->causa_fallo;
  ColorRGB color = color_rgb(240, 70, 50);
  if (mision_actual->resultado == MISION_EXITOSA) color = color_rgb(110, 230, 120);
  if (mision_actual->resultado == MISION_FALLO_REBOTE) color = color_rgb(240, 200, 60);

  int ancho = (int)strlen(causa);
  if ((int)strlen(titulo) > ancho) ancho = (int)strlen(titulo);
  ancho += 6;
  if (ancho > vista.ancho - 2) ancho = vista.ancho - 2;

  int x = vista.x + (vista.ancho - ancho) / 2;
  int y = vista.y + vista.alto / 2 - 4;
  lienzo_caja(&lienzo, x, y, ancho, 5, color);
  lienzo_texto(&lienzo, x + (ancho - (int)strlen(titulo)) / 2, y + 1, titulo, color);
  lienzo_texto(&lienzo, x + 3, y + 3, causa, color_rgb(220, 220, 230));
}

static void dibujar_desenlace(void) {
  const PuntoEscena *ultimo = &trayectoria.puntos[trayectoria.total - 1];
  double progreso = (double)cuadro_desenlace / (CUADROS_FINAL - 1);
  if (progreso > 1.0) progreso = 1.0;
  double cx = columna_de_x(ultimo->x);
  double cy = fila_de_altitud(ultimo->altitud);
  double ux = 0.0;
  double uy = 1.0;
  direccion_tramo(trayectoria.total - 2, &ux, &uy);

  switch (mision_actual->resultado) {
    case MISION_EXITOSA: {
      dibujar_salpicadura(cx, progreso, 8, 3);
      double vaiven = sin(cuadro * 0.3) * 0.4; // la capsula flota sobre las olas
      dibujar_capsula(cx, fila_mar() - 1 + vaiven, ux, uy, 0.0, progreso < 0.4);
      break;
    }
    case MISION_FALLO_IMPACTO:
      dibujar_salpicadura(cx, progreso, 18, 7);
      break;
    case MISION_FALLO_REBOTE: {
      // La capsula sigue de largo y escapa de la atmosfera.
      double x = ultimo->x + progreso * 0.5;
      double altitud = ALTITUD_REINGRESO_M * (1.0 + progreso * 0.6);
      dibujar_capsula(columna_de_x(x), fila_de_altitud(altitud), 0.9, -0.4, 0.3, 0);
      break;
    }
    default:
      dibujar_explosion(cx, cy, progreso);
      break;
  }
}

// ============================================================================
// Composicion del cuadro completo
// ============================================================================

static void dibujar_marco_vista(void) {
  const ColorRGB borde = color_rgb(70, 90, 130);
  lienzo_caja(&lienzo, 0, 0, panel.x, pantalla_filas, borde);
  lienzo_texto(&lienzo, 2, 0, "[ VISTA TELEMETRICA // REINGRESO ORION - ARTEMIS ]", color_rgb(120, 160, 230));
}

static void dibujar_escena(void) {
  int tramo_estela = tramo_actual;
  double fraccion_estela = fraccion_actual;
  PuntoEscena p = punto_actual();
  double met = (tramo_actual < 0) ? 0.0 : (tramo_actual + fraccion_actual) * DT_TURNO_S;

  dibujar_fondo();
  dibujar_eje_altitud();
  if (tramo_estela >= 0) dibujar_estela(tramo_estela, fraccion_estela);

  if (cuadro_desenlace >= 0) {
    dibujar_desenlace();
  } else {
    double ux = 0.0;
    double uy = 1.0;
    direccion_tramo(tramo_actual, &ux, &uy);
    dibujar_capsula(columna_de_x(p.x), fila_de_altitud(p.altitud), ux, uy,
                    calor_de(p.velocidad, p.altitud), p.paracaidas);
  }

  dibujar_hud(&p, met, tramo_actual + 1);

  char texto[128];
  snprintf(texto, sizeof texto, " Evento: %s", p.evento);
  pintar_texto(0, 0, texto, color_rgb(230, 230, 240));

  if (cuadro_desenlace > CUADROS_FINAL / 3) dibujar_cartel();
}

/**
 * Compone y muestra un cuadro completo: vista grafica + terminal.
 * Devuelve en (cursor_x, cursor_y) donde el operador escribe su respuesta.
 */
static void presentar_cuadro(const char *pregunta, int *cursor_x, int *cursor_y) {
  clock_t inicio = clock();
  double met = (tramo_actual < 0) ? 0.0 : (tramo_actual + fraccion_actual) * DT_TURNO_S;

  actualizar_disposicion();
  lienzo_limpiar(&lienzo, pantalla_columnas, pantalla_filas);
  dibujar_marco_vista();
  dibujar_escena();
  terminal_dibujar(&lienzo, panel.x, panel.y, panel.ancho, panel.alto, met, pregunta, cuadro,
                   cursor_x, cursor_y);
  render_presentar(&lienzo);

  ms_ultimo_cuadro = (double)(clock() - inicio) * 1000.0 / CLOCKS_PER_SEC;
  cuadro++;
}

static void presentar_y_esperar(void) {
  int x = 0;
  int y = 0;
  presentar_cuadro(NULL, &x, &y);
  render_esperar_ms(MS_POR_CUADRO);
}

// ============================================================================
// Entrada de datos desde la terminal del panel
// ============================================================================

static void cerrar_sesion(void) {
  render_finalizar();
  log_set_min_level(nivel_log_previo);
}

static void descartar_resto_de_linea(void) {
  int c;
  while ((c = getchar()) != '\n' && c != EOF) {
    // vaciar hasta salto de linea
  }
}

/** Muestra la pregunta en la terminal, ubica el cursor y lee una linea. */
static void leer_linea(const char *pregunta, char *buffer, int tam) {
  for (;;) {
    int cursor_x = 0;
    int cursor_y = 0;
    presentar_cuadro(pregunta, &cursor_x, &cursor_y);
    render_mostrar_cursor(cursor_x, cursor_y);

    if (fgets(buffer, tam, stdin) == NULL) {
      cerrar_sesion();
      printf("\nFin de la entrada. Saliendo del programa.\n");
      exit(EXIT_FAILURE);
    }
    render_ocultar_cursor();

    if (strchr(buffer, '\n') == NULL && !feof(stdin)) {
      descartar_resto_de_linea();
      terminal_escribir(TONO_ERROR, "[!] Error: la entrada es demasiado larga.");
      continue;
    }

    buffer[strcspn(buffer, "\r\n")] = '\0';
    terminal_escribir(TONO_ENTRADA, TERMINAL_PROMPT "%s", buffer);
    return;
  }
}

double visualizacion_leer_real(const char *pregunta, double minimo, double maximo) {
  char buffer[64];

  for (;;) {
    leer_linea(pregunta, buffer, (int)sizeof buffer);

    if (!es_real(buffer)) {
      terminal_escribir(TONO_ERROR, "[!] Error: ingrese un numero decimal valido.");
      continue;
    }
    for (int i = 0; buffer[i] != '\0'; i++) {
      if (buffer[i] == ',') buffer[i] = '.'; // se acepta coma decimal
    }
    double valor = strtod(buffer, NULL);
    if (valor < minimo || valor > maximo) {
      terminal_escribir(TONO_ERROR, "[!] Error: el valor debe estar entre %.1f y %.1f.", minimo, maximo);
      continue;
    }
    return valor;
  }
}

int visualizacion_leer_confirmacion(const char *pregunta) {
  char buffer[16];

  for (;;) {
    leer_linea(pregunta, buffer, (int)sizeof buffer);
    int respuesta = es_confirmacion(buffer);
    if (respuesta != -1) return respuesta;
    terminal_escribir(TONO_ERROR, "[!] Error: responda S o N.");
  }
}

// ============================================================================
// API publica
// ============================================================================

static TonoTerminal tono_de_evento(const char *evento) {
  if (strstr(evento, "EXITOSO") != NULL || strstr(evento, "desplegados") != NULL) return TONO_OK;
  if (strstr(evento, "ALERTA") != NULL || strstr(evento, "rasgados") != NULL) return TONO_ALERTA;
  if (strstr(evento, "CRITICO") != NULL || strstr(evento, "CATACLISMO") != NULL) return TONO_ERROR;
  return TONO_NORMAL;
}

int visualizacion_iniciar(const MisionArtemis *mision) {
  if (mision == NULL) return 0;

  int columnas = 0;
  int filas = 0;
  render_tamano_terminal(&columnas, &filas);
  if (columnas < PANTALLA_MIN_COLUMNAS || filas < PANTALLA_MIN_FILAS) {
    printf("[!] La ventana mide %dx%d y la vista grafica necesita al menos %dx%d.\n", columnas, filas,
           PANTALLA_MIN_COLUMNAS, PANTALLA_MIN_FILAS);
    printf("    Agrande la ventana para verla. Se continua en modo texto.\n\n");
    return 0;
  }
  if (!render_iniciar()) {
    printf("[!] La consola no admite graficos ANSI. Se continua en modo texto.\n\n");
    return 0;
  }

  // Los registros (LOG_*) escriben directo en la consola y romperian la pantalla dividida.
  nivel_log_previo = log_get_min_level();
  log_set_min_level(LOG_LEVEL_SILENCE);

  mision_actual = mision;
  pantalla_columnas = 0;
  pantalla_filas = 0;
  cuadro = 0;
  tramo_actual = -1;
  fraccion_actual = 1.0;
  cuadro_desenlace = -1;
  construir_trayectoria(mision, &trayectoria);
  terminal_reiniciar();

  // Secuencia de arranque: cada linea aparece como en una terminal real.
  terminal_escribir(TONO_OK, "ARTEMIS MISSION CONTROL - ORION GNC v4.2");
  terminal_escribir(TONO_TENUE, "Iniciando secuencia de reingreso...");
  terminal_escribir(TONO_NORMAL, "[ OK ] Enlace S-band con Orion establecido");
  terminal_escribir(TONO_NORMAL, "[ OK ] Telemetria: %.0f kg | %.0f m | %.0f m/s", mision->masa,
                    mision->altitud, mision->velocidad);
  terminal_escribir(TONO_NORMAL, "[ OK ] Escudo termico al %.0f%%", mision->integridad_escudo);
  terminal_escribir(TONO_INFO, "OBJETIVO: mantener gamma entre %.1f y %.1f grados.", ANGULO_REINGRESO_MIN,
                    ANGULO_REINGRESO_MAX);
  terminal_escribir(TONO_INFO, "Abrir los paracaidas recien a %.0f m (v <= %.0f m/s).", ALTITUD_PARACAIDAS_M,
                    VELOCIDAD_MAX_PARACAIDAS_MS);
  terminal_escribir(TONO_TENUE, "-----------------------------------------");

  for (int k = 0; k < CUADROS_ARRANQUE; k++) presentar_y_esperar();
  return 1;
}

void visualizacion_animar_turno(void) {
  construir_trayectoria(mision_actual, &trayectoria);
  int tramo = trayectoria.total - 2;
  const PuntoEscena *p = &trayectoria.puntos[tramo + 1];

  terminal_escribir(TONO_INFO, "[T+%04.0f] TURNO %d  gamma %.1f deg", (tramo + 1) * DT_TURNO_S, tramo + 1,
                    p->gamma);

  tramo_actual = tramo;
  for (int k = 0; k < CUADROS_POR_TURNO; k++) {
    fraccion_actual = (double)(k + 1) / CUADROS_POR_TURNO;
    presentar_y_esperar();
  }

  terminal_escribir(TONO_NORMAL, "  ALT %.0f m | VEL %.1f m/s | %.1f g", p->altitud, p->velocidad, p->carga_g);
  terminal_escribir(tono_de_evento(p->evento), "  > %s", p->evento);
  terminal_escribir(TONO_TENUE, "  Escudo termico: %.0f%%", mision_actual->integridad_escudo);
}

void visualizacion_finalizar(void) {
  int exito = (mision_actual->resultado == MISION_EXITOSA);
  terminal_escribir(TONO_TENUE, "-----------------------------------------");
  terminal_escribir(exito ? TONO_OK : TONO_ERROR, "=== %s ===", titulo_resultado(mision_actual->resultado));
  terminal_escribir(exito ? TONO_OK : TONO_ERROR, "%s", mision_actual->causa_fallo);

  for (cuadro_desenlace = 0; cuadro_desenlace < CUADROS_FINAL; cuadro_desenlace++) {
    presentar_y_esperar();
  }

  char buffer[16];
  leer_linea("Presione ENTER para ver el reporte.", buffer, (int)sizeof buffer);
  cerrar_sesion();
}
