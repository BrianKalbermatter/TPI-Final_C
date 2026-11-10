#include <curses.h>
#include <tui.h>
#include <simulador.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

static unsigned int semilla_universo = 1337;

// Matriz Bayer 4x4 normalizada para tramado ordenado continuo (elimina cualquier línea de corte)
static const double bayer4x4[4][4] = {
    {  0.0/16.0,  8.0/16.0,  2.0/16.0, 10.0/16.0 },
    { 12.0/16.0,  4.0/16.0, 14.0/16.0,  6.0/16.0 },
    {  3.0/16.0, 11.0/16.0,  1.0/16.0,  9.0/16.0 },
    { 15.0/16.0,  7.0/16.0, 13.0/16.0,  5.0/16.0 }
};

// -- Parámetros de la escena (altitudes en metros)
#define H_LIMITE_ESPACIO   80000.0  // Límite superior de la atmósfera visible (línea de airglow)
#define H_CIELO_PLENO      30000.0  // Por debajo: fondo celeste sólido
#define H_NUBES_MAX        15000.0  // Techo de la capa de nubes
#define H_NUBES_MIN          800.0  // Base de la capa de nubes
#define H_INICIO_OCEANO     2500.0  // Altitud a la que el océano asoma por el borde inferior
#define ESCALA_BAJA          300.0  // Metros por fila en baja altitud
#define NIVEL_MAXIMO          17    // Niveles de densidad: 0 = espacio, 17 = celeste sólido

// Identificadores semánticos de los pares
typedef enum {
    PAR_DEFECTO = 0,     // Reservado por Curses
    PAR_ESTRELLAS,       // Blanco sobre negro (Espacio)
    PAR_ATMOS_CELESTE,   // Celeste sobre negro (Fase 1: puntos celestes sobre fondo negro)
    PAR_ATMOS_MAXIMA,    // Negro sobre celeste (Fase 2: puntos negros sobre celeste / cielo pleno)
    PAR_AIRGLOW,         // Verde sobre negro (Luminiscencia en el contorno terrestre)
    PAR_NUBE,            // Blanco sobre celeste (Nubes cúmulo)
    PAR_BRUMA,           // Blanco sobre celeste (Línea de bruma del horizonte marino)
    PAR_MAR_LEJANO,      // Azul sobre azul (con A_BOLD: azul claro sobre azul oscuro)
    PAR_MAR,             // Cian sobre azul (Olas)
    PAR_ESPUMA,          // Blanco sobre azul (Crestas de espuma y ondas de amerizaje)
    PAR_SOL,             // Amarillo sobre azul (Reflejo del sol en el agua)
    PAR_PLASMA_FUEGO,    // Rojo sobre negro (Reservado para efectos térmicos de la cápsula)
    PAR_ALERTA_CRITICA,  // Blanco sobre rojo (Emergencias / Gs altas)
    PAR_EXITO_VERDE,     // Verde sobre negro (Misión cumplida)
    PAR_AMARILLO_NEGRO,  // Amarillo sobre negro (Detalles cápsula, airbags, escudo)
    PAR_CYAN_NEGRO,      // Cian sobre negro (Ventanas de cabina, vectores)
    PAR_AIRBAG_CIELO,    // Amarillo sobre cian (Airbags sobre el cielo diurno)
    PAR_BEACON_CIELO,    // Rojo sobre cian (Baliza activa sobre el cielo diurno)
    PAR_TEXTO_CIELO      // Negro sobre cian (Estructuras y baliza sobre el cielo diurno)
} ParesColor;

// Función de dispersión pseudoaleatoria espacial (x, y) de alta calidad
static inline unsigned int hash_espacial(int x, int y, unsigned int seed) {
    unsigned int h = (unsigned int)(x * 374761393u ^ y * 668265263u ^ seed * 1274126177u);
    h = (h ^ (h >> 13)) * 0x5bd1e995u;
    h ^= h >> 15;
    return h;
}

// Interpolación suave (smoothstep) con recorte a [0, 1]
static inline double suavizar(double t) {
    if (t <= 0.0) return 0.0;
    if (t >= 1.0) return 1.0;
    return t * t * (3.0 - 2.0 * t);
}

void configurar_paleta(void) {
    if (!has_colors()) return;

    start_color();
    use_default_colors();

    // init_pair(ID, TEXTO, FONDO)
    init_pair(PAR_ESTRELLAS,      COLOR_WHITE,   COLOR_BLACK);
    init_pair(PAR_ATMOS_CELESTE,  COLOR_CYAN,    COLOR_BLACK);
    init_pair(PAR_ATMOS_MAXIMA,   COLOR_BLACK,   COLOR_CYAN);
    init_pair(PAR_AIRGLOW,        COLOR_GREEN,   COLOR_BLACK);
    init_pair(PAR_NUBE,           COLOR_WHITE,   COLOR_CYAN);
    init_pair(PAR_BRUMA,          COLOR_WHITE,   COLOR_CYAN);
    init_pair(PAR_MAR_LEJANO,     COLOR_BLUE,    COLOR_BLUE);
    init_pair(PAR_MAR,            COLOR_CYAN,    COLOR_BLUE);
    init_pair(PAR_ESPUMA,         COLOR_WHITE,   COLOR_BLUE);
    init_pair(PAR_SOL,            COLOR_YELLOW,  COLOR_BLUE);
    init_pair(PAR_PLASMA_FUEGO,   COLOR_RED,     COLOR_BLACK);
    init_pair(PAR_ALERTA_CRITICA, COLOR_WHITE,   COLOR_RED);
    init_pair(PAR_EXITO_VERDE,    COLOR_GREEN,   COLOR_BLACK);
    init_pair(PAR_AMARILLO_NEGRO, COLOR_YELLOW,  COLOR_BLACK);
    init_pair(PAR_CYAN_NEGRO,     COLOR_CYAN,    COLOR_BLACK);
    init_pair(PAR_AIRBAG_CIELO,   COLOR_YELLOW,  COLOR_CYAN);
    init_pair(PAR_BEACON_CIELO,   COLOR_RED,     COLOR_CYAN);
    init_pair(PAR_TEXTO_CIELO,    COLOR_BLACK,   COLOR_CYAN);
}

void tui_iniciar(void) {
  initscr();
  cbreak();
  noecho();
  keypad(stdscr, TRUE);
  curs_set(0);

  configurar_paleta();
  semilla_universo = (unsigned int)time(NULL);
}

void tui_liberar(void) {
  endwin();
}

static int g_modo_depuracion = 0;

void tui_set_modo_depuracion(int activo) {
  g_modo_depuracion = activo ? 1 : 0;
}

int tui_get_modo_depuracion(void) {
  return g_modo_depuracion;
}

int menu_principal(void) {
  const char *opciones_normal[] = {
    "1. Iniciar Simulación Interactiva (Modo Piloto)",
    "2. Análisis Teórico (Preguntas Artemis TP)",
    "3. Ver Reporte de Telemetría",
    "4. Manual de Vuelo y Fórmulas Físicas",
    "5. Integrantes del Equipo",
    "6. Salir de la Misión"
  };
  const OpcionMenu mapeo_normal[] = {
    MENU_SIMULACION_INTERACTIVA,
    MENU_ANALISIS_TEORICO,
    MENU_REPORTE_TELEMETRIA,
    MENU_MANUAL_VUELO,
    MENU_INTEGRANTES,
    MENU_SALIR
  };

  const char *opciones_debug[] = {
    "1. Iniciar Simulación Interactiva (Modo Piloto)",
    "2. Análisis Teórico (Preguntas Artemis TP)",
    "3. Ver Reporte de Telemetría",
    "4. Manual de Vuelo y Fórmulas Físicas",
    "5. Integrantes del Equipo",
    "6. [TEST] Render de Fondo por Altura",
    "7. [TEST] Sprites de la Cápsula Orion",
    "8. Salir de la Misión"
  };
  const OpcionMenu mapeo_debug[] = {
    MENU_SIMULACION_INTERACTIVA,
    MENU_ANALISIS_TEORICO,
    MENU_REPORTE_TELEMETRIA,
    MENU_MANUAL_VUELO,
    MENU_INTEGRANTES,
    MENU_TEST_FONDO,
    MENU_TEST_CAPSULA,
    MENU_SALIR
  };

  int seleccion = 0;
  int tecla;

  int ancho = 68;
  int alto = g_modo_depuracion ? 14 : 12;
  int inicio_y = (LINES - alto) / 2;
  int inicio_x = (COLS - ancho) / 2;

  WINDOW *menu_win = newwin(alto, ancho, inicio_y, inicio_x);
  keypad(menu_win, TRUE);

  while (1) {
    int total_opciones = g_modo_depuracion ? 8 : 6;
    const char **opciones = g_modo_depuracion ? opciones_debug : opciones_normal;
    const OpcionMenu *mapeo = g_modo_depuracion ? mapeo_debug : mapeo_normal;

    werase(menu_win);

    // 1. Marco con caracteres de línea fina
    wattron(menu_win, COLOR_PAIR(PAR_ATMOS_CELESTE) | A_BOLD);
    mvwprintw(menu_win, 0, 0, "┌");
    for (int x = 1; x < ancho - 1; x++) mvwprintw(menu_win, 0, x, "─");
    mvwprintw(menu_win, 0, ancho - 1, "┐");

    for (int y = 1; y < alto - 1; y++) {
      mvwprintw(menu_win, y, 0, "│");
      mvwprintw(menu_win, y, ancho - 1, "│");
    }

    mvwprintw(menu_win, alto - 1, 0, "└");
    for (int x = 1; x < ancho - 1; x++) mvwprintw(menu_win, alto - 1, x, "─");
    mvwprintw(menu_win, alto - 1, ancho - 1, "┘");

    const char *titulo = g_modo_depuracion ? " [ MENÚ DE CONTROL (MODO DEPURACIÓN) ] " : " [ MENÚ DE CONTROL ] ";
    int tx = (ancho - (int)strlen(titulo)) / 2;
    if (tx < 1) tx = 1;
    mvwprintw(menu_win, 0, tx, "%s", titulo);
    wattroff(menu_win, COLOR_PAIR(PAR_ATMOS_CELESTE) | A_BOLD);

    // 2. Dibujar las opciones resaltando solo la actual
    for (int i = 0; i < total_opciones; i++) {
      if (i == seleccion) {
        wattron(menu_win, A_REVERSE | A_BOLD);
        mvwprintw(menu_win, i + 2, 4, "-> %s", opciones[i]);
        wattroff(menu_win, A_REVERSE | A_BOLD);
      } else {
        mvwprintw(menu_win, i + 2, 4, "   %s", opciones[i]);
      }
    }

    // Pie de ayuda centrado con suficiente margen
    const char *pie = g_modo_depuracion
      ? "[↑/↓] Navegar   [ENTER] Elegir   [D] Modo Normal   [Q] Salir"
      : "[↑/↓] Navegar   [ENTER] Elegir   [D] Depuración    [Q] Salir";
    int pie_x = (ancho - 58) / 2;
    if (pie_x < 2) pie_x = 2;
    mvwprintw(menu_win, alto - 2, pie_x, "%s", pie);    wrefresh(menu_win);

    // 3. Capturar la tecla pulsada
    tecla = wgetch(menu_win);
    switch (tecla) {
      case KEY_UP:
        seleccion--;
        if (seleccion < 0) seleccion = total_opciones - 1;
        break;
      case KEY_DOWN:
        seleccion++;
        if (seleccion >= total_opciones) seleccion = 0;
        break;
      case 'd':
      case 'D':
        g_modo_depuracion = !g_modo_depuracion;
        seleccion = 0;
        delwin(menu_win);
        alto = g_modo_depuracion ? 14 : 12;
        inicio_y = (LINES - alto) / 2;
        inicio_x = (COLS - ancho) / 2;
        clear();
        refresh();
        menu_win = newwin(alto, ancho, inicio_y, inicio_x);
        keypad(menu_win, TRUE);
        break;
      case 10: // ENTER
      case 13:
      case KEY_ENTER:
        delwin(menu_win);
        clear();
        refresh();
        return mapeo[seleccion];
      case 'q':
      case 'Q':
      case 27: // ESC
        delwin(menu_win);
        clear();
        refresh();
        return MENU_SALIR;
    }
  }

  delwin(menu_win);
  return MENU_SALIR;
}
// ============================================================================
//  ESCENA: utilidades de geometría
// ============================================================================

// Metros representados por cada fila. En baja altitud es fija (300 m/fila);
// por encima de 40 km se "aleja la cámara" para que en el espacio la fila
// inferior llegue a ~20 km y se vea el disco terrestre.
static double escala_vertical(double altitud, int filas_abajo) {
    if (filas_abajo < 1) filas_abajo = 1;
    double escala_alta = (altitud - 20000.0) / (double)filas_abajo;
    if (escala_alta < ESCALA_BAJA) escala_alta = ESCALA_BAJA;
    double t = suavizar((altitud - 40000.0) / 40000.0);
    return ESCALA_BAJA + t * (escala_alta - ESCALA_BAJA);
}

// Altura (en filas) del arco del horizonte terrestre: 0 en baja altitud, 8 en el espacio
static double curvatura_filas(double altitud) {
    return 8.0 * suavizar((altitud - 40000.0) / 60000.0);
}

// Densidad continua según la altitud de la celda: 0.0 (espacio) .. NIVEL_MAXIMO (cielo pleno)
static double nivel_densidad(double h) {
    if (h <= H_CIELO_PLENO) return (double)NIVEL_MAXIMO;
    if (h >= H_LIMITE_ESPACIO) return 0.0;
    return (H_LIMITE_ESPACIO - h) / (H_LIMITE_ESPACIO - H_CIELO_PLENO) * (double)NIVEL_MAXIMO;
}

// Línea Braille fina ubicada en la sub-fila t de la celda (0 = abajo, 1 = arriba)
static const char *linea_braille_subfila(double t) {
    if (t >= 0.75) return "⠉";
    if (t >= 0.50) return "⠒";
    if (t >= 0.25) return "⠤";
    return "⣀";
}

// ============================================================================
//  ESCENA: capas de dibujo
// ============================================================================

static void dibujar_estrella(WINDOW *win, int y, int x, unsigned int rnd) {
    if (rnd < 8) {
        wattron(win, COLOR_PAIR(PAR_ESTRELLAS) | A_BOLD);
        mvwprintw(win, y, x, "✦");
        wattroff(win, COLOR_PAIR(PAR_ESTRELLAS) | A_BOLD);
    } else if (rnd < 25) {
        wattron(win, COLOR_PAIR(PAR_ESTRELLAS));
        mvwprintw(win, y, x, "·");
        wattroff(win, COLOR_PAIR(PAR_ESTRELLAS));
    } else {
        wattron(win, COLOR_PAIR(PAR_ESTRELLAS));
        mvwaddch(win, y, x, '.');
        wattroff(win, COLOR_PAIR(PAR_ESTRELLAS));
    }
}

// Renderiza una celda según su nivel de densidad:
// 0 = espacio, 1-8 = puntos celestes sobre negro, 9-16 = puntos negros sobre celeste, 17 = celeste sólido
static void dibujar_celda_atmosfera(WINDOW *win, int y, int x, int nivel, unsigned int seed) {
    unsigned int rnd = hash_espacial(x, y, seed) % 1000;

    if (nivel <= 0) {
        // ESPACIO PROFUNDO (Estrellas aleatorias fijas sobre fondo negro)
        if (rnd < 45) dibujar_estrella(win, y, x, rnd);
        else mvwaddch(win, y, x, ' ');
        return;
    }

    // Estrellas que aún se ven entre los huecos negros de la atmósfera más tenue
    if ((nivel == 1 && rnd < 20) || (nivel == 2 && rnd < 10) || (nivel == 3 && rnd < 5)) {
        dibujar_estrella(win, y, x, rnd);
        return;
    }

    if (nivel >= NIVEL_MAXIMO) {
        // CIELO PLENO: Fondo celeste sólido sin texto
        wattron(win, COLOR_PAIR(PAR_ATMOS_MAXIMA));
        mvwaddch(win, y, x, ' ');
        wattroff(win, COLOR_PAIR(PAR_ATMOS_MAXIMA));
        return;
    }

    const char *car = " ";

    // FASE 1 (Niveles 1 al 8): Fondo negro con puntos celestes crecientes (0% -> 50% celeste)
    if (nivel <= 8) {
        switch (nivel) {
            case 1: car = (x % 2 == 0) ? "⠁" : " "; break;
            case 2: car = (x % 2 == 0) ? "⠁" : "⠄"; break;
            case 3: car = (x % 2 == 0) ? "⠁" : "⠅"; break;
            case 4: car = "⠅"; break;
            case 5: car = (x % 2 == 0) ? "⠕" : "⠅"; break;
            case 6: car = (x % 2 == 0) ? "⠕" : "⢅"; break;
            case 7: car = (x % 2 == 0) ? "⠕" : "⢕"; break;
            default: car = "⢕"; break;
        }
        wattron(win, COLOR_PAIR(PAR_ATMOS_CELESTE));
        mvwprintw(win, y, x, "%s", car);
        wattroff(win, COLOR_PAIR(PAR_ATMOS_CELESTE));
        return;
    }

    // FASE 2 (Niveles 9 al 16): Fondo celeste con puntos negros decrecientes (50% -> 100% celeste)
    switch (nivel) {
        case 9:  car = "⢕"; break;
        case 10: car = (x % 2 == 0) ? "⠕" : "⢕"; break;
        case 11: car = (x % 2 == 0) ? "⠕" : "⢅"; break;
        case 12: car = (x % 2 == 0) ? "⠕" : "⠅"; break;
        case 13: car = "⠅"; break;
        case 14: car = (x % 2 == 0) ? "⠁" : "⠅"; break;
        case 15: car = (x % 2 == 0) ? "⠁" : "⠄"; break;
        default: car = (x % 2 == 0) ? "⠁" : " "; break;
    }
    wattron(win, COLOR_PAIR(PAR_ATMOS_MAXIMA));
    mvwprintw(win, y, x, "%s", car);
    wattroff(win, COLOR_PAIR(PAR_ATMOS_MAXIMA));
}

// Determina si la celda (x, h_fila) pertenece a una nube cúmulo.
// Las nubes viven en coordenadas de mundo (altitud), por lo que se desplazan
// hacia arriba al descender (efecto de paralaje sin animación).
// Devuelve el glifo a dibujar o NULL si la celda no pertenece a ninguna nube.
static const char *glifo_nube(int x, double h_fila, int max_x, unsigned int seed) {
    if (h_fila < H_NUBES_MIN || h_fila > H_NUBES_MAX || max_x < 8) return NULL;

    int fila_mundo = (int)floor(h_fila / ESCALA_BAJA);
    int banda = fila_mundo / 6;            // Cada banda de 6 filas puede alojar nubes
    int r = fila_mundo - banda * 6;        // 4 = cima, 3 = cuerpo, 2 = base (mayor = más alto)
    if (r < 2 || r > 4) return NULL;

    int mitad = max_x / 2;
    for (int k = 0; k < 2; k++) {
        unsigned int hb = hash_espacial(banda, k + 7, seed ^ 0x9E3779B9u);
        if (hb % 100 >= 55) continue;      // 55% de probabilidad de nube por slot

        int w  = 6 + (int)((hb >> 8) % 10);                       // Semiancho del cuerpo (6..15)
        int cx = k * mitad + (int)((hb >> 16) % (unsigned int)mitad);
        int d  = x - cx;

        if (r == 4) {
            // CIMA (más angosta y desplazada para un perfil orgánico): ⢀⣠⣤⣤⣄⡀
            int wt  = w / 2;
            int off = (int)((hb >> 4) % (unsigned int)w) - w / 2;
            int dt  = x - (cx + off);
            if (dt < -wt || dt > wt) continue;
            if (dt == -wt)     return "⢀";
            if (dt == -wt + 1) return "⣠";
            if (dt ==  wt - 1) return "⣄";
            if (dt ==  wt)     return "⡀";
            return "⣤";
        }
        if (r == 3) {
            // CUERPO: ⢀⣴⣿⣿⣿⣿⣦⡀
            if (d < -w || d > w) continue;
            if (d == -w)     return "⢀";
            if (d == -w + 1) return "⣴";
            if (d ==  w - 1) return "⣦";
            if (d ==  w)     return "⡀";
            return "⣿";
        }
        // BASE PLANA: ⠘⠿⠿⠿⠿⠃
        if (d < -(w - 1) || d > (w - 1)) continue;
        if (d == -(w - 1)) return "⠘";
        if (d ==  (w - 1)) return "⠃";
        return "⠿";
    }
    return NULL;
}

// Dibuja una fila de océano con perspectiva:
// p = 0 (horizonte lejano, olas finas y oscuras) .. p = 1 (primer plano, olas grandes con espuma)
static void dibujar_fila_oceano(WINDOW *win, int y, double p, int max_x) {
    int cx_sol   = (max_x * 2) / 3;        // Columna del reflejo solar (fuera del centro de la cápsula)
    int semi_sol = (int)(p * 2.5);         // El reflejo se ensancha hacia el primer plano
    int d        = y;                      // Desfase por fila para romper la regularidad

    for (int x = 1; x < max_x - 1; x++) {
        // --- Reflejo del sol ---
        int dxs = abs(x - cx_sol);
        if (dxs <= semi_sol) {
            const char *g;
            if      (p < 0.30) g = "┊";
            else if (p < 0.50) g = "╎";
            else if (p < 0.75) g = "│";
            else if (p < 0.90) g = "┃";
            else               g = (dxs == 0) ? "█" : "▓";
            wattron(win, COLOR_PAIR(PAR_SOL) | A_BOLD);
            mvwprintw(win, y, x, "%s", g);
            wattroff(win, COLOR_PAIR(PAR_SOL) | A_BOLD);
            continue;
        }

        // --- Olas según la profundidad de perspectiva ---
        int s = x + d * 3;
        const char *g = " ";
        int par = PAR_MAR;
        attr_t at = 0;

        if (p < 0.15) {
            // Mar lejano: líneas finas casi continuas
            g = (s % 7 == 0) ? "⠒" : "⠤";
            par = PAR_MAR_LEJANO; at = A_BOLD;
        } else if (p < 0.35) {
            int m = s % 6;
            g = (m < 2) ? "⠒" : (m == 3 || m == 4) ? "⠤" : " ";
        } else if (p < 0.55) {
            int m = s % 5;
            g = (m == 0 || m == 3) ? "~" : " ";
            at = A_BOLD;
        } else if (p < 0.75) {
            g = (s % 6 < 3) ? "≈" : " ";
            at = A_BOLD;
        } else if (p < 0.90) {
            int m = s % 7;
            if (m == 2) { g = "˜"; par = PAR_ESPUMA; }
            else        { g = (m < 4) ? "∽" : " "; }
            at = A_BOLD;
        } else {
            if (s % 12 == 4) { g = "˜"; par = PAR_ESPUMA; }
            else             { g = (s % 2 == 0) ? "◠" : "◡"; }
            at = A_BOLD;
        }

        wattron(win, COLOR_PAIR(par) | at);
        mvwprintw(win, y, x, "%s", g);
        wattroff(win, COLOR_PAIR(par) | at);
    }
}

// Anillos concéntricos de ondas alrededor del punto de amerizaje (centro de la pantalla)
static void dibujar_ondas_amerizaje(WINDOW *win, int y, int max_x) {
    static const int radios[] = { 8, 11, 14 };
    int cx = max_x / 2;

    wattron(win, COLOR_PAIR(PAR_ESPUMA) | A_BOLD);
    for (int i = 0; i < 3; i++) {
        int xl = cx - radios[i];
        int xr = cx + radios[i];
        if (xl >= 1)         mvwprintw(win, y, xl, "(");
        if (xr < max_x - 1)  mvwprintw(win, y, xr, ")");
    }
    wattroff(win, COLOR_PAIR(PAR_ESPUMA) | A_BOLD);
}

// ============================================================================
//  ESCENA: composición del fondo completo
// ============================================================================

static void rellenar_fondo_escena_viewport(WINDOW *win, double altitud, int y_suelo, int y_amerizaje_final) {
    int max_y, max_x;
    getmaxyx(win, max_y, max_x);

    // 1. Geometría vertical ajustada al viewport útil
    double fila_centro = (1.0 + y_suelo) / 2.0;
    int filas_abajo    = (int)(y_suelo - fila_centro);
    double escala      = escala_vertical(altitud, filas_abajo > 0 ? filas_abajo : 1);
    double curvatura   = curvatura_filas(altitud);
    double semi_ancho  = max_x / 2.0;

    // 2. Horizonte marino: asoma por el borde inferior (y_suelo) a 2500 m y sube hasta y_amerizaje_final
    int y_amerizaje = y_amerizaje_final;
    int filas_mar_finales = y_suelo - y_amerizaje;
    if (filas_mar_finales < 1) filas_mar_finales = 1;

    int linea_agua = y_suelo + 1; // Fuera del viewport por defecto
    if (altitud <= H_INICIO_OCEANO) {
        double ratio = altitud / H_INICIO_OCEANO; // 1.0 a 2500 m, 0.0 a 0 m
        linea_agua = y_amerizaje + (int)(ratio * (y_suelo - y_amerizaje));
    }

    // 3. Pintar fila por fila desde el techo hasta y_suelo
    for (int y = 1; y <= y_suelo && y < max_y - 1; y++) {

        // --- OCÉANO con perspectiva ---
        if (y >= linea_agua) {
            // La perspectiva se mide contra el mar final: al acercarse aparecen olas más grandes
            double p = (double)(y - linea_agua) / (double)(filas_mar_finales > 1 ? filas_mar_finales - 1 : 1);
            if (p > 1.0) p = 1.0;
            dibujar_fila_oceano(win, y, p, max_x);
            continue;
        }

        // --- LÍNEA DE BRUMA sobre el horizonte marino ---
        if (y == linea_agua - 1) {
            wattron(win, COLOR_PAIR(PAR_BRUMA) | A_BOLD);
            for (int x = 1; x < max_x - 1; x++) mvwprintw(win, y, x, "─");
            wattroff(win, COLOR_PAIR(PAR_BRUMA) | A_BOLD);
            continue;
        }

        // --- CIELO: espacio, contorno terrestre, atmósfera y nubes ---
        double h_fila = altitud + (fila_centro - y) * escala;

        for (int x = 1; x < max_x - 1; x++) {
            // Curvatura terrestre: hacia los bordes la atmósfera queda más abajo (arco convexo)
            double dx = (x - semi_ancho) / semi_ancho;
            double h_celda = h_fila + curvatura * escala * dx * dx;

            // Airglow: línea fina verde exactamente sobre el límite de 80 km
            if (fabs(h_celda - H_LIMITE_ESPACIO) < escala / 2.0) {
                double t = (H_LIMITE_ESPACIO - (h_celda - escala / 2.0)) / escala;
                wattron(win, COLOR_PAIR(PAR_AIRGLOW) | A_BOLD);
                mvwprintw(win, y, x, "%s", linea_braille_subfila(t));
                wattroff(win, COLOR_PAIR(PAR_AIRGLOW) | A_BOLD);
                continue;
            }

            // Densidad atmosférica con tramado Bayer 4x4
            double nivel_continuo = nivel_densidad(h_celda);
            int nivel_celda = (int)nivel_continuo;
            double frac = nivel_continuo - (double)nivel_celda;
            if (frac > 0.0 && bayer4x4[y % 4][x % 4] < frac) nivel_celda++;

            // Nubes cúmulo sobre el cielo pleno
            if (nivel_celda >= NIVEL_MAXIMO) {
                const char *nube = glifo_nube(x, h_fila, max_x, semilla_universo);
                if (nube != NULL) {
                    wattron(win, COLOR_PAIR(PAR_NUBE) | A_BOLD);
                    mvwprintw(win, y, x, "%s", nube);
                    wattroff(win, COLOR_PAIR(PAR_NUBE) | A_BOLD);
                    continue;
                }
            }

            dibujar_celda_atmosfera(win, y, x, nivel_celda, semilla_universo);
        }
    }

    // 4. Ondas de amerizaje para el visor de fondo estático
    if (altitud <= 50.0 && y_amerizaje_final == (max_y - (max_y / 3))) {
        int y_ondas = linea_agua + (filas_mar_finales * 2) / 3;
        if (y_ondas >= 1 && y_ondas < max_y - 1) {
            dibujar_ondas_amerizaje(win, y_ondas, max_x);
        }
    }
}

void rellenar_fondo_escena(WINDOW *win, double altitud) {
    int max_y, max_x;
    getmaxyx(win, max_y, max_x);
    (void)max_x;
    rellenar_fondo_escena_viewport(win, altitud, max_y - 2, max_y - (max_y / 3));
}

// Nombre descriptivo de la capa visible para el HUD de prueba
static const char *nombre_capa(double altitud) {
    if (altitud >= 100000.0) return "ESPACIO (curvatura terrestre)";
    if (altitud >=  80000.0) return "LIMITE ESPACIAL (airglow)";
    if (altitud >=  55000.0) return "MESOSFERA (puntos celestes)";
    if (altitud >=  30000.0) return "ESTRATOSFERA (mezcla invertida)";
    if (altitud >=  15000.0) return "ESTRATOSFERA BAJA (cielo pleno)";
    if (altitud >   H_INICIO_OCEANO) return "TROPOSFERA (nubes)";
    if (altitud >       50.0) return "APROXIMACION FINAL (oceano)";
    return "AMERIZAJE (Oceano Pacifico)";
}

void tui_test_fondo(void) {
    WINDOW *test_win = newwin(LINES, COLS, 0, 0);
    if (!test_win) return;

    keypad(test_win, TRUE);

    double altitud = 120000.0; // Comenzar en espacio exterior (120 km)
    int salir = 0;

    while (!salir) {
        werase(test_win);

        // 1. Renderizar fondo estático según la altitud actual
        rellenar_fondo_escena(test_win, altitud);

        // 2. Dibujar marco exterior
        box(test_win, 0, 0);

        // 3. Panel superior de Telemetría (HUD)
        wattron(test_win, A_REVERSE | A_BOLD);
        mvwprintw(test_win, 0, 2, " [ TEST DE FONDO POR ALTITUD ] ");
        mvwprintw(test_win, 1, 2, " Altitud: %9.1f m (%6.2f km) | %-34s ",
                  altitud, altitud / 1000.0, nombre_capa(altitud));
        wattroff(test_win, A_REVERSE | A_BOLD);

        // 4. Panel inferior con guía de controles
        wattron(test_win, A_REVERSE);
        mvwprintw(test_win, LINES - 1, 2,
                  " [↑/↓] ±2 km (±250 m bajo 5 km)  [PgUp/PgDn] ±10 km  [Inicio/Fin]  [R] Semilla  [Q] Volver ");
        wattroff(test_win, A_REVERSE);

        wrefresh(test_win);

        // 5. Espera síncrona por tecla (sin animación ni polling activo)
        double paso = (altitud < 5000.0) ? 250.0 : 2000.0;
        int tecla = wgetch(test_win);
        switch (tecla) {
            case KEY_UP:
                altitud += paso;
                if (altitud > 140000.0) altitud = 140000.0;
                break;
            case KEY_DOWN:
                altitud -= paso;
                if (altitud < 0.0) altitud = 0.0;
                break;
            case KEY_PPAGE: // Page Up
                altitud += 10000.0;
                if (altitud > 140000.0) altitud = 140000.0;
                break;
            case KEY_NPAGE: // Page Down
                altitud -= 10000.0;
                if (altitud < 0.0) altitud = 0.0;
                break;
            case KEY_HOME:
                altitud = 140000.0;
                break;
            case KEY_END:
                altitud = 0.0;
                break;
            case 'r':
            case 'R':
                semilla_universo = (unsigned int)rand() ^ (unsigned int)time(NULL);
                break;
            case 'q':
            case 'Q':
            case 27:        // ESC
            case 10:        // ENTER
            case 13:
            case KEY_ENTER:
                salir = 1;
                break;
            default:
                break;
        }
    }

    delwin(test_win);
    clear();
    refresh();
}

// ============================================================================
//  SPRITES Y ESTADOS VISUALES DE LA CÁPSULA ORION
// ============================================================================

typedef enum {
    ESTADO_CAP_ESPACIO = 0,
    ESTADO_CAP_PLASMA,
    ESTADO_CAP_DROGUE,
    ESTADO_CAP_PARACAIDAS,
    ESTADO_CAP_AMERIZAJE,
    ESTADO_CAP_REBOTE,
    ESTADO_CAP_FALLO_TERMICO,
    ESTADO_CAP_FALLO_IMPACTO,
    TOTAL_ESTADOS_CAP
} EstadoCapsulaVisual;

typedef struct {
    const char *titulo;
    const char *fase_sim;
    const char *condicion;
    double altitud_escena;
    double velocidad_ms;
    double temp_escudo;
    double integridad;
    double carga_g;
    const char *descripcion;
    const char *analisis_fisico;
} InfoEstadoCapsula;

static const InfoEstadoCapsula INFO_ESTADOS[TOTAL_ESTADOS_CAP] = {
    {
        "1. VUELO ORBITAL Y ORIENTACIÓN",
        "FASE_ENTRADA_ATMOSFERICA",
        "Nominal (Alineación con gamma = 6.2°)",
        120000.0,
        11000.0,
        25.0,
        100.0,
        0.0,
        "Cápsula en el vacío espacial con escudo térmico al frente. RCS activo.",
        "Energía Mecánica inicial Em = 2.45e12 J (máxima). Velocidad Mach ~32 antes del choque."
    },
    {
        "2. REINGRESO HIPERSÓNICO Y PICO TÉRMICO",
        "FASE_FRENADO_HIPERSONICO",
        "Nominal (Frenado mesosférico violento)",
        55000.0,
        7800.0,
        2760.0,
        88.0,
        8.5,
        "Fricción extrema contra el aire. El escudo de ablación Avcoat ioniza el flujo formando plasma.",
        "Trabajo de fricción W_roz disipa el 99% de Ek en calor radiativo. Blackout de comunicaciones."
    },
    {
        "3. EYECCIÓN DE CUBIERTA Y DROGUE CHUTES",
        "FASE_FRENADO_HIPERSONICO -> PARACAÍDAS",
        "Nominal (Transición a régimen subsónico)",
        7500.0,
        135.0,
        320.0,
        76.0,
        2.3,
        "Cubierta delantera eyectada. 2 paracaídas de frenado (drogue chutes) estabilizan la nave.",
        "Reducción de velocidad por arrastre aerodinámico previo al despliegue de cúpulas principales."
    },
    {
        "4. DESPLIEGUE 3 PARACAÍDAS PRINCIPALES",
        "FASE_PARACAIDAS",
        "Nominal (Descenso terminal controlado)",
        1800.0,
        8.5,
        65.0,
        76.0,
        1.0,
        "3 cúpulas concéntricas tricolor de 35 m de diámetro. Descenso terminal suave y seguro.",
        "Equilibrio de fuerzas F_arrastre = m*g. Velocidad terminal constante de ~8.0 a 8.5 m/s."
    },
    {
        "5. AMERIZAJE Y FLOTACIÓN ACTIVA (CMUS)",
        "FASE_AMERIZAJE / MISION_EXITOSA",
        "Éxito total de la misión",
        0.0,
        0.0,
        22.0,
        76.0,
        0.0,
        "Amerizaje en el Océano Pacífico. Inflado de 5 airbags CMUS verticales y baliza activa.",
        "Teorema Trabajo-Energía en agua: penetración 1.5 m. Fuerza hidrocinética soportada = 83 kN."
    },
    {
        "6. FALLO CRÍTICO: REBOTE ATMOSFÉRICO",
        "MISION_FALLO_REBOTE",
        "Ángulo muy plano (gamma < 5.5°)",
        85000.0,
        10850.0,
        450.0,
        98.0,
        1.4,
        "La nave 'patinó' sobre la alta atmósfera y sale rebotada hacia el espacio profundo.",
        "Disipación de energía insuficiente. Órbita heliocéntrica sin retorno para la tripulación."
    },
    {
        "7. FALLO CRÍTICO: COLAPSO TÉRMICO",
        "MISION_FALLO_TERMICO",
        "Ángulo excesivo (gamma > 7.0°) o daño escudo",
        42000.0,
        8900.0,
        3950.0,
        0.0,
        16.8,
        "El flujo calórico superó el límite de ablación. Ruptura del fuselaje e ignición catastrófica.",
        "Carga aerodinámica > 15 G. Tasa de transferencia calórica destructiva instantánea."
    },
    {
        "8. FALLO CRÍTICO: IMPACTO TERMINAL",
        "MISION_FALLO_IMPACTO / PARACAIDAS",
        "Fallo de apertura de paracaídas (v > 120 m/s)",
        0.0,
        110.0,
        40.0,
        0.0,
        41.5,
        "Colisión a 396 km/h contra el océano sin desaceleración previa. Rotura estructural total.",
        "Fuerza media de impacto F = m*v^2/(2*d) = 4.2 MN (~41 veces el peso). Catastrófico."
    }
};

static void dibujar_sprite_espacio(WINDOW *win, int cy, int cx, int anim_tick) {
    // Cápsula Orion en el espacio: cuerpo en cy - 1 a cy + 6 (sin etiqueta de velocidad)
    // Línea 0: Cúpula de acople (k = 3, ancho = 7)
    wattron(win, COLOR_PAIR(PAR_ESTRELLAS) | A_BOLD);
    mvwprintw(win, cy - 1, cx - 3, "%s", "╭━━┻━━╮");

    // Línea 1: Módulo RCS (k = 4, ancho = 9)
    mvwprintw(win, cy,     cx - 4, "%s", "/  RCS  \\");
    wattroff(win, COLOR_PAIR(PAR_ESTRELLAS) | A_BOLD);
    wattron(win, COLOR_PAIR(PAR_AMARILLO_NEGRO) | A_BOLD);
    mvwprintw(win, cy,     cx - 1, "%s", "RCS");
    wattroff(win, COLOR_PAIR(PAR_AMARILLO_NEGRO) | A_BOLD);

    // Línea 2: Parte superior cabina (k = 5, ancho = 11)
    wattron(win, COLOR_PAIR(PAR_ESTRELLAS) | A_BOLD);
    mvwprintw(win, cy + 1, cx - 5, "%s", "/ ╭─────╮ \\");

    // Línea 3: Ventanas de tripulación (k = 6, ancho = 13)
    mvwprintw(win, cy + 2, cx - 6, "%s", "/  │     │  \\");
    wattroff(win, COLOR_PAIR(PAR_ESTRELLAS) | A_BOLD);
    wattron(win, COLOR_PAIR(PAR_CYAN_NEGRO) | A_BOLD);
    mvwprintw(win, cy + 2, cx - 1, "%s", "◉ ◉");
    wattroff(win, COLOR_PAIR(PAR_CYAN_NEGRO) | A_BOLD);

    // Línea 4: Base cabina (k = 7, ancho = 15)
    wattron(win, COLOR_PAIR(PAR_ESTRELLAS) | A_BOLD);
    mvwprintw(win, cy + 3, cx - 7, "%s", "/   ╰─────╯   \\");

    // Línea 5: Insignias de misión (k = 8, ancho = 17)
    mvwprintw(win, cy + 4, cx - 8, "%s", "/ ◈  USA NASA ◈ \\");
    wattroff(win, COLOR_PAIR(PAR_ESTRELLAS) | A_BOLD);
    wattron(win, COLOR_PAIR(PAR_PLASMA_FUEGO) | A_BOLD);
    mvwprintw(win, cy + 4, cx - 3, "%s", "USA");
    wattroff(win, COLOR_PAIR(PAR_PLASMA_FUEGO) | A_BOLD);
    wattron(win, COLOR_PAIR(PAR_CYAN_NEGRO) | A_BOLD);
    mvwprintw(win, cy + 4, cx + 1, "%s", "NASA");
    wattroff(win, COLOR_PAIR(PAR_CYAN_NEGRO) | A_BOLD);

    // Línea 6: Borde inferior del cono (k = 9, ancho = 19)
    wattron(win, COLOR_PAIR(PAR_ESTRELLAS) | A_BOLD);
    mvwprintw(win, cy + 5, cx - 9, "%s", "╭─────────────────╮");
    wattroff(win, COLOR_PAIR(PAR_ESTRELLAS) | A_BOLD);

    // Línea 7: Escudo térmico Avcoat (k = 9, ancho = 19)
    wattron(win, COLOR_PAIR(PAR_AMARILLO_NEGRO) | A_BOLD);
    mvwprintw(win, cy + 6, cx - 9, "%s", "╰━━▓▓▓▓▓▓▓▓▓▓▓▓▓━━╯");
    wattroff(win, COLOR_PAIR(PAR_AMARILLO_NEGRO) | A_BOLD);

    // Gas RCS frío animado (pulsos rítmicos)
    if ((anim_tick / 6) % 2 == 1) {
        wattron(win, COLOR_PAIR(PAR_CYAN_NEGRO) | A_BOLD);
        mvwprintw(win, cy + 7, cx - 7, "%s", "· ·");
        mvwprintw(win, cy + 7, cx + 5, "%s", "· ·");
        wattroff(win, COLOR_PAIR(PAR_CYAN_NEGRO) | A_BOLD);
    }
}

static void dibujar_sprite_plasma(WINDOW *win, int cy, int cx, int anim_tick) {
    int flame_step = anim_tick % 3;

    // 1. Vaina de plasma envolvente en los flancos laterales (sin etiqueta de velocidad)
    wattron(win, COLOR_PAIR(PAR_PLASMA_FUEGO) | A_BOLD);
    if (flame_step == 0) {
        mvwprintw(win, cy - 1, cx - 7,  "%s", "░ ▒ ");
        mvwprintw(win, cy - 1, cx + 4,  "%s", " ▒ ░");
        mvwprintw(win, cy,     cx - 8,  "%s", "▒ ▓ ");
        mvwprintw(win, cy,     cx + 5,  "%s", " ▓ ▒");
        mvwprintw(win, cy + 1, cx - 9,  "%s", "▓ █ ");
        mvwprintw(win, cy + 1, cx + 6,  "%s", " █ ▓");
        mvwprintw(win, cy + 2, cx - 10, "%s", "█ ▓ ");
        mvwprintw(win, cy + 2, cx + 7,  "%s", " ▓ █");
        mvwprintw(win, cy + 3, cx - 11, "%s", "▓ █ ");
        mvwprintw(win, cy + 3, cx + 8,  "%s", " █ ▓");
        mvwprintw(win, cy + 4, cx - 12, "%s", "█ ▓ ");
        mvwprintw(win, cy + 4, cx + 9,  "%s", " ▓ █");
    } else if (flame_step == 1) {
        mvwprintw(win, cy - 1, cx - 7,  "%s", " ж  ");
        mvwprintw(win, cy - 1, cx + 4,  "%s", "  ж ");
        mvwprintw(win, cy,     cx - 8,  "%s", "░ ▒ ");
        mvwprintw(win, cy,     cx + 5,  "%s", " ▒ ░");
        mvwprintw(win, cy + 1, cx - 9,  "%s", "▒ ▓ ");
        mvwprintw(win, cy + 1, cx + 6,  "%s", " ▓ ▒");
        mvwprintw(win, cy + 2, cx - 10, "%s", "▓ █ ");
        mvwprintw(win, cy + 2, cx + 7,  "%s", " █ ▓");
        mvwprintw(win, cy + 3, cx - 11, "%s", "█ ▓ ");
        mvwprintw(win, cy + 3, cx + 8,  "%s", " ▓ █");
        mvwprintw(win, cy + 4, cx - 12, "%s", "▓ █ ");
        mvwprintw(win, cy + 4, cx + 9,  "%s", " █ ▓");
    } else {
        mvwprintw(win, cy - 1, cx - 7,  "%s", " ~  ");
        mvwprintw(win, cy - 1, cx + 4,  "%s", "  ~ ");
        mvwprintw(win, cy,     cx - 8,  "%s", " ж  ");
        mvwprintw(win, cy,     cx + 5,  "%s", "  ж ");
        mvwprintw(win, cy + 1, cx - 9,  "%s", "░ ▒ ");
        mvwprintw(win, cy + 1, cx + 6,  "%s", " ▒ ░");
        mvwprintw(win, cy + 2, cx - 10, "%s", "▒ ▓ ");
        mvwprintw(win, cy + 2, cx + 7,  "%s", " ▓ ▒");
        mvwprintw(win, cy + 3, cx - 11, "%s", "▓ █ ");
        mvwprintw(win, cy + 3, cx + 8,  "%s", " █ ▓");
        mvwprintw(win, cy + 4, cx - 12, "%s", "█ ▓ ");
        mvwprintw(win, cy + 4, cx + 9,  "%s", " ▓ █");
    }
    wattroff(win, COLOR_PAIR(PAR_PLASMA_FUEGO) | A_BOLD);

    // 2. Cápsula Orion: EXACTAMENTE ALINEADA CON ESTADO_CAP_ESPACIO (cy - 1 a cy + 6)
    // Línea 0: Cúpula de acople (k = 3, ancho = 7)
    wattron(win, COLOR_PAIR(PAR_ESTRELLAS) | A_BOLD);
    mvwprintw(win, cy - 1, cx - 3, "%s", "╭━━┻━━╮");

    // Línea 1: Módulo RCS (k = 4, ancho = 9)
    mvwprintw(win, cy,     cx - 4, "%s", "/  RCS  \\");
    wattroff(win, COLOR_PAIR(PAR_ESTRELLAS) | A_BOLD);
    wattron(win, COLOR_PAIR(PAR_AMARILLO_NEGRO) | A_BOLD);
    mvwprintw(win, cy,     cx - 1, "%s", "RCS");
    wattroff(win, COLOR_PAIR(PAR_AMARILLO_NEGRO) | A_BOLD);

    // Línea 2: Parte superior cabina (k = 5, ancho = 11)
    wattron(win, COLOR_PAIR(PAR_ESTRELLAS) | A_BOLD);
    mvwprintw(win, cy + 1, cx - 5, "%s", "/ ╭─────╮ \\");

    // Línea 3: Ventanas de tripulación (k = 6, ancho = 13)
    mvwprintw(win, cy + 2, cx - 6, "%s", "/  │     │  \\");
    wattroff(win, COLOR_PAIR(PAR_ESTRELLAS) | A_BOLD);
    wattron(win, COLOR_PAIR(PAR_CYAN_NEGRO) | A_BOLD);
    mvwprintw(win, cy + 2, cx - 1, "%s", "◉ ◉");
    wattroff(win, COLOR_PAIR(PAR_CYAN_NEGRO) | A_BOLD);

    // Línea 4: Base cabina (k = 7, ancho = 15)
    wattron(win, COLOR_PAIR(PAR_ESTRELLAS) | A_BOLD);
    mvwprintw(win, cy + 3, cx - 7, "%s", "/   ╰─────╯   \\");

    // Línea 5: Insignias de misión (k = 8, ancho = 17)
    mvwprintw(win, cy + 4, cx - 8, "%s", "/ ◈  USA NASA ◈ \\");
    wattroff(win, COLOR_PAIR(PAR_ESTRELLAS) | A_BOLD);
    wattron(win, COLOR_PAIR(PAR_PLASMA_FUEGO) | A_BOLD);
    mvwprintw(win, cy + 4, cx - 3, "%s", "USA");
    wattroff(win, COLOR_PAIR(PAR_PLASMA_FUEGO) | A_BOLD);
    wattron(win, COLOR_PAIR(PAR_CYAN_NEGRO) | A_BOLD);
    mvwprintw(win, cy + 4, cx + 1, "%s", "NASA");
    wattroff(win, COLOR_PAIR(PAR_CYAN_NEGRO) | A_BOLD);

    // Línea 6: Borde inferior del cono con Bow Shock lateral
    wattron(win, COLOR_PAIR(PAR_AMARILLO_NEGRO) | A_BOLD);
    if (flame_step == 0) {
        mvwprintw(win, cy + 5, cx - 13, "%s", "◀◀◀ ");
        mvwprintw(win, cy + 5, cx + 10, "%s", " ▶▶▶");
    } else {
        mvwprintw(win, cy + 5, cx - 13, "%s", "◄◄◄ ");
        mvwprintw(win, cy + 5, cx + 10, "%s", " ►►►");
    }
    wattroff(win, COLOR_PAIR(PAR_AMARILLO_NEGRO) | A_BOLD);

    wattron(win, COLOR_PAIR(PAR_ESTRELLAS) | A_BOLD);
    mvwprintw(win, cy + 5, cx - 9, "%s", "╭─────────────────╮");
    wattroff(win, COLOR_PAIR(PAR_ESTRELLAS) | A_BOLD);

    // Línea 7: Escudo térmico Avcoat incandescente
    wattron(win, COLOR_PAIR(PAR_AMARILLO_NEGRO) | A_BOLD);
    mvwprintw(win, cy + 6, cx - 9, "%s", "╰━━▓▓▓▓▓▓▓▓▓▓▓▓▓━━╯");
    wattroff(win, COLOR_PAIR(PAR_AMARILLO_NEGRO) | A_BOLD);

    wattron(win, COLOR_PAIR(PAR_PLASMA_FUEGO) | A_BOLD);
    mvwprintw(win, cy + 6, cx - 13, "%s", "░▒▓ ");
    mvwprintw(win, cy + 6, cx + 10, "%s", " ▓▒░");
    wattroff(win, COLOR_PAIR(PAR_PLASMA_FUEGO) | A_BOLD);

    // 3. Capa de choque hipersónico (shock layer) y cola de plasma bajo el escudo
    wattron(win, COLOR_PAIR(PAR_PLASMA_FUEGO) | A_BOLD);
    if (flame_step == 0) {
        mvwprintw(win, cy + 7, cx - 10, "%s", "╲ ╲ ж ж ж ж ж ж ж ╱ ╱");
        mvwprintw(win, cy + 8, cx - 9,  "%s", "╲ ╲  ░ ▒ ▓ ▒ ░  ╱ ╱");
    } else if (flame_step == 1) {
        mvwprintw(win, cy + 7, cx - 10, "%s", " ╲ ╲ ▓ ▒ ░ ▒ ▓ ▒ ╱ ╱ ");
        mvwprintw(win, cy + 8, cx - 9,  "%s", " ╲ ╲  ж ж ж ж ж  ╱ ╱ ");
    } else {
        mvwprintw(win, cy + 7, cx - 10, "%s", "╲ ╲ ~ ~ ~ ~ ~ ~ ~ ╱ ╱");
        mvwprintw(win, cy + 8, cx - 9,  "%s", "╲ ╲  ж ░ ▒ ░ ж  ╱ ╱");
    }
    wattroff(win, COLOR_PAIR(PAR_PLASMA_FUEGO) | A_BOLD);
}

static void dibujar_sprite_drogue(WINDOW *win, int cy, int cx, int anim_tick) {
    int c_izq = cx - 8;
    int c_der = cx + 8;
    int flutter = (anim_tick / 5) % 2;

    // 1. 2 Paracaídas Drogue simétricos (cy - 7 a cy - 5, ancho = 7, centros cx - 8 y cx + 8)
    wattron(win, COLOR_PAIR(PAR_PLASMA_FUEGO) | A_BOLD);
    mvwprintw(win, cy - 7, c_izq - 3, "%s", "╭━━━━━╮");
    mvwprintw(win, cy - 7, c_der - 3, "%s", "╭━━━━━╮");
    if (flutter == 0) {
        mvwprintw(win, cy - 6, c_izq - 3, "%s", "╭┫█ █┣╮");
        mvwprintw(win, cy - 6, c_der - 3, "%s", "╭┫█ █┣╮");
        mvwprintw(win, cy - 5, c_izq - 3, "%s", "╰━╲ ╱━╯");
        mvwprintw(win, cy - 5, c_der - 3, "%s", "╰━╲ ╱━╯");
    } else {
        mvwprintw(win, cy - 6, c_izq - 3, "%s", "╭┫▓ █┣╮");
        mvwprintw(win, cy - 6, c_der - 3, "%s", "╭┫█ ▓┣╮");
        mvwprintw(win, cy - 5, c_izq - 3, "%s", "╰━╲░╱━╯");
        mvwprintw(win, cy - 5, c_der - 3, "%s", "╰━╲░╱━╯");
    }
    wattroff(win, COLOR_PAIR(PAR_PLASMA_FUEGO) | A_BOLD);

    // 2. Risers de suspensión convergentes simétricos (cy - 4 a cy - 2)
    wattron(win, COLOR_PAIR(PAR_ESTRELLAS));
    mvwprintw(win, cy - 4, c_izq - 1, "%s", "╲ ╱");
    mvwprintw(win, cy - 4, c_der - 1, "%s", "╲ ╱");
    mvwprintw(win, cy - 3, cx - 5,     "%s", "╲");
    mvwprintw(win, cy - 3, cx + 5,     "%s", "╱");
    mvwprintw(win, cy - 2, cx - 2,     "%s", "╲ ┃ ╱");
    wattroff(win, COLOR_PAIR(PAR_ESTRELLAS));

    // 3. Cápsula Orion: EXACTAMENTE ALINEADA CON ESTADO_CAP_ESPACIO (cy - 1 a cy + 6)
    wattron(win, COLOR_PAIR(PAR_ESTRELLAS) | A_BOLD);
    mvwprintw(win, cy - 1, cx - 3, "%s", "╭━━┻━━╮");
    mvwprintw(win, cy,     cx - 4, "%s", "/  RCS  \\");
    wattroff(win, COLOR_PAIR(PAR_ESTRELLAS) | A_BOLD);
    wattron(win, COLOR_PAIR(PAR_AMARILLO_NEGRO) | A_BOLD);
    mvwprintw(win, cy,     cx - 1, "%s", "RCS");
    wattroff(win, COLOR_PAIR(PAR_AMARILLO_NEGRO) | A_BOLD);

    wattron(win, COLOR_PAIR(PAR_ESTRELLAS) | A_BOLD);
    mvwprintw(win, cy + 1, cx - 5, "%s", "/ ╭─────╮ \\");
    mvwprintw(win, cy + 2, cx - 6, "%s", "/  │     │  \\");
    wattroff(win, COLOR_PAIR(PAR_ESTRELLAS) | A_BOLD);

    wattron(win, COLOR_PAIR(PAR_CYAN_NEGRO) | A_BOLD);
    mvwprintw(win, cy + 2, cx - 1, "%s", "◉ ◉");
    wattroff(win, COLOR_PAIR(PAR_CYAN_NEGRO) | A_BOLD);

    wattron(win, COLOR_PAIR(PAR_ESTRELLAS) | A_BOLD);
    mvwprintw(win, cy + 3, cx - 7, "%s", "/   ╰─────╯   \\");
    mvwprintw(win, cy + 4, cx - 8, "%s", "/ ◈  USA NASA ◈ \\");
    wattroff(win, COLOR_PAIR(PAR_ESTRELLAS) | A_BOLD);
    wattron(win, COLOR_PAIR(PAR_PLASMA_FUEGO) | A_BOLD);
    mvwprintw(win, cy + 4, cx - 3, "%s", "USA");
    wattroff(win, COLOR_PAIR(PAR_PLASMA_FUEGO) | A_BOLD);
    wattron(win, COLOR_PAIR(PAR_CYAN_NEGRO) | A_BOLD);
    mvwprintw(win, cy + 4, cx + 1, "%s", "NASA");
    wattroff(win, COLOR_PAIR(PAR_CYAN_NEGRO) | A_BOLD);

    wattron(win, COLOR_PAIR(PAR_ESTRELLAS) | A_BOLD);
    mvwprintw(win, cy + 5, cx - 9, "%s", "╭─────────────────╮");
    wattroff(win, COLOR_PAIR(PAR_ESTRELLAS) | A_BOLD);

    wattron(win, COLOR_PAIR(PAR_AMARILLO_NEGRO) | A_BOLD);
    mvwprintw(win, cy + 6, cx - 9, "%s", "╰━━▓▓▓▓▓▓▓▓▓▓▓▓▓━━╯");
    wattroff(win, COLOR_PAIR(PAR_AMARILLO_NEGRO) | A_BOLD);
}

static void dibujar_sprite_paracaidas(WINDOW *win, int cy, int cx, int anim_tick) {
    int centros[3] = { cx - 11, cx, cx + 11 };
    int billow_base = (anim_tick / 6) % 3;

    // 1. 3 Cúpulas de 35 m de diámetro simétricas (cy - 8 a cy - 5, ancho = 9, centros cx - 11, cx, cx + 11)
    for (int i = 0; i < 3; i++) {
        int c  = centros[i];
        int x0 = c - 4;
        int phase = (billow_base + i) % 3;

        wattron(win, COLOR_PAIR(PAR_PLASMA_FUEGO) | A_BOLD);
        mvwprintw(win, cy - 8, x0, "%s", "╭━━━━━━━╮");
        if (phase == 0) {
            mvwprintw(win, cy - 7, x0, "%s", "╭┫█ █ █┣╮");
            mvwprintw(win, cy - 6, x0, "%s", "╰━╲ █ ╱━╯");
        } else if (phase == 1) {
            mvwprintw(win, cy - 7, x0, "%s", "╭┫█ ▓ █┣╮");
            mvwprintw(win, cy - 6, x0, "%s", "╰━╲ ▓ ╱━╯");
        } else {
            mvwprintw(win, cy - 7, x0, "%s", "╭┫▓ █ ▓┣╮");
            mvwprintw(win, cy - 6, x0, "%s", "╰━╲ █ ╱━╯");
        }
        mvwprintw(win, cy - 5, c - 2, "%s", "╲ █ ╱");
        wattroff(win, COLOR_PAIR(PAR_PLASMA_FUEGO) | A_BOLD);

        // Línea de suspensión vertical centrada
        wattron(win, COLOR_PAIR(PAR_ESTRELLAS));
        mvwprintw(win, cy - 4, c, "%s", "┃");
        wattroff(win, COLOR_PAIR(PAR_ESTRELLAS));
    }

    // 2. Bridas convergentes con pendiente simétrica uniforme (cy - 3 a cy - 2)
    wattron(win, COLOR_PAIR(PAR_ESTRELLAS));
    mvwprintw(win, cy - 3, cx - 7, "%s", "╲");
    mvwprintw(win, cy - 3, cx,     "%s", "┃");
    mvwprintw(win, cy - 3, cx + 7, "%s", "╱");
    mvwprintw(win, cy - 2, cx - 3, "%s", "╲  ┃  ╱");
    wattroff(win, COLOR_PAIR(PAR_ESTRELLAS));

    // 3. Cápsula Orion suspendida: EXACTAMENTE ALINEADA CON ESTADO_CAP_ESPACIO (cy - 1 a cy + 6)
    wattron(win, COLOR_PAIR(PAR_ESTRELLAS) | A_BOLD);
    mvwprintw(win, cy - 1, cx - 3, "%s", "╭━━┻━━╮");
    mvwprintw(win, cy,     cx - 4, "%s", "/  RCS  \\");
    wattroff(win, COLOR_PAIR(PAR_ESTRELLAS) | A_BOLD);
    wattron(win, COLOR_PAIR(PAR_AMARILLO_NEGRO) | A_BOLD);
    mvwprintw(win, cy,     cx - 1, "%s", "RCS");
    wattroff(win, COLOR_PAIR(PAR_AMARILLO_NEGRO) | A_BOLD);

    wattron(win, COLOR_PAIR(PAR_ESTRELLAS) | A_BOLD);
    mvwprintw(win, cy + 1, cx - 5, "%s", "/ ╭─────╮ \\");
    mvwprintw(win, cy + 2, cx - 6, "%s", "/  │     │  \\");
    wattroff(win, COLOR_PAIR(PAR_ESTRELLAS) | A_BOLD);

    wattron(win, COLOR_PAIR(PAR_CYAN_NEGRO) | A_BOLD);
    mvwprintw(win, cy + 2, cx - 1, "%s", "◉ ◉");
    wattroff(win, COLOR_PAIR(PAR_CYAN_NEGRO) | A_BOLD);

    wattron(win, COLOR_PAIR(PAR_ESTRELLAS) | A_BOLD);
    mvwprintw(win, cy + 3, cx - 7, "%s", "/   ╰─────╯   \\");
    mvwprintw(win, cy + 4, cx - 8, "%s", "/ ◈  USA NASA ◈ \\");
    wattroff(win, COLOR_PAIR(PAR_ESTRELLAS) | A_BOLD);
    wattron(win, COLOR_PAIR(PAR_PLASMA_FUEGO) | A_BOLD);
    mvwprintw(win, cy + 4, cx - 3, "%s", "USA");
    wattroff(win, COLOR_PAIR(PAR_PLASMA_FUEGO) | A_BOLD);
    wattron(win, COLOR_PAIR(PAR_CYAN_NEGRO) | A_BOLD);
    mvwprintw(win, cy + 4, cx + 1, "%s", "NASA");
    wattroff(win, COLOR_PAIR(PAR_CYAN_NEGRO) | A_BOLD);

    wattron(win, COLOR_PAIR(PAR_ESTRELLAS) | A_BOLD);
    mvwprintw(win, cy + 5, cx - 9, "%s", "╭─────────────────╮");
    wattroff(win, COLOR_PAIR(PAR_ESTRELLAS) | A_BOLD);

    wattron(win, COLOR_PAIR(PAR_AMARILLO_NEGRO) | A_BOLD);
    mvwprintw(win, cy + 6, cx - 9, "%s", "╰━━▓▓▓▓▓▓▓▓▓▓▓▓▓━━╯");
    wattroff(win, COLOR_PAIR(PAR_AMARILLO_NEGRO) | A_BOLD);
}

static void dibujar_sprite_amerizaje(WINDOW *win, int cy, int cx, int anim_tick) {
    int beacon_blink = (anim_tick / 6) % 2;
    int wave_phase   = (anim_tick / 5) % 3;

    // 1. Airbags CMUS inflados en cubierta superior (cy - 4 a cy - 3 sobre fondo celeste diurno)
    wattron(win, COLOR_PAIR(PAR_AIRBAG_CIELO) | A_BOLD);
    mvwprintw(win, cy - 4, cx - 7, "%s", "(●)   (●)   (●)");
    mvwprintw(win, cy - 3, cx - 7, "%s", "(●)   (●)   (●)");
    wattroff(win, COLOR_PAIR(PAR_AIRBAG_CIELO) | A_BOLD);

    wattron(win, COLOR_PAIR(PAR_TEXTO_CIELO) | A_BOLD);
    mvwprintw(win, cy - 2, cx - 4, "%s", "╲   ┃   ╱");
    wattroff(win, COLOR_PAIR(PAR_TEXTO_CIELO) | A_BOLD);

    // 2. Baliza de emergencia activa (cy - 3 a cy - 2 sobre fondo celeste diurno)
    if (beacon_blink == 1) {
        wattron(win, COLOR_PAIR(PAR_BEACON_CIELO) | A_BOLD);
        mvwprintw(win, cy - 3, cx + 6, "%s", "▲ 121.5 MHz");
        wattroff(win, COLOR_PAIR(PAR_BEACON_CIELO) | A_BOLD);
    } else {
        wattron(win, COLOR_PAIR(PAR_TEXTO_CIELO) | A_BOLD);
        mvwprintw(win, cy - 3, cx + 6, "%s", "△ 121.5 MHz");
        wattroff(win, COLOR_PAIR(PAR_TEXTO_CIELO) | A_BOLD);
    }

    wattron(win, COLOR_PAIR(PAR_TEXTO_CIELO) | A_BOLD);
    mvwprintw(win, cy - 2, cx + 6, "%s", "┃ [BEACON]");
    wattroff(win, COLOR_PAIR(PAR_TEXTO_CIELO) | A_BOLD);

    // 3. Cabina superior emergida: EXACTAMENTE ALINEADA (cy - 1 a cy + 3)
    wattron(win, COLOR_PAIR(PAR_ESTRELLAS) | A_BOLD);
    mvwprintw(win, cy - 1, cx - 3, "%s", "╭━━┻━━╮");
    mvwprintw(win, cy,     cx - 4, "%s", "/  RCS  \\");
    wattroff(win, COLOR_PAIR(PAR_ESTRELLAS) | A_BOLD);
    wattron(win, COLOR_PAIR(PAR_AMARILLO_NEGRO) | A_BOLD);
    mvwprintw(win, cy,     cx - 1, "%s", "RCS");
    wattroff(win, COLOR_PAIR(PAR_AMARILLO_NEGRO) | A_BOLD);

    wattron(win, COLOR_PAIR(PAR_ESTRELLAS) | A_BOLD);
    mvwprintw(win, cy + 1, cx - 5, "%s", "/ ╭─────╮ \\");
    mvwprintw(win, cy + 2, cx - 6, "%s", "/  │     │  \\");
    wattroff(win, COLOR_PAIR(PAR_ESTRELLAS) | A_BOLD);

    wattron(win, COLOR_PAIR(PAR_CYAN_NEGRO) | A_BOLD);
    mvwprintw(win, cy + 2, cx - 1, "%s", "◉ ◉");
    wattroff(win, COLOR_PAIR(PAR_CYAN_NEGRO) | A_BOLD);

    wattron(win, COLOR_PAIR(PAR_ESTRELLAS) | A_BOLD);
    mvwprintw(win, cy + 3, cx - 7, "%s", "/   ╰─────╯   \\");
    wattroff(win, COLOR_PAIR(PAR_ESTRELLAS) | A_BOLD);

    // 4. Línea de flotación en cy + 4 (coincide exactamente con y_amerizaje = cy + 4)
    wattron(win, COLOR_PAIR(PAR_MAR) | A_BOLD);
    if (wave_phase == 0) {
        mvwprintw(win, cy + 4, cx - 18, "%s", "≋ ≋ ≋ ≋ ≋ ");
        mvwprintw(win, cy + 4, cx + 9,  "%s", " ≋ ≋ ≋ ≋ ≋");
    } else if (wave_phase == 1) {
        mvwprintw(win, cy + 4, cx - 18, "%s", " ≋ ≋ ≋ ≋  ");
        mvwprintw(win, cy + 4, cx + 9,  "%s", "  ≋ ≋ ≋ ≋ ");
    } else {
        mvwprintw(win, cy + 4, cx - 18, "%s", "≈ ≈ ≈ ≈ ≈ ");
        mvwprintw(win, cy + 4, cx + 9,  "%s", " ≈ ≈ ≈ ≈ ≈");
    }
    wattroff(win, COLOR_PAIR(PAR_MAR) | A_BOLD);

    // Fuselaje en la línea de agua (ancho 17 de cx - 8 a cx + 8)
    wattron(win, COLOR_PAIR(PAR_ESTRELLAS) | A_BOLD);
    mvwprintw(win, cy + 4, cx - 8, "%s", "/");
    mvwprintw(win, cy + 4, cx + 8, "%s", "\\");
    wattroff(win, COLOR_PAIR(PAR_ESTRELLAS) | A_BOLD);

    wattron(win, COLOR_PAIR(PAR_CYAN_NEGRO) | A_BOLD);
    mvwprintw(win, cy + 4, cx - 6, "%s", "◈");
    mvwprintw(win, cy + 4, cx + 6, "%s", "◈");
    wattroff(win, COLOR_PAIR(PAR_CYAN_NEGRO) | A_BOLD);

    wattron(win, COLOR_PAIR(PAR_PLASMA_FUEGO) | A_BOLD);
    mvwprintw(win, cy + 4, cx - 3, "%s", "USA");
    wattroff(win, COLOR_PAIR(PAR_PLASMA_FUEGO) | A_BOLD);

    wattron(win, COLOR_PAIR(PAR_CYAN_NEGRO) | A_BOLD);
    mvwprintw(win, cy + 4, cx + 1, "%s", "NASA");
    wattroff(win, COLOR_PAIR(PAR_CYAN_NEGRO) | A_BOLD);

    // 5. Casco sumergido en el océano (cy + 5)
    wattron(win, COLOR_PAIR(PAR_ESPUMA) | A_BOLD);
    mvwprintw(win, cy + 5, cx - 18, "%s", "≈ ≈ ≈ ≈  ");
    mvwprintw(win, cy + 5, cx - 9,  "%s", "╭─────────────────╮");
    mvwprintw(win, cy + 5, cx + 10, "%s", "  ≈ ≈ ≈ ≈");
    wattroff(win, COLOR_PAIR(PAR_ESPUMA) | A_BOLD);

    // 6. Escudo térmico Avcoat sumergido en el mar (cy + 6 con fondo azul marino)
    wattron(win, COLOR_PAIR(PAR_MAR) | A_BOLD);
    mvwprintw(win, cy + 6, cx - 18, "%s", " ~ ~ ~ ~ ");
    mvwprintw(win, cy + 6, cx + 10, "%s", " ~ ~ ~ ~ ");
    wattroff(win, COLOR_PAIR(PAR_MAR) | A_BOLD);

    wattron(win, COLOR_PAIR(PAR_SOL) | A_BOLD);
    mvwprintw(win, cy + 6, cx - 9, "%s", "╰━━▓▓▓▓▓▓▓▓▓▓▓▓▓━━╯");
    wattroff(win, COLOR_PAIR(PAR_SOL) | A_BOLD);

    // 7. Ondas concéntricas de amerizaje sobre el mar azul (cy + 7)
    wattron(win, COLOR_PAIR(PAR_ESPUMA) | A_BOLD);
    if (wave_phase == 0) {
        mvwprintw(win, cy + 7, cx - 14, "%s", "( ( (   ≋ ≋ ≋ ≋ ≋ ≋   ) ) )");
    } else if (wave_phase == 1) {
        mvwprintw(win, cy + 7, cx - 15, "%s", "( ( ( (  ≈ ≈ ≈ ≈ ≈ ≈  ) ) ) )");
    } else {
        mvwprintw(win, cy + 7, cx - 16, "%s", " ( ( (    ~ ~ ~ ~ ~ ~    ) ) ) ");
    }
    wattroff(win, COLOR_PAIR(PAR_ESPUMA) | A_BOLD);
}

static void dibujar_sprite_rebote(WINDOW *win, int cy, int cx, int anim_tick) {
    int pulse = (anim_tick / 4) % 2;

    // Vector de escape orbital (sin etiqueta numérica de velocidad)
    wattron(win, COLOR_PAIR(PAR_AMARILLO_NEGRO) | A_BOLD);
    mvwprintw(win, cy - 4, cx - 11, "%s", pulse ? "▲ ESCAPE HELIOCÉNTRICO ▲" : "△ ESCAPE HELIOCÉNTRICO △");
    mvwprintw(win, cy - 3, cx + 1, "%s", "┃ ╱");
    mvwprintw(win, cy - 2, cx + 2, "%s", "┃╱");
    mvwprintw(win, cy - 1, cx + 3, "%s", "╱");
    wattroff(win, COLOR_PAIR(PAR_AMARILLO_NEGRO) | A_BOLD);

    // Cápsula inclinada / pérdida
    wattron(win, COLOR_PAIR(PAR_ESTRELLAS));
    mvwprintw(win, cy - 3, cx - 11, "%s", "╭━━━┻━━━╮");
    mvwprintw(win, cy - 2, cx - 12, "%s", "/ ( XX )  \\");
    mvwprintw(win, cy - 1, cx - 13, "%s", "/  LOST    \\");
    mvwprintw(win, cy,     cx - 14, "%s", "╭───────────╮");
    wattroff(win, COLOR_PAIR(PAR_ESTRELLAS));

    wattron(win, COLOR_PAIR(PAR_AMARILLO_NEGRO) | A_BOLD);
    mvwprintw(win, cy + 1, cx - 14, "%s", "╰━━▓▓▓▓▓▓▓━━╯");
    wattroff(win, COLOR_PAIR(PAR_AMARILLO_NEGRO) | A_BOLD);

    // Límite de la atmósfera Kármán (centrado en cx)
    wattron(win, COLOR_PAIR(PAR_AIRGLOW) | A_BOLD);
    mvwprintw(win, cy + 2, cx - 17, "%s", "─────── LÍNEA KÁRMÁN (80 km) ───────");
    wattroff(win, COLOR_PAIR(PAR_AIRGLOW) | A_BOLD);

    wattron(win, COLOR_PAIR(PAR_ATMOS_CELESTE));
    mvwprintw(win, cy + 3, cx - 17, "%s", "⠁⠁⠁⠁⠁⠁⠁⠁⠁⠁⠁⠁⠁⠁⠁⠁⠁⠁⠁⠁⠁⠁⠁⠁⠁⠁⠁⠁⠁⠁⠁⠁⠁⠁⠁⠁");
    mvwprintw(win, cy + 4, cx - 17, "%s", "⠕⠅⠕⠅⠕⠅⠕⠅⠕⠅⠕⠅⠕⠅⠕⠅⠕⠅⠕⠅⠕⠅⠕⠅⠕⠅⠕⠅⠕⠅⠕⠅⠕⠅⠕⠅");
    wattroff(win, COLOR_PAIR(PAR_ATMOS_CELESTE));
}

static void dibujar_sprite_fallo_termico(WINDOW *win, int cy, int cx, int anim_tick) {
    int spark = anim_tick % 2;

    wattron(win, COLOR_PAIR(PAR_PLASMA_FUEGO) | A_BOLD);
    mvwprintw(win, cy - 4, cx - 4,  "%s", spark ? "╲   ┃   ╱" : " ╲  ┃  ╱ ");
    mvwprintw(win, cy - 3, cx - 6,  "%s", spark ? "━  ✸  █  ✸  ━" : "━  ★  █  ★  ━");
    mvwprintw(win, cy - 2, cx - 8,  "%s", "/  ✸ █ █ █ █ ✸  \\");
    mvwprintw(win, cy - 1, cx - 11, "%s", "━ ╭ █ █ [BREACH] █ ╮ ━");
    mvwprintw(win, cy,     cx - 11, "%s", "( ( ( █ █ █ █ █ █ ) ) )");
    wattroff(win, COLOR_PAIR(PAR_PLASMA_FUEGO) | A_BOLD);

    wattron(win, COLOR_PAIR(PAR_ALERTA_CRITICA) | A_BOLD);
    mvwprintw(win, cy + 1, cx - 11, "%s", "═══ ░░░░░░░░░░░░░░░ ═══");
    wattroff(win, COLOR_PAIR(PAR_ALERTA_CRITICA) | A_BOLD);

    wattron(win, COLOR_PAIR(PAR_AMARILLO_NEGRO) | A_BOLD);
    mvwprintw(win, cy + 2, cx - 10, "%s", "( ( ж ж ж ж ж ж ж ) )");
    mvwprintw(win, cy + 3, cx - 8,  "%s", "╲ ╲ ╱ ╲ ╱ ╱ ╱");
    mvwprintw(win, cy + 4, cx - 7,  "%s", spark ? "DESINTEGRACIÓN" : "COLAPSO TÉRMICO");
    wattroff(win, COLOR_PAIR(PAR_AMARILLO_NEGRO) | A_BOLD);
}

static void dibujar_sprite_fallo_impacto(WINDOW *win, int cy, int cx, int anim_tick) {
    int wave = (anim_tick / 3) % 2;

    wattron(win, COLOR_PAIR(PAR_PLASMA_FUEGO) | A_BOLD);
    mvwprintw(win, cy - 4, cx - 4, "%s", "┆   ┆   ┆");
    mvwprintw(win, cy - 3, cx - 4, "%s", "▲   ▲   ▲");
    wattroff(win, COLOR_PAIR(PAR_PLASMA_FUEGO) | A_BOLD);

    wattron(win, COLOR_PAIR(PAR_ESTRELLAS) | A_BOLD);
    mvwprintw(win, cy - 2, cx - 4, "%s", "╭━━━┳━━━╮");
    mvwprintw(win, cy - 1, cx - 6, "%s", "/ ╲ │ ✖ ✖ │ ╱ \\");
    mvwprintw(win, cy,     cx - 9, "%s", "╭─────────────────╮");
    wattroff(win, COLOR_PAIR(PAR_ESTRELLAS) | A_BOLD);

    wattron(win, COLOR_PAIR(PAR_AMARILLO_NEGRO) | A_BOLD);
    mvwprintw(win, cy + 1, cx - 9, "%s", "╰━━━━▓▓▓▓▓▓▓▓━━━━━╯");
    wattroff(win, COLOR_PAIR(PAR_AMARILLO_NEGRO) | A_BOLD);

    wattron(win, COLOR_PAIR(PAR_ESPUMA) | A_BOLD);
    mvwprintw(win, cy + 2, cx - 13, "%s", wave ? "───  ≈≈≈≈≈≈≈≈≈≈≈≈≈  ───" : "───  ≋≋≋≋≋≋≋≋≋≋≋≋≋  ───");
    mvwprintw(win, cy + 3, cx - 11, "%s", "╱ ╱ ╱ █ █ █ █ █ █ ╲ ╲ ╲");
    mvwprintw(win, cy + 4, cx - 10, "%s", "IMPACTO FATAL: 41.5 G");
    wattroff(win, COLOR_PAIR(PAR_ESPUMA) | A_BOLD);
}

static void dibujar_capsula_estado(WINDOW *win, int cy, int cx, EstadoCapsulaVisual estado, int anim_tick) {
    switch (estado) {
        case ESTADO_CAP_ESPACIO:
            dibujar_sprite_espacio(win, cy, cx, anim_tick);
            break;
        case ESTADO_CAP_PLASMA:
            dibujar_sprite_plasma(win, cy, cx, anim_tick);
            break;
        case ESTADO_CAP_DROGUE:
            dibujar_sprite_drogue(win, cy, cx, anim_tick);
            break;
        case ESTADO_CAP_PARACAIDAS:
            dibujar_sprite_paracaidas(win, cy, cx, anim_tick);
            break;
        case ESTADO_CAP_AMERIZAJE:
            dibujar_sprite_amerizaje(win, cy, cx, anim_tick);
            break;
        case ESTADO_CAP_REBOTE:
            dibujar_sprite_rebote(win, cy, cx, anim_tick);
            break;
        case ESTADO_CAP_FALLO_TERMICO:
            dibujar_sprite_fallo_termico(win, cy, cx, anim_tick);
            break;
        case ESTADO_CAP_FALLO_IMPACTO:
            dibujar_sprite_fallo_impacto(win, cy, cx, anim_tick);
            break;
        default:
            break;
    }
}

static EstadoCapsulaVisual determinar_estado_altitud(double altitud, int modo_fallo, int paracaidas_abiertos) {
    if (modo_fallo == 1) return ESTADO_CAP_REBOTE;
    if (modo_fallo == 2) return ESTADO_CAP_FALLO_TERMICO;
    if (modo_fallo == 3) return ESTADO_CAP_FALLO_IMPACTO;

    if (altitud > 75000.0) return ESTADO_CAP_ESPACIO;
    if (altitud > 18000.0) return ESTADO_CAP_PLASMA;
    if (altitud > 3000.0)  return ESTADO_CAP_DROGUE;
    if (altitud > 0.0) {
        return paracaidas_abiertos ? ESTADO_CAP_PARACAIDAS : ESTADO_CAP_DROGUE;
    }
    return ESTADO_CAP_AMERIZAJE;
}

static void calcular_telemetria_vuelo(double altitud, int modo_fallo,
                                     double *v_out, double *t_out, double *esc_out, double *g_out) {
    if (modo_fallo == 1) { // Rebote atmosférico
        *v_out = 10850.0;
        *t_out = 450.0;
        *esc_out = 98.0;
        *g_out = 1.4;
        return;
    }
    if (modo_fallo == 2) { // Colapso térmico
        *v_out = 8900.0;
        *t_out = 3950.0;
        *esc_out = 0.0;
        *g_out = 16.8;
        return;
    }
    if (modo_fallo == 3) { // Impacto violento
        *v_out = (altitud <= 0.0) ? 0.0 : 110.0;
        *t_out = 40.0;
        *esc_out = 0.0;
        *g_out = 41.5;
        return;
    }

    // Vuelo nominal: interpolación suave continua según altitud
    if (altitud >= 75000.0) {
        double f = (120000.0 - altitud) / 45000.0;
        if (f < 0.0) { f = 0.0; }
        if (f > 1.0) { f = 1.0; }
        *v_out = 11000.0 - f * 500.0;
        *t_out = 25.0 + f * 155.0;
        *esc_out = 100.0;
        *g_out = f * 0.8;
    } else if (altitud >= 18000.0) {
        double f = (75000.0 - altitud) / 57000.0;
        if (f < 0.0) { f = 0.0; }
        if (f > 1.0) { f = 1.0; }
        *v_out = 10500.0 - f * 9600.0;
        double campana = sin(f * 3.14159265);
        *t_out = 180.0 + campana * 2580.0;
        *esc_out = 100.0 - f * 22.0;
        *g_out = 0.8 + campana * 7.7;
    } else if (altitud >= 3000.0) {
        double f = (18000.0 - altitud) / 15000.0;
        if (f < 0.0) { f = 0.0; }
        if (f > 1.0) { f = 1.0; }
        *v_out = 900.0 - f * 780.0;
        *t_out = 1100.0 - f * 850.0;
        *esc_out = 78.0 - f * 2.0;
        *g_out = 3.0 - f * 1.5;
    } else if (altitud > 0.0) {
        double f = (3000.0 - altitud) / 3000.0;
        if (f < 0.0) { f = 0.0; }
        if (f > 1.0) { f = 1.0; }
        *v_out = 120.0 - f * 111.8;
        *t_out = 250.0 - f * 205.0;
        *esc_out = 76.0;
        *g_out = 1.5 - f * 0.5;
    } else {
        *v_out = 0.0;
        *t_out = 22.0;
        *esc_out = 76.0;
        *g_out = 0.0;
    }
}

static void dibujar_tarjeta_telemetria(WINDOW *win, int max_y, int max_x, const InfoEstadoCapsula *info,
                                      double altitud, double velocidad, double temp_escudo, double integridad, double carga_g) {
    int ancho_caja = max_x - 4;
    int alto_caja  = 6;
    int y_caja     = max_y - alto_caja - 2;
    int x_caja     = 2;

    if (ancho_caja < 60 || y_caja < 6) return;

    // 1. RELLENAR COMPLETAMENTE DE NEGRO TODA LA CAJA (fondo 100% opaco, sin filtraciones)
    wattron(win, COLOR_PAIR(PAR_ESTRELLAS));
    for (int y = y_caja; y < y_caja + alto_caja; y++) {
        for (int x = x_caja; x < x_caja + ancho_caja; x++) {
            mvwaddch(win, y, x, ' ');
        }
    }
    wattroff(win, COLOR_PAIR(PAR_ESTRELLAS));

    // 2. Marco exterior de la tarjeta
    wattron(win, COLOR_PAIR(PAR_ESTRELLAS) | A_BOLD);
    mvwprintw(win, y_caja, x_caja, "%s", "┌── TELEMETRÍA Y DINÁMICA DE VUELO EN TIEMPO REAL ");
    for (int x = x_caja + 50; x < x_caja + ancho_caja - 1; x++) mvwprintw(win, y_caja, x, "%s", "─");
    mvwprintw(win, y_caja, x_caja + ancho_caja - 1, "%s", "┐");

    for (int y = y_caja + 1; y < y_caja + alto_caja - 1; y++) {
        mvwprintw(win, y, x_caja, "%s", "│");
        mvwprintw(win, y, x_caja + ancho_caja - 1, "%s", "│");
    }
    mvwprintw(win, y_caja + alto_caja - 1, x_caja, "%s", "└");
    for (int x = x_caja + 1; x < x_caja + ancho_caja - 1; x++) mvwprintw(win, y_caja + alto_caja - 1, x, "%s", "─");
    mvwprintw(win, y_caja + alto_caja - 1, x_caja + ancho_caja - 1, "%s", "┘");
    wattroff(win, COLOR_PAIR(PAR_ESTRELLAS) | A_BOLD);

    // Fila 1: Fase y condición
    wattron(win, COLOR_PAIR(PAR_ESTRELLAS) | A_BOLD);
    mvwprintw(win, y_caja + 1, x_caja + 2, "Fase: %-26s │ Condición: %-30s", info->fase_sim, info->condicion);
    wattroff(win, COLOR_PAIR(PAR_ESTRELLAS) | A_BOLD);

    // Fila 2: Métricas cuantitativas continuas en tiempo real
    wattron(win, COLOR_PAIR(PAR_ESTRELLAS));
    mvwprintw(win, y_caja + 2, x_caja + 2, "Alt: %5.1f km │ Vel: %5.0f m/s (%5.0f km/h) │ Temp: %4.0f°C │ Escudo: %3.0f%% │ G: %4.1f",
              altitud / 1000.0,
              velocidad,
              velocidad * 3.6,
              temp_escudo,
              integridad,
              carga_g);
    wattroff(win, COLOR_PAIR(PAR_ESTRELLAS));

    // Fila 3: Descripción visual
    wattron(win, COLOR_PAIR(PAR_CYAN_NEGRO));
    mvwprintw(win, y_caja + 3, x_caja + 2, "Arte : %-70.70s", info->descripcion);
    wattroff(win, COLOR_PAIR(PAR_CYAN_NEGRO));

    // Fila 4: Física de la Situación 4
    wattron(win, COLOR_PAIR(PAR_AMARILLO_NEGRO));
    mvwprintw(win, y_caja + 4, x_caja + 2, "Física: %-70.70s", info->analisis_fisico);
    wattroff(win, COLOR_PAIR(PAR_AMARILLO_NEGRO));
}

void tui_test_capsula(void) {
    WINDOW *win = newwin(LINES, COLS, 0, 0);
    if (!win) return;

    keypad(win, TRUE);
    wtimeout(win, 40); // 25 FPS para animación suave continua

    double altitud = 120000.0;
    double velocidad = 11000.0;
    double temp_escudo = 25.0;
    double integridad = 100.0;
    double carga_g = 0.0;

    int fondo_real      = 1;   // 1 = renderiza atmósfera/mar de fondo, 0 = fondo negro puro
    int ver_ficha       = 1;   // 1 = muestra tarjeta de telemetría inferior
    int en_pausa        = 0;   // 0 = reproduciendo animación, 1 = pausado
    double mult_vel     = 1.0; // Multiplicador de velocidad de reproducción (0.5x, 1x, 2x, 4x)
    int modo_fallo      = 0;   // 0 = Nominal, 1 = Rebote, 2 = Colapso térmico, 3 = Impacto fatal
    int anim_tick       = 0;
    int salir           = 0;

    while (!salir) {
        werase(win);

        int max_y, max_x;
        getmaxyx(win, max_y, max_x);

        // 1. Determinar estado visual y físicas según la altitud actual
        EstadoCapsulaVisual estado = determinar_estado_altitud(altitud, modo_fallo, 1);
        const InfoEstadoCapsula *info = &INFO_ESTADOS[estado];
        calcular_telemetria_vuelo(altitud, modo_fallo, &velocidad, &temp_escudo, &integridad, &carga_g);

        // 2. Geometría vertical fija continua para todos los estados
        int y_suelo = ver_ficha ? (max_y - 8) : (max_y - 2);
        int y_techo = 2;
        int h_disp  = y_suelo - y_techo;
        int cy = y_techo + h_disp / 2;
        if (cy < 9) cy = 9;
        int cx = max_x / 2;
        int y_agua_contacto = cy + 4; // Línea de flotación exacta de la cápsula

        // 3. Fondo dinámico interactivo con nivel del agua coordinado
        if (fondo_real) {
            rellenar_fondo_escena_viewport(win, altitud, y_suelo, y_agua_contacto);
        }

        // 4. Marco exterior de la ventana
        box(win, 0, 0);

        // 5. HUD superior
        wattron(win, A_REVERSE | A_BOLD);
        for (int x = 1; x < max_x - 1; x++) {
            mvwaddch(win, 0, x, ' ');
            mvwaddch(win, 1, x, ' ');
        }
        mvwprintw(win, 0, 2, " [ SIMULADOR CINEMÁTICO: REINGRESO CÁPSULA ORION ] ");
        mvwprintw(win, 1, 2, " Estado [%d/%d]: %-30s │ Alt: %6.1f km │ %-12s ",
                  estado + 1,
                  TOTAL_ESTADOS_CAP,
                  info->titulo,
                  altitud / 1000.0,
                  en_pausa ? "[PAUSA]" : "[ANIMANDO]");
        wattroff(win, A_REVERSE | A_BOLD);

        // 6. Dibujar sprite animado del estado con Unicode
        dibujar_capsula_estado(win, cy, cx, estado, anim_tick);

        // 7. Tarjeta inferior de telemetría y física en tiempo real
        if (ver_ficha) {
            dibujar_tarjeta_telemetria(win, max_y, max_x, info, altitud, velocidad, temp_escudo, integridad, carga_g);
        }

        // 8. Barra de controles de animación
        wattron(win, A_REVERSE);
        for (int x = 1; x < max_x - 1; x++) mvwaddch(win, max_y - 1, x, ' ');
        mvwprintw(win, max_y - 1, 2,
                  " [ESPACIO] %s  [↑/↓] Vel (%.1fx)  [1-5] Fase  [6-8] Fallas  [R] Reset  [B] Fondo  [Q] Menú ",
                  en_pausa ? "Reanudar" : "Pausar",
                  mult_vel);
        wattroff(win, A_REVERSE);

        wrefresh(win);

        // 9. Captura de teclas no bloqueante (40 ms timeout)
        int tecla = wgetch(win);
        switch (tecla) {
            case ' ':
                en_pausa = !en_pausa;
                break;
            case KEY_UP:
            case '+':
            case '=':
                if (mult_vel < 4.0) mult_vel *= 2.0;
                break;
            case KEY_DOWN:
            case '-':
            case '_':
                if (mult_vel > 0.25) mult_vel /= 2.0;
                break;
            case KEY_LEFT:
            case 'a':
            case 'A':
                altitud += (altitud < 5000.0) ? 250.0 : 5000.0;
                if (altitud > 120000.0) altitud = 120000.0;
                modo_fallo = 0;
                break;
            case KEY_RIGHT:
            case 'd':
            case 'D':
                altitud -= (altitud <= 5000.0) ? 250.0 : 5000.0;
                if (altitud < 0.0) altitud = 0.0;
                modo_fallo = 0;
                break;
            case '1':
                altitud = 120000.0;
                modo_fallo = 0;
                break;
            case '2':
                altitud = 55000.0;
                modo_fallo = 0;
                break;
            case '3':
                altitud = 12000.0;
                modo_fallo = 0;
                break;
            case '4':
                altitud = 2000.0;
                modo_fallo = 0;
                break;
            case '5':
                altitud = 0.0;
                modo_fallo = 0;
                break;
            case '6':
                modo_fallo = 1; // Rebote
                altitud = 85000.0;
                break;
            case '7':
                modo_fallo = 2; // Colapso térmico
                altitud = 45000.0;
                break;
            case '8':
                modo_fallo = 3; // Impacto fatal
                altitud = 0.0;
                break;
            case 'b':
            case 'B':
                fondo_real = !fondo_real;
                break;
            case 't':
            case 'T':
                ver_ficha = !ver_ficha;
                break;
            case 'r':
            case 'R':
                altitud = 120000.0;
                modo_fallo = 0;
                en_pausa = 0;
                semilla_universo = (unsigned int)rand() ^ (unsigned int)time(NULL);
                break;
            case 'q':
            case 'Q':
            case 27:
                salir = 1;
                break;
            default:
                break;
        }

        // 10. Actualización física del descenso cuando no está en pausa
        if (!en_pausa) {
            anim_tick++;

            if (modo_fallo == 0) {
                // Tasa de descenso adaptativa según capa para una cinemática fluida
                double tasa_descenso;
                if (altitud > 75000.0) {
                    tasa_descenso = 3200.0;
                } else if (altitud > 18000.0) {
                    tasa_descenso = 2400.0;
                } else if (altitud > 3000.0) {
                    tasa_descenso = 1200.0;
                } else if (altitud > 100.0) {
                    tasa_descenso = 400.0;
                } else {
                    tasa_descenso = 80.0;
                }

                altitud -= tasa_descenso * 0.040 * mult_vel;
                if (altitud < 0.0) altitud = 0.0;
            } else if (modo_fallo == 1) {
                // En rebote, la nave asciende escapando al espacio
                altitud += 2500.0 * 0.040 * mult_vel;
                if (altitud > 120000.0) altitud = 120000.0;
            }
        }
    }

    wtimeout(win, -1); // Restaurar modo bloqueante
    delwin(win);
    clear();
    refresh();
}


// ============================================================================
// FUNCIONES DE LAS OPCIONES DEL MENÚ MIGRARADAS A TUI
// ============================================================================

static const char *nombre_fase_mision(FaseVuelo fase) {
    switch (fase) {
        case FASE_ENTRADA_ATMOSFERICA: return "Entrada Atmosferica";
        case FASE_FRENADO_HIPERSONICO: return "Frenado Hipersonico";
        case FASE_PARACAIDAS:          return "Descenso Paracaidas";
        case FASE_AMERIZAJE:           return "Amerizaje en Oceano";
        case FASE_FINALIZADA:          return "Mision Finalizada";
        default:                       return "Desconocida";
    }
}

// ----------------------------------------------------------------------------
// 1. SIMULACIÓN INTERACTIVA (MODO PILOTO)
// ----------------------------------------------------------------------------
static void dibujar_linea_h(WINDOW *win, int y, int x1, int x2) {
    for (int x = x1; x <= x2; x++) {
        mvwprintw(win, y, x, "─");
    }
}

void tui_simulacion_interactiva(MisionArtemis *mision) {
    if (mision == NULL) return;

    simulador_inicializar(mision);

    WINDOW *win = newwin(LINES, COLS, 0, 0);
    keypad(win, TRUE);
    wtimeout(win, 35); // ~28 FPS para cinematica en tiempo real

    double angulo_guia = 6.2;
    double mult_ritmo = 1.0;
    int en_pausa = 0;
    int modo_fallo = 0;
    int anim_tick = 0;
    int salir = 0;
    int corridor_breached_steep = 0;
    int corridor_breached_shallow = 0;

    double tiempo_acumulado = 0.0;
    double ultimo_registro_tiempo = 0.0;
    char ultimo_evento[TAM_EVENTO] = "Orbita de aproximacion lunar. Ventana segura de reingreso: 5.50 a 7.00 deg";

    while (!salir) {
        int max_y, max_x;
        getmaxyx(win, max_y, max_x);

        // 1. Sprite de estado segun altitud fisica, apertura de paracaidas y modo de fallo
        EstadoCapsulaVisual estado = determinar_estado_altitud(mision->altitud, modo_fallo, mision->paracaidas_desplegados);

        // 2. Geometria vertical fija continua para todos los estados
        int y_suelo = max_y - 9;
        if (y_suelo < 14) y_suelo = 14;
        int cy = (y_suelo > 18) ? (y_suelo / 2) : 9;
        int cx = max_x / 2;
        int y_amerizaje = cy + 4; // Coincide exactamente con la linea de flotacion (cy + 4)

        werase(win);

        // 3. Renderizar fondo atmosferico continuo y oceano
        rellenar_fondo_escena_viewport(win, mision->altitud, y_suelo, y_amerizaje);

        // 4. Renderizar sprite de la capsula Orion con animacion
        dibujar_capsula_estado(win, cy, cx, estado, anim_tick);

        // 5. Barra superior de navegacion e informacion
        wattron(win, A_REVERSE | A_BOLD);
        for (int x = 0; x < max_x; x++) mvwaddch(win, 0, x, ' ');
        mvwprintw(win, 0, 2, " MISION ARTEMIS IV | PILOTO EN TIEMPO REAL | Fase: %s ",
                  nombre_fase_mision(mision->fase_actual));
        mvwprintw(win, 0, max_x - 42, " gamma: %4.2f deg | Escudo: %5.1f%% | T: %4.1fs | %s ",
                  angulo_guia, mision->integridad_escudo, tiempo_acumulado,
                  en_pausa ? "[PAUSA]" : "[VUELO]");
        wattroff(win, A_REVERSE | A_BOLD);

        // 6. Tarjeta HUD de telemetria inferior con cuadricula matematica exacta (linea fina)
        int ancho_hud = 82;
        int x0 = (max_x - ancho_hud) / 2;
        if (x0 < 1) x0 = 1;
        int x1 = x0 + 26;
        int x2 = x0 + 53;
        int x3 = x0 + 81;
        int y_hud = max_y - 9;

        // Rellenar de negro absoluto el fondo de la tabla de telemetria
        wattron(win, COLOR_PAIR(PAR_DEFECTO));
        for (int y = y_hud; y <= y_hud + 6; y++) {
            for (int x = x0; x <= x3; x++) {
                mvwaddch(win, y, x, ' ');
            }
        }
        wattroff(win, COLOR_PAIR(PAR_DEFECTO));

        wattron(win, A_BOLD);

        // Borde superior continuo con uniones finas
        mvwprintw(win, y_hud, x0, "┌");
        dibujar_linea_h(win, y_hud, x0 + 1, x1 - 1);
        mvwprintw(win, y_hud, x1, "┬");
        dibujar_linea_h(win, y_hud, x1 + 1, x2 - 1);
        mvwprintw(win, y_hud, x2, "┬");
        dibujar_linea_h(win, y_hud, x2 + 1, x3 - 1);
        mvwprintw(win, y_hud, x3, "┐");

        // Titulos de seccion sobre el borde superior
        mvwprintw(win, y_hud, x0 + 3, " DINAMICA DE VUELO ");
        mvwprintw(win, y_hud, x1 + 3, " ESCUDO Y TERMAL ");
        mvwprintw(win, y_hud, x2 + 3, " ENERGIA Y CARGAS ");

        // Columnas verticales de las 3 filas de datos
        for (int y = y_hud + 1; y <= y_hud + 3; y++) {
            mvwprintw(win, y, x0, "│");
            mvwprintw(win, y, x1, "│");
            mvwprintw(win, y, x2, "│");
            mvwprintw(win, y, x3, "│");
        }

        // Fila separadora intermedia
        mvwprintw(win, y_hud + 4, x0, "├");
        dibujar_linea_h(win, y_hud + 4, x0 + 1, x1 - 1);
        mvwprintw(win, y_hud + 4, x1, "┴");
        dibujar_linea_h(win, y_hud + 4, x1 + 1, x2 - 1);
        mvwprintw(win, y_hud + 4, x2, "┴");
        dibujar_linea_h(win, y_hud + 4, x2 + 1, x3 - 1);
        mvwprintw(win, y_hud + 4, x3, "┤");

        // Fila de evento
        mvwprintw(win, y_hud + 5, x0, "│");
        mvwprintw(win, y_hud + 5, x3, "│");

        // Borde inferior
        mvwprintw(win, y_hud + 6, x0, "└");
        dibujar_linea_h(win, y_hud + 6, x0 + 1, x3 - 1);
        mvwprintw(win, y_hud + 6, x3, "┘");

        // Datos Columna 1
        mvwprintw(win, y_hud + 1, x0 + 2, "Altitud:   %7.0f m", mision->altitud);
        mvwprintw(win, y_hud + 2, x0 + 2, "Rapidez:   %7.1f m/s", mision->velocidad);
        mvwprintw(win, y_hud + 3, x0 + 2, "Regimen:   Mach %-5.1f", mision->velocidad / 343.0);

        // Datos Columna 2
        mvwprintw(win, y_hud + 1, x1 + 2, "Escudo:    %5.1f%%", mision->integridad_escudo);
        mvwprintw(win, y_hud + 2, x1 + 2, "Temp:      %5.0f C", mision->temperatura_escudo);
        mvwprintw(win, y_hud + 3, x1 + 2, "Angulo gamma: %4.2f deg", angulo_guia);

        // Datos Columna 3
        mvwprintw(win, y_hud + 1, x2 + 2, "Trabajo: %10.2e J", mision->trabajo_rozamiento);
        mvwprintw(win, y_hud + 2, x2 + 2, "Carga G:   %6.2f g", mision->desaceleracion_max_g);
        mvwprintw(win, y_hud + 3, x2 + 2, "Paracaidas: %-11s", mision->paracaidas_desplegados ? "DESPLEGADOS" : "EN ESPERA");

        // Evento
        mvwprintw(win, y_hud + 5, x0 + 2, "Registro: %-68.68s", ultimo_evento);

        wattroff(win, A_BOLD);

        // 7. Barra de comandos inferior
        wattron(win, A_REVERSE);
        for (int x = 0; x < max_x; x++) {
            mvwaddch(win, max_y - 2, x, ' ');
            mvwaddch(win, max_y - 1, x, ' ');
        }

        if (mision->resultado == MISION_EN_CURSO) {
            if (mision->altitud > 10000.0) {
                mvwprintw(win, max_y - 2, 2, " VUELO EN TIEMPO REAL: [<-/->] Variar Gamma (%.2f deg)   [ESPACIO] %s   [Arriba/Abajo] Ritmo (%.1fx)",
                          angulo_guia, en_pausa ? "Reanudar" : "Pausar", mult_ritmo);
                mvwprintw(win, max_y - 1, 2, " CONTROL: Ventana segura [5.50 - 7.00 deg]   [E] Abrir Paracaidas (esperar baja cota)   [Q] Abortar");
            } else if (!mision->paracaidas_desplegados) {
                wattron(win, COLOR_PAIR(PAR_ALERTA_CRITICA) | A_BOLD);
                mvwprintw(win, max_y - 2, 2, " [!] ZONA DE DESPLIEGUE (h = %.0f m): Presione [E] para ABRIR PARACAIDAS", mision->altitud);
                wattroff(win, COLOR_PAIR(PAR_ALERTA_CRITICA) | A_BOLD);
                mvwprintw(win, max_y - 1, 2, " [E] ABRIR PARACAIDAS AHORA!   [<-/->] Corregir Trayectoria   [ESPACIO] %s   [Q] Abortar", en_pausa ? "Reanudar" : "Pausar");
            } else {
                wattron(win, COLOR_PAIR(PAR_EXITO_VERDE) | A_BOLD);
                mvwprintw(win, max_y - 2, 2, " [*] PARACAIDAS ACTIVADOS: Descenso suave hacia el Oceano Pacifico (terminal 8.0 m/s)");
                wattroff(win, COLOR_PAIR(PAR_EXITO_VERDE) | A_BOLD);
                mvwprintw(win, max_y - 1, 2, " Aproximando al agua...   [ESPACIO] %s   [Arriba/Abajo] Ritmo (%.1fx)   [Q] Abortar", en_pausa ? "Reanudar" : "Pausar", mult_ritmo);
            }
        } else {
            mvwprintw(win, max_y - 2, 2, " MISION FINALIZADA: %s", mision->causa_fallo);
            mvwprintw(win, max_y - 1, 2, " [ENTER] Ver Reporte de Telemetria   [R] Reintentar Mision   [Q] Menu Principal");
        }
        wattroff(win, A_REVERSE);

        // 8. Ventana modal al finalizar la mision
        if (mision->resultado != MISION_EN_CURSO) {
            int diag_ancho = 72;
            int diag_alto  = 13;
            int mx0 = (max_x - diag_ancho) / 2;
            if (mx0 < 1) mx0 = 1;
            int mx1 = mx0 + 36;
            int mx2 = mx0 + diag_ancho - 1;
            int my  = (y_suelo - diag_alto) / 2;
            if (my < 1) my = 1;

            int color_borde = (mision->resultado == MISION_EXITOSA) ? PAR_EXITO_VERDE : PAR_ALERTA_CRITICA;

            wattron(win, COLOR_PAIR(PAR_DEFECTO));
            for (int y = my; y < my + diag_alto; y++) {
                for (int x = mx0; x <= mx2; x++) {
                    mvwaddch(win, y, x, ' ');
                }
            }
            wattroff(win, COLOR_PAIR(PAR_DEFECTO));

            wattron(win, COLOR_PAIR(color_borde) | A_BOLD);
            mvwprintw(win, my, mx0, "┌");
            dibujar_linea_h(win, my, mx0 + 1, mx2 - 1);
            mvwprintw(win, my, mx2, "┐");

            mvwprintw(win, my + 1, mx0, "│");
            mvwprintw(win, my + 1, mx2, "│");
            if (mision->resultado == MISION_EXITOSA) {
                mvwprintw(win, my + 1, mx0 + 6, "*** MISION EXITOSA! AMERIZAJE CONFIRMADO EN EL PACIFICO ***");
            } else {
                mvwprintw(win, my + 1, mx0 + 12, "XXX INFORME DE INCIDENTE: MISION FALLIDA XXX");
            }

            mvwprintw(win, my + 2, mx0, "├");
            dibujar_linea_h(win, my + 2, mx0 + 1, mx1 - 1);
            mvwprintw(win, my + 2, mx1, "┬");
            dibujar_linea_h(win, my + 2, mx1 + 1, mx2 - 1);
            mvwprintw(win, my + 2, mx2, "┤");
            wattroff(win, COLOR_PAIR(color_borde) | A_BOLD);

            for (int y = my + 3; y <= my + 4; y++) {
                mvwprintw(win, y, mx0, "│");
                mvwprintw(win, y, mx1, "│");
                mvwprintw(win, y, mx2, "│");
            }
            wattron(win, A_BOLD);
            mvwprintw(win, my + 3, mx0 + 2, "Diagnostico de Mision");
            mvwprintw(win, my + 3, mx1 + 2, "Causa Primaria");
            wattroff(win, A_BOLD);
            mvwprintw(win, my + 4, mx0 + 2, "%-32.32s", (mision->resultado == MISION_EXITOSA) ? "Amerizaje Nominal Completado" : "Fallo Catastrofico de Vuelo");
            mvwprintw(win, my + 4, mx1 + 2, "%-32.32s", mision->causa_fallo);

            mvwprintw(win, my + 5, mx0, "├");
            dibujar_linea_h(win, my + 5, mx0 + 1, mx1 - 1);
            mvwprintw(win, my + 5, mx1, "┼");
            dibujar_linea_h(win, my + 5, mx1 + 1, mx2 - 1);
            mvwprintw(win, my + 5, mx2, "┤");

            for (int y = my + 6; y <= my + 9; y++) {
                mvwprintw(win, y, mx0, "│");
                mvwprintw(win, y, mx1, "│");
                mvwprintw(win, y, mx2, "│");
            }
            mvwprintw(win, my + 6, mx0 + 2, "Tiempo Total Vuelo: %-7.1f s", tiempo_acumulado);
            mvwprintw(win, my + 6, mx1 + 2, "Trabajo W_roz: %10.3e J", mision->trabajo_rozamiento);

            mvwprintw(win, my + 7, mx0 + 2, "Desaceleracion Med: %-5.2f m/s2", fabs(mision->desaceleracion_media));
            mvwprintw(win, my + 7, mx1 + 2, "Carga G Maxima:   %6.2f g", mision->desaceleracion_max_g);

            mvwprintw(win, my + 8, mx0 + 2, "Salud Final Escudo: %-5.0f %%", mision->integridad_escudo);
            mvwprintw(win, my + 8, mx1 + 2, "Rapidez Impacto:     %5.1f m/s", mision->velocidad);

            if (mision->altitud <= 0.0 && mision->resultado == MISION_EXITOSA) {
                mvwprintw(win, my + 9, mx0 + 2, "Fuerza del Agua:    %-7.0f N", mision->fuerza_impacto_n);
                mvwprintw(win, my + 9, mx1 + 2, "Carga de Choque:    %5.2f x P", mision->fuerza_impacto_multiplo_peso);
            } else {
                mvwprintw(win, my + 9, mx0 + 2, "Estado Capsula:     %-10s", "Destruida");
                mvwprintw(win, my + 9, mx1 + 2, "Supervivencia:      0%%");
            }

            mvwprintw(win, my + 10, mx0, "├");
            dibujar_linea_h(win, my + 10, mx0 + 1, mx1 - 1);
            mvwprintw(win, my + 10, mx1, "┴");
            dibujar_linea_h(win, my + 10, mx1 + 1, mx2 - 1);
            mvwprintw(win, my + 10, mx2, "┤");

            wattron(win, A_REVERSE | A_BOLD);
            mvwprintw(win, my + 11, mx0, "│");
            for (int x = mx0 + 1; x < mx2; x++) mvwaddch(win, my + 11, x, ' ');
            mvwprintw(win, my + 11, mx0 + 6, "[ENTER] Ver Telemetria    [R] Reintentar    [Q] Menu Principal");
            mvwprintw(win, my + 11, mx2, "│");
            wattroff(win, A_REVERSE | A_BOLD);

            wattron(win, COLOR_PAIR(color_borde) | A_BOLD);
            mvwprintw(win, my + 12, mx0, "└");
            dibujar_linea_h(win, my + 12, mx0 + 1, mx2 - 1);
            mvwprintw(win, my + 12, mx2, "┘");
            wattroff(win, COLOR_PAIR(color_borde) | A_BOLD);
        }

        wrefresh(win);

        // 9. Captura de teclas y comandos
        int tecla = wgetch(win);

        if (mision->resultado != MISION_EN_CURSO) {
            switch (tecla) {
                case 10:
                case 13:
                case KEY_ENTER:
                    tui_reporte_telemetria(mision);
                    salir = 1;
                    break;
                case 'r':
                case 'R':
                    simulador_inicializar(mision);
                    angulo_guia = 6.2;
                    mult_ritmo = 1.0;
                    en_pausa = 0;
                    modo_fallo = 0;
                    corridor_breached_steep = 0;
                    corridor_breached_shallow = 0;
                    tiempo_acumulado = 0.0;
                    ultimo_registro_tiempo = 0.0;
                    strcpy(ultimo_evento, "Simulacion reiniciada en tiempo real. Sistemas en espera.");
                    break;
                case 'q':
                case 'Q':
                case 27:
                    salir = 1;
                    break;
                default:
                    break;
            }
        } else {
            switch (tecla) {
                case ' ':
                    en_pausa = !en_pausa;
                    break;
                case KEY_UP:
                case '+':
                case '=':
                    if (mult_ritmo < 2.5) mult_ritmo += 0.25;
                    break;
                case KEY_DOWN:
                case '-':
                case '_':
                    if (mult_ritmo > 0.5) mult_ritmo -= 0.25;
                    break;
                case KEY_LEFT:
                case 'a':
                case 'A':
                    if (angulo_guia > 1.0) angulo_guia -= 0.05;
                    break;
                case KEY_RIGHT:
                case 'd':
                case 'D':
                    if (angulo_guia < 89.0) angulo_guia += 0.05;
                    break;
                case 'e':
                case 'E':
                    if (!mision->paracaidas_desplegados) {
                        if (mision->velocidad > 200.0 || mision->altitud > 15000.0) {
                            mision->integridad_escudo -= 40.0;
                            strcpy(ultimo_evento, "ALERTA CRITICA: Paracaidas rasgados por exceso de velocidad!");
                        } else {
                            mision->paracaidas_desplegados = 1;
                            strcpy(ultimo_evento, "DESPLIEGUE CONFIRMADO: 3 paracaidas principales abiertos.");
                        }
                    }
                    break;
                case 'q':
                case 'Q':
                case 27:
                    salir = 1;
                    break;
                default:
                    break;
            }
        }

        // 10. Actualizacion cinematica y fisica continua en tiempo real
        if (!en_pausa && mision->resultado == MISION_EN_CURSO) {
            double dt_frame = 0.035 * mult_ritmo;
            tiempo_acumulado += dt_frame;
            mision->angulo_reingreso = angulo_guia;

            double dh_base;
            double v_target;
            double g_base;

            if (mision->altitud > 75000.0) {
                dh_base = 2200.0;
                v_target = 11000.0 - (120000.0 - mision->altitud) * 0.02;
                g_base = 0.2 + (120000.0 - mision->altitud) / 45000.0 * 0.8;
                mision->temperatura_escudo = 20.0 + (120000.0 - mision->altitud) * 0.02;
                if (mision->integridad_escudo > 98.0) mision->integridad_escudo -= 0.02 * dt_frame;
            } else if (mision->altitud > 40000.0) {
                dh_base = 1800.0;
                v_target = 10000.0 - (75000.0 - mision->altitud) * 0.20;
                g_base = 1.0 + (75000.0 - mision->altitud) / 35000.0 * 4.2;
                mision->temperatura_escudo = 900.0 + (75000.0 - mision->altitud) / 35000.0 * 1700.0;
                mision->integridad_escudo -= 0.25 * dt_frame;
            } else if (mision->altitud > 15000.0) {
                dh_base = 1400.0;
                v_target = 3000.0 - (40000.0 - mision->altitud) * 0.10;
                g_base = 5.2 - (40000.0 - mision->altitud) / 25000.0 * 3.8;
                mision->temperatura_escudo = 2600.0 - (40000.0 - mision->altitud) / 25000.0 * 1800.0;
                mision->integridad_escudo -= 0.10 * dt_frame;
            } else if (mision->altitud > 2500.0) {
                dh_base = 900.0;
                v_target = 500.0 - (15000.0 - mision->altitud) * 0.036;
                g_base = 1.4 - (15000.0 - mision->altitud) / 12500.0 * 0.6;
                mision->temperatura_escudo = 800.0 - (15000.0 - mision->altitud) / 12500.0 * 600.0;
            } else {
                if (mision->paracaidas_desplegados) {
                    dh_base = (mision->altitud > 500.0) ? 180.0 : 40.0;
                    v_target = 25.0 - (2500.0 - mision->altitud) / 2500.0 * 17.0;
                    g_base = (mision->altitud > 2000.0) ? 1.4 : 1.0;
                    mision->temperatura_escudo = 80.0;
                } else {
                    dh_base = 950.0;
                    v_target = 110.0;
                    g_base = 1.0;
                }
            }

            double dh_actual = dh_base;
            double g_actual = g_base;

            if (mision->altitud <= ALTITUD_FIN_ENTRADA_M) {
                if (angulo_guia > ANGULO_REINGRESO_MAX) corridor_breached_steep = 1;
                else if (angulo_guia < ANGULO_REINGRESO_MIN) corridor_breached_shallow = 1;
            }

            if (angulo_guia > ANGULO_REINGRESO_MAX) {
                corridor_breached_steep = 1;
                double exceso = angulo_guia - ANGULO_REINGRESO_MAX;
                // En reingreso escarpado, la deceleración y el calor aumentan violentamente
                g_actual += exceso * 28.0 + (exceso * exceso) * 65.0;
                dh_actual *= (1.0 + exceso * 0.9);
                mision->temperatura_escudo += exceso * 1250.0;
                mision->integridad_escudo -= (5.0 + exceso * 32.0) * dt_frame;
                snprintf(ultimo_evento, sizeof(ultimo_evento),
                         "CRITICO: Reingreso empinado (gamma=%.2f deg > 7.0). Gs y calor letales!", angulo_guia);
            } else if (angulo_guia < ANGULO_REINGRESO_MIN) {
                corridor_breached_shallow = 1;
                double defecto = ANGULO_REINGRESO_MIN - angulo_guia;
                if (mision->altitud > 45000.0) {
                    // Rebote atmosférico (skip-out): la nave es expulsada hacia el espacio
                    dh_actual = -2500.0;
                    modo_fallo = 1;
                    snprintf(ultimo_evento, sizeof(ultimo_evento),
                             "CRITICO: Rebote atmosferico (gamma=%.2f deg < 5.5). Skip-out al espacio!", angulo_guia);
                } else {
                    dh_actual *= (1.0 - defecto * 0.6);
                    snprintf(ultimo_evento, sizeof(ultimo_evento),
                             "ALERTA: Trayectoria muy plana (gamma=%.2f deg < 5.5). Perdida de frenado.", angulo_guia);
                }
            } else {
                if (mision->altitud > 75000.0) {
                    strcpy(ultimo_evento, "Aproximacion nominal a la interfaz atmosferica (gamma seguro).");
                } else if (mision->altitud > 40000.0) {
                    strcpy(ultimo_evento, "Frenado hipersonico: Escudo termico Avcoat disipando energia.");
                } else if (mision->altitud > 10000.0) {
                    strcpy(ultimo_evento, "Transicion supersonica completada. Capsula desacelerando.");
                } else if (!mision->paracaidas_desplegados) {
                    strcpy(ultimo_evento, "Altitud critica: Preparar despliegue de paracaidas con [E].");
                }
            }

            mision->velocidad += (v_target - mision->velocidad) * (0.06 * (mult_ritmo > 1.0 ? 1.0 : mult_ritmo));
            mision->altitud -= dh_actual * dt_frame;

            if (g_actual > mision->desaceleracion_max_g) {
                mision->desaceleracion_max_g = g_actual;
            }

            double em_actual = 0.5 * mision->masa * mision->velocidad * mision->velocidad +
                               mision->masa * GRAVEDAD_TIERRA * mision->altitud;
            double em_inicial = 0.5 * mision->masa * VELOCIDAD_REINGRESO_MS * VELOCIDAD_REINGRESO_MS +
                                mision->masa * GRAVEDAD_TIERRA * ALTITUD_REINGRESO_M;
            mision->trabajo_rozamiento = em_actual - em_inicial;

            if (mision->altitud > ALTITUD_FIN_ENTRADA_M) mision->fase_actual = FASE_ENTRADA_ATMOSFERICA;
            else if (mision->altitud > ALTITUD_PARACAIDAS_M) mision->fase_actual = FASE_FRENADO_HIPERSONICO;
            else if (mision->altitud > 0.0) mision->fase_actual = FASE_PARACAIDAS;
            else mision->fase_actual = FASE_AMERIZAJE;

            if (tiempo_acumulado - ultimo_registro_tiempo >= 2.5 || mision->altitud <= 0.0) {
                ultimo_registro_tiempo = tiempo_acumulado;
                guardar_turno_historial(mision, angulo_guia, g_actual, mision->trabajo_rozamiento,
                                        tiempo_acumulado, ultimo_evento);
            }

            if (modo_fallo == 1 && mision->altitud >= 115000.0) {
                mision->resultado = MISION_FALLO_REBOTE;
                strcpy(mision->causa_fallo, "Rebote atmosferico: gamma < 5.50 deg (la nave se perdio en el espacio)");
                strcpy(ultimo_evento, "REBOTE CONFIRMADO: Nave en orbita heliocentrica sin retorno.");
            } else if (mision->integridad_escudo <= 0.0) {
                mision->resultado = MISION_FALLO_TERMICO;
                modo_fallo = 2;
                strcpy(mision->causa_fallo, "Colapso termico: escudo destruido por reingreso empinado (gamma > 7.0 deg)");
                strcpy(ultimo_evento, "COLAPSO: Escudo termico Avcoat destruido por calor extremo.");
            } else if (g_actual > CARGA_G_MAXIMA) {
                mision->resultado = MISION_FALLO_GS;
                modo_fallo = 2;
                strcpy(mision->causa_fallo, "Sobrecarga de Gs: desaceleracion letal para la tripulacion (> 15.0 g)");
                snprintf(ultimo_evento, sizeof(ultimo_evento), "SOBRECARGA FATAL: Deceleracion insoportable (%.1f g).", g_actual);
            } else if (mision->altitud <= 0.0) {
                mision->altitud = 0.0;
                mision->fase_actual = FASE_AMERIZAJE;

                mision->fuerza_impacto_n = calcular_fuerza_impacto(mision->masa, mision->velocidad, DISTANCIA_HUNDIMIENTO_M);
                mision->fuerza_impacto_multiplo_peso = calcular_multiplo_peso(mision->fuerza_impacto_n, mision->masa);
                mision->desaceleracion_media = calcular_desaceleracion_media(VELOCIDAD_REINGRESO_MS, mision->velocidad, tiempo_acumulado);

                if (corridor_breached_steep || angulo_guia > ANGULO_REINGRESO_MAX) {
                    mision->resultado = MISION_FALLO_TERMICO;
                    modo_fallo = 2;
                    strcpy(mision->causa_fallo, "Fallo catastrofico: angulo gamma fuera de ventana (> 7.00 deg)");
                    strcpy(ultimo_evento, "FALLO FATAL: La nave sufrio colapso termico / Gs por angulo empinado.");
                } else if (corridor_breached_shallow || angulo_guia < ANGULO_REINGRESO_MIN) {
                    mision->resultado = MISION_FALLO_REBOTE;
                    modo_fallo = 1;
                    strcpy(mision->causa_fallo, "Fallo de reingreso: angulo gamma fuera de ventana (< 5.50 deg)");
                    strcpy(ultimo_evento, "FALLO FATAL: Trayectoria anomala fuera del corredor (< 5.50 deg).");
                } else if (mision->paracaidas_desplegados && mision->velocidad <= 14.0) {
                    mision->resultado = MISION_EXITOSA;
                    strcpy(mision->causa_fallo, "Amerizaje exitoso: tripulacion a salvo en el Pacifico");
                    strcpy(ultimo_evento, "AMERIZAJE EXITOSO: Capsula flotando nominalmente.");
                } else {
                    mision->resultado = MISION_FALLO_IMPACTO;
                    modo_fallo = 3;
                    strcpy(mision->causa_fallo, "Impacto terminal contra el oceano sin paracaidas");
                    strcpy(ultimo_evento, "IMPACTO CRITICO: Choque violento contra el agua.");
                }
            }
        }

        anim_tick++;
    }

    wtimeout(win, -1);
    delwin(win);
    clear();
    refresh();
}

// ----------------------------------------------------------------------------
// 2. ANÁLISIS TEÓRICO (PREGUNTAS ARTEMIS TP)
// ----------------------------------------------------------------------------
static void dibujar_tarjeta_fina(WINDOW *win, int y0, int y1, int x0, int x1, const char *titulo, int color_par) {
    int max_y, max_x;
    getmaxyx(win, max_y, max_x);
    (void)max_x;

    if (color_par) wattron(win, COLOR_PAIR(color_par) | A_BOLD);
    else wattron(win, A_BOLD);

    if (y0 >= 0 && y0 < max_y - 1) {
        mvwprintw(win, y0, x0, "┌");
        dibujar_linea_h(win, y0, x0 + 1, x1 - 1);
        mvwprintw(win, y0, x1, "┐");
        if (titulo && titulo[0] != '\0') {
            mvwprintw(win, y0, x0 + 2, " %s ", titulo);
        }
    }

    for (int y = y0 + 1; y < y1; y++) {
        if (y >= 0 && y < max_y - 1) {
            mvwprintw(win, y, x0, "│");
            mvwprintw(win, y, x1, "│");
        }
    }

    if (y1 >= 0 && y1 < max_y - 1) {
        mvwprintw(win, y1, x0, "└");
        dibujar_linea_h(win, y1, x0 + 1, x1 - 1);
        mvwprintw(win, y1, x1, "┘");
    }

    if (color_par) wattroff(win, COLOR_PAIR(color_par) | A_BOLD);
    else wattroff(win, A_BOLD);
}

static void dibujar_divisor_fino(WINDOW *win, int y, int x0, int x1) {
    int max_y, max_x;
    getmaxyx(win, max_y, max_x);
    (void)max_x;
    if (y >= 0 && y < max_y - 1) {
        mvwprintw(win, y, x0, "├");
        dibujar_linea_h(win, y, x0 + 1, x1 - 1);
        mvwprintw(win, y, x1, "┤");
    }
}

#define PRINT_SAFE(y, x, ...) do { \
    if ((y) >= 0 && (y) < max_y - 1) { \
        mvwprintw(win, (y), (x), __VA_ARGS__); \
    } \
} while (0)

void tui_analisis_teorico(void) {
    WINDOW *win = newwin(LINES, COLS, 0, 0);
    keypad(win, TRUE);
    wtimeout(win, -1);

    DatosEscenario datos;
    datos.masa = MASA_ORION_KG;
    datos.vi = VELOCIDAD_REINGRESO_MS;
    datos.vf = VELOCIDAD_PARACAIDAS_MS;
    datos.hi = ALTITUD_REINGRESO_M;
    datos.hf = ALTITUD_PARACAIDAS_M;
    datos.delta_t = DELTA_T_DESACELERACION_S;
    datos.v_impacto = VELOCIDAD_AMERIZAJE_MS;
    datos.distancia_frenado = DISTANCIA_HUNDIMIENTO_M;
    datos.gamma = 6.2;

    int modo_personalizado = 0;
    int campo = 0;
    int scroll_y = 0;
    int salir = 0;

    while (!salir) {
        int max_y, max_x;
        getmaxyx(win, max_y, max_x);

        int ancho = 81;
        if (max_x - 4 < ancho) ancho = max_x - 4;
        if (ancho < 74) ancho = 74;
        int x0 = (max_x - ancho) / 2;
        if (x0 < 1) x0 = 1;
        int x1 = x0 + ancho - 1;

        ResultadoArtemis res;
        resolverEscenarioArtemis(&res, datos.masa, datos.vi, datos.vf, datos.hi, datos.hf,
                                 datos.delta_t, datos.v_impacto, datos.distancia_frenado);
        EstadoAngulo est_ang = evaluar_angulo_reingreso(datos.gamma);

        werase(win);

        int total_alto = 43;
        int max_scroll = (total_alto > max_y - 1) ? (total_alto - (max_y - 1)) : 0;
        if (scroll_y > max_scroll) scroll_y = max_scroll;
        if (scroll_y < 0) scroll_y = 0;

        int y_base = 1 - scroll_y;

        // 1. Tarjeta de Encabezado Institucional
        int y_h0 = y_base;
        int y_h1 = y_h0 + 3;
        dibujar_tarjeta_fina(win, y_h0, y_h1, x0, x1, "ANALISIS TEORICO Y FISICO - MISION ARTEMIS (SITUACION 4)", PAR_ATMOS_CELESTE);
        wattron(win, COLOR_PAIR(PAR_ATMOS_CELESTE) | A_BOLD);
        PRINT_SAFE(y_h0 + 1, x0 + 2, "%-76.76s", "Universidad Tecnologica Nacional (FRRe) - Catedra Algoritmos y ED / Fisica");
        PRINT_SAFE(y_h0 + 2, x0 + 2, "%-76.76s", "Modelado Energetico, Dinamica de Frenado, Impacto y Ventana de Reingreso");
        wattroff(win, COLOR_PAIR(PAR_ATMOS_CELESTE) | A_BOLD);

        // 2. Selector de Modo
        int y_sel = y_h1 + 1;
        PRINT_SAFE(y_sel, x0 + 2, "Modo de Analisis: ");
        if (modo_personalizado == 0) {
            wattron(win, A_REVERSE | A_BOLD);
            PRINT_SAFE(y_sel, x0 + 20, " [1] * DATOS OFICIALES DE LA MISION ");
            wattroff(win, A_REVERSE | A_BOLD);
            PRINT_SAFE(y_sel, x0 + 58, " [2]   Modo Parametros Libres ");
        } else {
            PRINT_SAFE(y_sel, x0 + 20, " [1]   Datos Oficiales de la Mision ");
            wattron(win, A_REVERSE | A_BOLD);
            PRINT_SAFE(y_sel, x0 + 58, " [2] * MODO PARAMETROS LIBRES ");
            wattroff(win, A_REVERSE | A_BOLD);
        }

        // 3. Tarjeta / Tabla de Parámetros
        int y_p0 = y_sel + 1;
        int y_p1 = y_p0 + 5;

        if (!modo_personalizado) {
            // Dimensiones exactas de las 5 columnas: c0 .. c5
            int c0 = x0;
            int c1 = x0 + 16;
            int c2 = x0 + 32;
            int c3 = x0 + 48;
            int c4 = x0 + 64;
            int c5 = x1;

            // Fila 0: Borde superior continuo con título institucional de la tabla
            if (y_p0 >= 0 && y_p0 < max_y - 1) {
                mvwprintw(win, y_p0, c0, "┌");
                dibujar_linea_h(win, y_p0, c0 + 1, c5 - 1);
                mvwprintw(win, y_p0, c5, "┐");
                wattron(win, A_BOLD);
                mvwprintw(win, y_p0, c0 + 2, " PARAMETROS NOMINALES DE LA MISION ");
                wattroff(win, A_BOLD);
            }

            // Fila 1: Transición de encabezado a las 5 columnas cuadriculadas
            if (y_p0 + 1 >= 0 && y_p0 + 1 < max_y - 1) {
                mvwprintw(win, y_p0 + 1, c0, "├");
                dibujar_linea_h(win, y_p0 + 1, c0 + 1, c1 - 1);
                mvwprintw(win, y_p0 + 1, c1, "┬");
                dibujar_linea_h(win, y_p0 + 1, c1 + 1, c2 - 1);
                mvwprintw(win, y_p0 + 1, c2, "┬");
                dibujar_linea_h(win, y_p0 + 1, c2 + 1, c3 - 1);
                mvwprintw(win, y_p0 + 1, c3, "┬");
                dibujar_linea_h(win, y_p0 + 1, c3 + 1, c4 - 1);
                mvwprintw(win, y_p0 + 1, c4, "┬");
                dibujar_linea_h(win, y_p0 + 1, c4 + 1, c5 - 1);
                mvwprintw(win, y_p0 + 1, c5, "┤");
            }

            // Fila 2: Datos fila 1 con barras verticales exactas
            if (y_p0 + 2 >= 0 && y_p0 + 2 < max_y - 1) {
                mvwprintw(win, y_p0 + 2, c0, "│");
                mvwprintw(win, y_p0 + 2, c1, "│");
                mvwprintw(win, y_p0 + 2, c2, "│");
                mvwprintw(win, y_p0 + 2, c3, "│");
                mvwprintw(win, y_p0 + 2, c4, "│");
                mvwprintw(win, y_p0 + 2, c5, "│");

                mvwprintw(win, y_p0 + 2, c0 + 1, " Masa: %4.0f kg", datos.masa);
                mvwprintw(win, y_p0 + 2, c1 + 1, " vi: %5.0f m/s", datos.vi);
                mvwprintw(win, y_p0 + 2, c2 + 1, " vf: %4.1f m/s", datos.vf);
                mvwprintw(win, y_p0 + 2, c3 + 1, " hi: %5.0f m", datos.hi);
                mvwprintw(win, y_p0 + 2, c4 + 1, " hf: %5.0f m", datos.hf);
            }

            // Fila 3: Separador intermedio cuadriculado
            if (y_p0 + 3 >= 0 && y_p0 + 3 < max_y - 1) {
                mvwprintw(win, y_p0 + 3, c0, "├");
                dibujar_linea_h(win, y_p0 + 3, c0 + 1, c1 - 1);
                mvwprintw(win, y_p0 + 3, c1, "┼");
                dibujar_linea_h(win, y_p0 + 3, c1 + 1, c2 - 1);
                mvwprintw(win, y_p0 + 3, c2, "┼");
                dibujar_linea_h(win, y_p0 + 3, c2 + 1, c3 - 1);
                mvwprintw(win, y_p0 + 3, c3, "┼");
                dibujar_linea_h(win, y_p0 + 3, c3 + 1, c4 - 1);
                mvwprintw(win, y_p0 + 3, c4, "┼");
                dibujar_linea_h(win, y_p0 + 3, c4 + 1, c5 - 1);
                mvwprintw(win, y_p0 + 3, c5, "┤");
            }

            // Fila 4: Datos fila 2 con barras verticales exactas
            if (y_p0 + 4 >= 0 && y_p0 + 4 < max_y - 1) {
                mvwprintw(win, y_p0 + 4, c0, "│");
                mvwprintw(win, y_p0 + 4, c1, "│");
                mvwprintw(win, y_p0 + 4, c2, "│");
                mvwprintw(win, y_p0 + 4, c3, "│");
                mvwprintw(win, y_p0 + 4, c4, "│");
                mvwprintw(win, y_p0 + 4, c5, "│");

                mvwprintw(win, y_p0 + 4, c0 + 1, " dt:   %4.0f s ", datos.delta_t);
                mvwprintw(win, y_p0 + 4, c1 + 1, " v_imp: %3.1f m/s", datos.v_impacto);
                mvwprintw(win, y_p0 + 4, c2 + 1, " d:    %4.2f m ", datos.distancia_frenado);
                mvwprintw(win, y_p0 + 4, c3 + 1, " gam:  %4.2f deg", datos.gamma);
                mvwprintw(win, y_p0 + 4, c4 + 1, " G_lim: < 8.0 g");
            }

            // Fila 5: Borde inferior con uniones de cierre
            if (y_p0 + 5 >= 0 && y_p0 + 5 < max_y - 1) {
                mvwprintw(win, y_p0 + 5, c0, "└");
                dibujar_linea_h(win, y_p0 + 5, c0 + 1, c1 - 1);
                mvwprintw(win, y_p0 + 5, c1, "┴");
                dibujar_linea_h(win, y_p0 + 5, c1 + 1, c2 - 1);
                mvwprintw(win, y_p0 + 5, c2, "┴");
                dibujar_linea_h(win, y_p0 + 5, c2 + 1, c3 - 1);
                mvwprintw(win, y_p0 + 5, c3, "┴");
                dibujar_linea_h(win, y_p0 + 5, c3 + 1, c4 - 1);
                mvwprintw(win, y_p0 + 5, c4, "┴");
                dibujar_linea_h(win, y_p0 + 5, c4 + 1, c5 - 1);
                mvwprintw(win, y_p0 + 5, c5, "┘");
            }
            y_p1 = y_p0 + 5;
        } else {
            // Modo editable: tarjeta con selector y campos interactivos
            dibujar_tarjeta_fina(win, y_p0, y_p0 + 3, x0, x1, "PARAMETROS DEL ESCENARIO [▲/▼ Seleccionar, ◄/► Modificar Valor]", PAR_AMARILLO_NEGRO);

            // Fila 1: Masa, vi, vf, hi
            if (y_p0 + 1 >= 0 && y_p0 + 1 < max_y - 1) {
                int col_x = x0 + 3;

                if (campo == 0) wattron(win, COLOR_PAIR(PAR_AMARILLO_NEGRO) | A_BOLD | A_REVERSE);
                mvwprintw(win, y_p0 + 1, col_x, "%c Masa: %5.0f kg", campo == 0 ? '>' : ' ', datos.masa);
                if (campo == 0) wattroff(win, COLOR_PAIR(PAR_AMARILLO_NEGRO) | A_BOLD | A_REVERSE);
                col_x += 19;

                if (campo == 1) wattron(win, COLOR_PAIR(PAR_AMARILLO_NEGRO) | A_BOLD | A_REVERSE);
                mvwprintw(win, y_p0 + 1, col_x, "%c vi: %5.0f m/s", campo == 1 ? '>' : ' ', datos.vi);
                if (campo == 1) wattroff(win, COLOR_PAIR(PAR_AMARILLO_NEGRO) | A_BOLD | A_REVERSE);
                col_x += 18;

                if (campo == 2) wattron(win, COLOR_PAIR(PAR_AMARILLO_NEGRO) | A_BOLD | A_REVERSE);
                mvwprintw(win, y_p0 + 1, col_x, "%c vf: %4.1f m/s", campo == 2 ? '>' : ' ', datos.vf);
                if (campo == 2) wattroff(win, COLOR_PAIR(PAR_AMARILLO_NEGRO) | A_BOLD | A_REVERSE);
                col_x += 18;

                if (campo == 3) wattron(win, COLOR_PAIR(PAR_AMARILLO_NEGRO) | A_BOLD | A_REVERSE);
                mvwprintw(win, y_p0 + 1, col_x, "%c hi: %6.0f m", campo == 3 ? '>' : ' ', datos.hi);
                if (campo == 3) wattroff(win, COLOR_PAIR(PAR_AMARILLO_NEGRO) | A_BOLD | A_REVERSE);
            }

            // Fila 2: hf, delta_t, v_imp, d_frenado, gamma
            if (y_p0 + 2 >= 0 && y_p0 + 2 < max_y - 1) {
                int col_x = x0 + 3;

                if (campo == 4) wattron(win, COLOR_PAIR(PAR_AMARILLO_NEGRO) | A_BOLD | A_REVERSE);
                mvwprintw(win, y_p0 + 2, col_x, "%c hf: %4.0f m", campo == 4 ? '>' : ' ', datos.hf);
                if (campo == 4) wattroff(win, COLOR_PAIR(PAR_AMARILLO_NEGRO) | A_BOLD | A_REVERSE);
                col_x += 14;

                if (campo == 5) wattron(win, COLOR_PAIR(PAR_AMARILLO_NEGRO) | A_BOLD | A_REVERSE);
                mvwprintw(win, y_p0 + 2, col_x, "%c dt: %4.0f s", campo == 5 ? '>' : ' ', datos.delta_t);
                if (campo == 5) wattroff(win, COLOR_PAIR(PAR_AMARILLO_NEGRO) | A_BOLD | A_REVERSE);
                col_x += 14;

                if (campo == 6) wattron(win, COLOR_PAIR(PAR_AMARILLO_NEGRO) | A_BOLD | A_REVERSE);
                mvwprintw(win, y_p0 + 2, col_x, "%c v_imp: %4.1f m/s", campo == 6 ? '>' : ' ', datos.v_impacto);
                if (campo == 6) wattroff(win, COLOR_PAIR(PAR_AMARILLO_NEGRO) | A_BOLD | A_REVERSE);
                col_x += 18;

                if (campo == 7) wattron(win, COLOR_PAIR(PAR_AMARILLO_NEGRO) | A_BOLD | A_REVERSE);
                mvwprintw(win, y_p0 + 2, col_x, "%c d: %4.2f m", campo == 7 ? '>' : ' ', datos.distancia_frenado);
                if (campo == 7) wattroff(win, COLOR_PAIR(PAR_AMARILLO_NEGRO) | A_BOLD | A_REVERSE);
                col_x += 14;

                if (campo == 8) wattron(win, COLOR_PAIR(PAR_AMARILLO_NEGRO) | A_BOLD | A_REVERSE);
                mvwprintw(win, y_p0 + 2, col_x, "%c γ: %4.2f°", campo == 8 ? '>' : ' ', datos.gamma);
                if (campo == 8) wattroff(win, COLOR_PAIR(PAR_AMARILLO_NEGRO) | A_BOLD | A_REVERSE);
            }
            y_p1 = y_p0 + 3;
        }

        // 4. Tarjeta Pregunta a: Trabajo de Rozamiento
        int y_a0 = y_p1 + 1;
        int y_a1 = y_a0 + 8;
        dibujar_tarjeta_fina(win, y_a0, y_a1, x0, x1, "a) TRABAJO DE LA FUERZA DE ROZAMIENTO AERODINAMICO (W_roz)", 0);
        PRINT_SAFE(y_a0 + 1, x0 + 2, "Principio: Teorema Trabajo-Energia: W_roz = Delta Em = Delta K + Delta U");
        dibujar_divisor_fino(win, y_a0 + 2, x0, x1);
        PRINT_SAFE(y_a0 + 3, x0 + 2, "  Delta K = 1/2*m*(vf² - vi²) = %12.4e J",
                   0.5 * datos.masa * (datos.vf * datos.vf - datos.vi * datos.vi));
        PRINT_SAFE(y_a0 + 4, x0 + 2, "  Delta U = m*g*(hf - hi)     = %12.4e J",
                   datos.masa * GRAVEDAD_TIERRA * (datos.hf - datos.hi));
        wattron(win, COLOR_PAIR(PAR_AMARILLO_NEGRO) | A_BOLD);
        PRINT_SAFE(y_a0 + 5, x0 + 2, "  W_roz   = Delta Em          = %12.4e J  (%.2f GJ disipados en calor)",
                   res.trabajoRozamiento, res.trabajoRozamiento / 1e9);
        wattroff(win, COLOR_PAIR(PAR_AMARILLO_NEGRO) | A_BOLD);
        dibujar_divisor_fino(win, y_a0 + 6, x0, x1);
        PRINT_SAFE(y_a0 + 7, x0 + 2, "Interpretacion: El signo negativo certifica perdida de energia mecanica.");

        // 5. Tarjeta Pregunta b: Desaceleración y Carga G
        int y_b0 = y_a1 + 1;
        int y_b1 = y_b0 + 7;
        dibujar_tarjeta_fina(win, y_b0, y_b1, x0, x1, "b) DESACELERACION MEDIA Y FACTOR DE CARGA G", 0);
        PRINT_SAFE(y_b0 + 1, x0 + 2, "Principio: Cinematica de desaceleracion: a_med = (vf - vi) / Delta t");
        dibujar_divisor_fino(win, y_b0 + 2, x0, x1);
        wattron(win, COLOR_PAIR(PAR_AMARILLO_NEGRO) | A_BOLD);
        PRINT_SAFE(y_b0 + 3, x0 + 2, "  a_med = (%.1f - %.1f) / %.1f = %7.2f m/s² (modulo: %5.2f m/s²)",
                   datos.vf, datos.vi, datos.delta_t, res.desaceleracion, fabs(res.desaceleracion));
        PRINT_SAFE(y_b0 + 4, x0 + 2, "  Carga Fisiologica: G = |a_med| / g = %5.2f g", res.factorG);
        wattroff(win, COLOR_PAIR(PAR_AMARILLO_NEGRO) | A_BOLD);
        dibujar_divisor_fino(win, y_b0 + 5, x0, x1);
        PRINT_SAFE(y_b0 + 6, x0 + 2, "Diagnostico: Carga dentro de tolerancia humana supina (< 4 g nom, < 8 g max).");

        // 6. Tarjeta Pregunta c: Amerizaje y Múltiplo de Peso
        int y_c0 = y_b1 + 1;
        int y_c1 = y_c0 + 7;
        dibujar_tarjeta_fina(win, y_c0, y_c1, x0, x1, "c) FUERZA PROMEDIO DE AMERIZAJE Y MULTIPLO DEL PESO", 0);
        PRINT_SAFE(y_c0 + 1, x0 + 2, "Principio: Teorema Trabajo-Energia en agua: -F_prom * d = 0 - 1/2*m*v_imp²");
        dibujar_divisor_fino(win, y_c0 + 2, x0, x1);
        wattron(win, COLOR_PAIR(PAR_AMARILLO_NEGRO) | A_BOLD);
        PRINT_SAFE(y_c0 + 3, x0 + 2, "  Fuerza del Agua: F_prom = (1/2*m*v_imp²) / d = %9.2f N  (%6.2f kN)",
                   res.fuerzaAmerizaje, res.fuerzaAmerizaje / 1000.0);
        PRINT_SAFE(y_c0 + 4, x0 + 2, "  Peso de la Nave: P = m * g = %9.2f N => F_prom / P = %5.2f veces peso",
                   datos.masa * GRAVEDAD_TIERRA, res.multiploPeso);
        wattroff(win, COLOR_PAIR(PAR_AMARILLO_NEGRO) | A_BOLD);
        dibujar_divisor_fino(win, y_c0 + 5, x0, x1);
        PRINT_SAFE(y_c0 + 6, x0 + 2, "Diagnostico: Desaceleracion moderada (~2.2 g) absorbida por agua y quilla.");

        // 7. Tarjeta Pregunta d: Ventana de Reingreso
        int y_d0 = y_c1 + 1;
        int y_d1 = y_d0 + 5;
        char tit_d[80];
        snprintf(tit_d, sizeof(tit_d), "d) ANALISIS DEL ANGULO DE REINGRESO (GAMMA: %5.2f deg)", datos.gamma);
        dibujar_tarjeta_fina(win, y_d0, y_d1, x0, x1, tit_d, 0);
        PRINT_SAFE(y_d0 + 1, x0 + 2, "Ventana Teorica Segura: 5.50 deg <= gamma <= 7.00 deg");
        dibujar_divisor_fino(win, y_d0 + 2, x0, x1);

        if (est_ang == ANGULO_OPTIMO) {
            wattron(win, COLOR_PAIR(PAR_EXITO_VERDE) | A_BOLD);
            PRINT_SAFE(y_d0 + 3, x0 + 2, "ESTADO: [OPTIMO Y SEGURO] Trayectoria nominal dentro del corredor.");
            wattroff(win, COLOR_PAIR(PAR_EXITO_VERDE) | A_BOLD);
            PRINT_SAFE(y_d0 + 4, x0 + 2, "Consecuencias: Disipacion termica controlada, frenado suave y amerizaje pleno.");
        } else if (est_ang == ANGULO_MUY_PLANO_REBOTE) {
            wattron(win, COLOR_PAIR(PAR_ALERTA_CRITICA) | A_BOLD);
            PRINT_SAFE(y_d0 + 3, x0 + 2, "ESTADO: [CRITICO: REBOTE ATMOSFERICO] gamma < 5.50 deg (Trayectoria rasa)");
            wattroff(win, COLOR_PAIR(PAR_ALERTA_CRITICA) | A_BOLD);
            PRINT_SAFE(y_d0 + 4, x0 + 2, "Consecuencias: Densidad de aire insuficiente; la nave escapa al espacio.");
        } else {
            wattron(win, COLOR_PAIR(PAR_ALERTA_CRITICA) | A_BOLD);
            PRINT_SAFE(y_d0 + 3, x0 + 2, "ESTADO: [CRITICO: REINGRESO MUY ESCARPADO] gamma > 7.00 deg");
            wattroff(win, COLOR_PAIR(PAR_ALERTA_CRITICA) | A_BOLD);
            PRINT_SAFE(y_d0 + 4, x0 + 2, "Consecuencias: Carga G letal (> 15 g), calentamiento e ignicion total.");
        }

        // 8. Barra Inferior de Comandos
        wattron(win, A_REVERSE | A_BOLD);
        for (int x = 0; x < max_x; x++) mvwaddch(win, max_y - 1, x, ' ');
        if (modo_personalizado) {
            mvwprintw(win, max_y - 1, 2, " [1] Oficiales  [2] Libres  [▲/▼] Campo  [◄/►] Modificar  [PgUp/Dn] Scroll  [Q] Menu ");
        } else {
            mvwprintw(win, max_y - 1, 2, " [1] Oficiales  [2] Libres  [R] Restablecer  [▲/▼ / PgUp/Dn] Scroll  [Q] Menu ");
        }
        wattroff(win, A_REVERSE | A_BOLD);

        wrefresh(win);

        int c = wgetch(win);
        switch (c) {
            case '1':
                modo_personalizado = 0;
                datos.masa = MASA_ORION_KG;
                datos.vi = VELOCIDAD_REINGRESO_MS;
                datos.vf = VELOCIDAD_PARACAIDAS_MS;
                datos.hi = ALTITUD_REINGRESO_M;
                datos.hf = ALTITUD_PARACAIDAS_M;
                datos.delta_t = DELTA_T_DESACELERACION_S;
                datos.v_impacto = VELOCIDAD_AMERIZAJE_MS;
                datos.distancia_frenado = DISTANCIA_HUNDIMIENTO_M;
                datos.gamma = 6.2;
                break;
            case '2':
                modo_personalizado = 1;
                break;
            case 'r':
            case 'R':
                datos.masa = MASA_ORION_KG;
                datos.vi = VELOCIDAD_REINGRESO_MS;
                datos.vf = VELOCIDAD_PARACAIDAS_MS;
                datos.hi = ALTITUD_REINGRESO_M;
                datos.hf = ALTITUD_PARACAIDAS_M;
                datos.delta_t = DELTA_T_DESACELERACION_S;
                datos.v_impacto = VELOCIDAD_AMERIZAJE_MS;
                datos.distancia_frenado = DISTANCIA_HUNDIMIENTO_M;
                datos.gamma = 6.2;
                break;
            case KEY_PPAGE:
                if (scroll_y > 0) scroll_y -= 4;
                if (scroll_y < 0) scroll_y = 0;
                break;
            case KEY_NPAGE:
                if (scroll_y < max_scroll) scroll_y += 4;
                if (scroll_y > max_scroll) scroll_y = max_scroll;
                break;
            case KEY_UP:
                if (modo_personalizado) {
                    if (campo > 0) campo--;
                    if (campo < 4 && scroll_y > 0) scroll_y = 0;
                } else {
                    if (scroll_y > 0) scroll_y--;
                }
                break;
            case KEY_DOWN:
                if (modo_personalizado) {
                    if (campo < 8) campo++;
                } else {
                    if (scroll_y < max_scroll) scroll_y++;
                }
                break;
            case KEY_LEFT:
                if (modo_personalizado) {
                    if (campo == 0 && datos.masa > 1000.0) datos.masa -= 500.0;
                    else if (campo == 1 && datos.vi > 1000.0) datos.vi -= 500.0;
                    else if (campo == 2 && datos.vf > 5.0) datos.vf -= 5.0;
                    else if (campo == 3 && datos.hi > 10000.0) datos.hi -= 5000.0;
                    else if (campo == 4 && datos.hf > 500.0) datos.hf -= 500.0;
                    else if (campo == 5 && datos.delta_t > 50.0) datos.delta_t -= 25.0;
                    else if (campo == 6 && datos.v_impacto > 1.0) datos.v_impacto -= 1.0;
                    else if (campo == 7 && datos.distancia_frenado > 0.5) datos.distancia_frenado -= 0.25;
                    else if (campo == 8 && datos.gamma > 1.0) datos.gamma -= 0.2;
                }
                break;
            case KEY_RIGHT:
                if (modo_personalizado) {
                    if (campo == 0 && datos.masa < 50000.0) datos.masa += 500.0;
                    else if (campo == 1 && datos.vi < 20000.0) datos.vi += 500.0;
                    else if (campo == 2 && datos.vf < datos.vi) datos.vf += 5.0;
                    else if (campo == 3 && datos.hi < 200000.0) datos.hi += 5000.0;
                    else if (campo == 4 && datos.hf < datos.hi) datos.hf += 500.0;
                    else if (campo == 5 && datos.delta_t < 2000.0) datos.delta_t += 25.0;
                    else if (campo == 6 && datos.v_impacto < 100.0) datos.v_impacto += 1.0;
                    else if (campo == 7 && datos.distancia_frenado < 10.0) datos.distancia_frenado += 0.25;
                    else if (campo == 8 && datos.gamma < 80.0) datos.gamma += 0.2;
                }
                break;
            case 'q':
            case 'Q':
            case 27:
                salir = 1;
                break;
            default:
                break;
        }
    }

    delwin(win);
    clear();
    refresh();
}

// ----------------------------------------------------------------------------
// 3. REPORTE DE TELEMETRÍA DE LA MISIÓN
// ----------------------------------------------------------------------------
void tui_reporte_telemetria(const MisionArtemis *mision) {
    WINDOW *win = newwin(LINES, COLS, 0, 0);
    keypad(win, TRUE);
    wtimeout(win, -1);

    MisionArtemis mision_local;
    if (mision != NULL && mision->total_puntos > 0) {
        mision_local = *mision;
    } else {
        simulador_inicializar(&mision_local);
        while (mision_local.resultado == MISION_EN_CURSO) {
            int accion = (mision_local.altitud <= ALTITUD_FRENADO_M) ? 1 : 0;
            actualizar_estado_turno(&mision_local, 6.2, accion, DT_TURNO_S);
        }
    }

    int seleccion = 0;
    int scroll_offset = 0;
    int salir = 0;

    while (!salir) {
        int max_y, max_x;
        getmaxyx(win, max_y, max_x);

        werase(win);

        wattron(win, A_REVERSE | A_BOLD);
        for (int x = 0; x < max_x; x++) mvwaddch(win, 0, x, ' ');
        mvwprintw(win, 0, 2, " REPORTE DE TELEMETRIA Y REGISTRO DE CAJA NEGRA (VDR) - MISION ARTEMIS ");
        mvwprintw(win, 0, max_x - 22, " Total Turnos: %d ", mision_local.total_puntos);
        wattroff(win, A_REVERSE | A_BOLD);

        int color_veredicto = (mision_local.resultado == MISION_EXITOSA) ? PAR_EXITO_VERDE : PAR_ALERTA_CRITICA;
        wattron(win, COLOR_PAIR(color_veredicto) | A_BOLD);
        mvwprintw(win, 2, 2, "VEREDICTO: %-50.50s", mision_local.causa_fallo);
        wattroff(win, COLOR_PAIR(color_veredicto) | A_BOLD);

        double t_vuelo = (mision_local.total_puntos > 0) ? mision_local.historial[mision_local.total_puntos - 1].tiempo : 0.0;
        mvwprintw(win, 3, 2, "Tiempo de Vuelo: %-6.0f s | Trabajo W_roz: %10.4e J | Desaceleracion Media: %5.2f m/s2",
                  t_vuelo, mision_local.trabajo_rozamiento, fabs(mision_local.desaceleracion_media));
        mvwprintw(win, 4, 2, "Carga G Maxima:  %5.2f g | Salud Escudo:  %5.1f%%          | Fuerza Amerizaje:     %8.2f N",
                  mision_local.desaceleracion_max_g, mision_local.integridad_escudo, mision_local.fuerza_impacto_n);
        if (mision_local.fuerza_impacto_multiplo_peso > 0.0) {
            mvwprintw(win, 5, 2, "Multiplo de Peso Impacto: %.2f veces el peso de la nave (amortiguacion nominal)",
                      mision_local.fuerza_impacto_multiplo_peso);
        }

        int y_tabla = 7;
        wattron(win, A_BOLD | A_UNDERLINE);
        mvwprintw(win, y_tabla, 2,
                  "%-4s %-7s %-10s %-9s %-6s %-7s %-6s %-16s %s",
                  "Turn", "t (s)", "Altitud(m)", "v (m/s)", "Mach", "Gamma", "G", "Fase", "Evento");
        wattroff(win, A_BOLD | A_UNDERLINE);

        int filas_visibles = max_y - y_tabla - 3;
        if (filas_visibles < 1) filas_visibles = 1;

        if (seleccion < scroll_offset) scroll_offset = seleccion;
        if (seleccion >= scroll_offset + filas_visibles) scroll_offset = seleccion - filas_visibles + 1;

        for (int i = 0; i < filas_visibles && (scroll_offset + i) < mision_local.total_puntos; i++) {
            int idx = scroll_offset + i;
            const PuntoTelemetria *p = &mision_local.historial[idx];
            int y_fila = y_tabla + 1 + i;

            if (idx == seleccion) {
                wattron(win, A_REVERSE | A_BOLD);
                for (int x = 1; x < max_x - 1; x++) mvwaddch(win, y_fila, x, ' ');
            }

            mvwprintw(win, y_fila, 2,
                      "%-4d %-7.0f %-10.0f %-9.1f %-6.1f %-7.2f %-6.2f %-16.16s %-32.32s",
                      idx + 1, p->tiempo, p->altitud, p->velocidad,
                      p->velocidad / 343.0, p->angulo_gamma, p->aceleracion_g,
                      nombre_fase_mision(p->fase), p->evento);

            if (idx == seleccion) {
                wattroff(win, A_REVERSE | A_BOLD);
            }
        }

        wattron(win, A_REVERSE);
        for (int x = 0; x < max_x; x++) mvwaddch(win, max_y - 1, x, ' ');
        mvwprintw(win, max_y - 1, 2, " [Arriba/Abajo] Fila  [PgUp/PgDn] Pagina  [D] Recargar Demo  [Q / ENTER] Volver al Menu ");
        wattroff(win, A_REVERSE);

        wrefresh(win);

        int tecla = wgetch(win);
        switch (tecla) {
            case KEY_UP:
                if (seleccion > 0) seleccion--;
                break;
            case KEY_DOWN:
                if (seleccion < mision_local.total_puntos - 1) seleccion++;
                break;
            case KEY_PPAGE:
                seleccion -= 5;
                if (seleccion < 0) seleccion = 0;
                break;
            case KEY_NPAGE:
                seleccion += 5;
                if (seleccion >= mision_local.total_puntos) seleccion = mision_local.total_puntos - 1;
                break;
            case 'd':
            case 'D':
                simulador_inicializar(&mision_local);
                while (mision_local.resultado == MISION_EN_CURSO) {
                    int accion = (mision_local.altitud <= ALTITUD_FRENADO_M) ? 1 : 0;
                    actualizar_estado_turno(&mision_local, 6.2, accion, DT_TURNO_S);
                }
                seleccion = 0;
                scroll_offset = 0;
                break;
            case 'q':
            case 'Q':
            case 27:
            case 10:
            case 13:
            case KEY_ENTER:
                salir = 1;
                break;
            default:
                break;
        }
    }

    delwin(win);
    clear();
    refresh();
}

// ----------------------------------------------------------------------------
// 4. MANUAL DE VUELO Y FÓRMULAS FÍSICAS
// ----------------------------------------------------------------------------
void tui_manual_vuelo(void) {
    WINDOW *win = newwin(LINES, COLS, 0, 0);
    keypad(win, TRUE);
    wtimeout(win, -1);

    int tab = 0;
    int salir = 0;

    while (!salir) {
        int max_y, max_x;
        getmaxyx(win, max_y, max_x);

        werase(win);

        wattron(win, A_REVERSE | A_BOLD);
        for (int x = 0; x < max_x; x++) mvwaddch(win, 0, x, ' ');
        mvwprintw(win, 0, 2, " MANUAL DE VUELO Y FORMULAS FISICAS - MISION ARTEMIS (SITUACION 4) ");
        wattroff(win, A_REVERSE | A_BOLD);

        wattron(win, A_BOLD);
        mvwprintw(win, 2, 2, " %s  |  %s  |  %s  |  %s  |  %s ",
                  tab == 0 ? "[1. Mision Artemis]" : " 1. Mision Artemis ",
                  tab == 1 ? "[2. Trabajo y Em]" : " 2. Trabajo y Em ",
                  tab == 2 ? "[3. Cinematica y G]" : " 3. Cinematica y G ",
                  tab == 3 ? "[4. Amerizaje]" : " 4. Amerizaje ",
                  tab == 4 ? "[5. Ventana Gamma]" : " 5. Ventana Gamma ");
        wattroff(win, A_BOLD);

        for (int x = 2; x < max_x - 2; x++) mvwaddch(win, 3, x, ACS_HLINE);

        if (tab == 0) {
            wattron(win, COLOR_PAIR(PAR_ATMOS_CELESTE) | A_BOLD);
            mvwprintw(win, 5, 2, "1. CONTEXTO DE LA MISION Y CAPSULA ORION:");
            wattroff(win, COLOR_PAIR(PAR_ATMOS_CELESTE) | A_BOLD);
            mvwprintw(win, 7, 4, "- Masa del modulo de tripulacion Orion: 10.400 kg.");
            mvwprintw(win, 8, 4, "- Interfaz de reingreso (Entry Interface): Altitud 120 km (120.000 m).");
            mvwprintw(win, 9, 4, "- Rapidez en la interfaz: 11.000 m/s (~Mach 32, velocidad de retorno lunar).");
            mvwprintw(win, 10, 4, "- Escudo termico: Material ablativo Avcoat de 4 cm de espesor.");
            mvwprintw(win, 11, 4, "- Despliegue de paracaidas principales: Altitud 2.500 m, velocidad 25 m/s.");
            mvwprintw(win, 12, 4, "- Velocidad de amerizaje: 8,0 m/s (~28,8 km/h) en el Oceano Pacifico.");
            mvwprintw(win, 13, 4, "- Penetracion en el agua (carrera de frenado): 1,50 metros.");
            mvwprintw(win, 15, 4, "La capsula Orion utiliza reingreso guiado con sustentacion aerodinamica");
            mvwprintw(win, 16, 4, "para gestionar las cargas termicas y desaceleraciones de la tripulacion.");
        } else if (tab == 1) {
            wattron(win, COLOR_PAIR(PAR_AMARILLO_NEGRO) | A_BOLD);
            mvwprintw(win, 5, 2, "2. TEOREMA DE TRABAJO Y ENERGIA MECANICA (W_roz):");
            wattroff(win, COLOR_PAIR(PAR_AMARILLO_NEGRO) | A_BOLD);
            mvwprintw(win, 7, 4, "Formula:  W_nc = Delta Em = Em_final - Em_inicial");
            mvwprintw(win, 8, 4, "          Em = K + U = 1/2*m*v2 + m*g*h");
            mvwprintw(win, 10, 4, "Desarrollo:");
            mvwprintw(win, 11, 6, "Delta K = 1/2*m*(vf2 - vi2)   [Variacion de energia cinetica]");
            mvwprintw(win, 12, 6, "Delta U = m*g*(hf - hi)       [Variacion de energia potencial gravitatoria]");
            mvwprintw(win, 13, 6, "W_roz = Delta K + Delta U     [Trabajo de la fuerza de friccion del aire]");
            mvwprintw(win, 15, 4, "Propiedad fisica:");
            mvwprintw(win, 16, 6, "W_roz es siempre NEGATIVO (W_roz < 0). El signo menos indica que la");
            mvwprintw(win, 17, 6, "fuerza de rozamiento es disipativa y extrae energia mecanica de la nave,");
            mvwprintw(win, 18, 6, "convirtiendola en calor intenso (plasma a mas de 2.800 C) en el escudo.");
        } else if (tab == 2) {
            wattron(win, COLOR_PAIR(PAR_CYAN_NEGRO) | A_BOLD);
            mvwprintw(win, 5, 2, "3. DESACELERACION MEDIA Y FACTOR DE CARGA FISIOLOGICA (G):");
            wattroff(win, COLOR_PAIR(PAR_CYAN_NEGRO) | A_BOLD);
            mvwprintw(win, 7, 4, "Formula de desaceleracion media:  a_med = (vf - vi) / Delta t");
            mvwprintw(win, 9, 4, "Formula de carga en g:            G = |a_med| / g   (con g = 9,80 m/s2)");
            mvwprintw(win, 11, 4, "Limites biofisiologicos para la tripulacion humana:");
            mvwprintw(win, 12, 6, "- 0 a 4 g: Operacion nominal comoda en posicion supina.");
            mvwprintw(win, 13, 6, "- 4 a 8 g: Exigencia elevada pero tolerable durante varios minutos.");
            mvwprintw(win, 14, 6, "- 8 a 12 g: Limite critico, vision de tunel y riesgo de desmayo (G-LOC).");
            mvwprintw(win, 15, 6, "- > 15 g: Colapso estructural del fuselaje y letal para los astronautas.");
        } else if (tab == 3) {
            wattron(win, COLOR_PAIR(PAR_EXITO_VERDE) | A_BOLD);
            mvwprintw(win, 5, 2, "4. DINAMICA DE IMPACTO EN EL AGUA (AMERIZAJE):");
            wattroff(win, COLOR_PAIR(PAR_EXITO_VERDE) | A_BOLD);
            mvwprintw(win, 7, 4, "Teorema Trabajo-Energia en el fluido:");
            mvwprintw(win, 8, 6, "W_frenado = Delta K");
            mvwprintw(win, 9, 6, "-F_prom * d = 0 - 1/2*m*v_impacto2");
            mvwprintw(win, 10, 6, "F_prom = (1/2*m*v_impacto2) / d");
            mvwprintw(win, 12, 4, "Multiplo del peso:");
            mvwprintw(win, 13, 6, "P = m * g");
            mvwprintw(win, 14, 6, "Relacion = F_prom / P = (1/2*v_impacto2) / (g * d)");
            mvwprintw(win, 16, 4, "En la mision oficial (v=8 m/s, d=1.5 m), la carga es de 2,18 g,");
            mvwprintw(win, 17, 4, "completamente segura y amortiguada por bolsas de aire y atenuadores.");
        } else if (tab == 4) {
            wattron(win, COLOR_PAIR(PAR_ALERTA_CRITICA) | A_BOLD);
            mvwprintw(win, 5, 2, "5. CORREDOR Y VENTANA DE REINGRESO (ANGULO GAMMA):");
            wattroff(win, COLOR_PAIR(PAR_ALERTA_CRITICA) | A_BOLD);
            mvwprintw(win, 7, 4, "Ventana de Entrada Segura:  5.50 deg <= gamma <= 7.00 deg");
            mvwprintw(win, 9, 4, "Riesgos en trayectorias anomalas:");
            mvwprintw(win, 11, 6, "* gamma < 5.50 deg  (Angulo muy superficial): Rebote atmosferico (skip-out).");
            mvwprintw(win, 12, 8, "La atmosfera no provee suficiente friccion y la nave se pierde en el espacio.");
            mvwprintw(win, 14, 6, "* gamma > 7.00 deg  (Angulo excesivamente escarpado): Entrada violenta.");
            mvwprintw(win, 15, 8, "Flujo termico excede 3.000 C y desaceleraciones > 15 g desintegran la nave.");
        }

        wattron(win, A_REVERSE);
        for (int x = 0; x < max_x; x++) mvwaddch(win, max_y - 1, x, ' ');
        mvwprintw(win, max_y - 1, 2, " [1-5] Seleccionar Pestana   [TAB / Flechas] Navegar   [Q / ENTER] Volver al Menu ");
        wattroff(win, A_REVERSE);

        wrefresh(win);

        int tecla = wgetch(win);
        switch (tecla) {
            case '1': tab = 0; break;
            case '2': tab = 1; break;
            case '3': tab = 2; break;
            case '4': tab = 3; break;
            case '5': tab = 4; break;
            case '	':
            case KEY_RIGHT:
                tab = (tab + 1) % 5;
                break;
            case KEY_LEFT:
                tab = (tab + 4) % 5;
                break;
            case 'q':
            case 'Q':
            case 27:
            case 10:
            case 13:
            case KEY_ENTER:
                salir = 1;
                break;
            default:
                break;
        }
    }

    delwin(win);
    clear();
    refresh();
}

// ----------------------------------------------------------------------------
// 5. INTEGRANTES DEL EQUIPO
// ----------------------------------------------------------------------------
void tui_integrantes(void) {
    int alto = 20, ancho = 74;
    int inicio_y = (LINES - alto) / 2;
    int inicio_x = (COLS - ancho) / 2;
    if (inicio_y < 0) inicio_y = 0;
    if (inicio_x < 0) inicio_x = 0;

    WINDOW *win = newwin(alto, ancho, inicio_y, inicio_x);
    keypad(win, TRUE);
    wtimeout(win, -1);

    werase(win);

    // Marco exterior con líneas finas simples
    wattron(win, COLOR_PAIR(PAR_ATMOS_CELESTE) | A_BOLD);
    mvwprintw(win, 0, 0, "┌");
    dibujar_linea_h(win, 0, 1, ancho - 2);
    mvwprintw(win, 0, ancho - 1, "┐");

    for (int y = 1; y < alto - 1; y++) {
        mvwprintw(win, y, 0, "│");
        mvwprintw(win, y, ancho - 1, "│");
    }

    mvwprintw(win, alto - 1, 0, "└");
    dibujar_linea_h(win, alto - 1, 1, ancho - 2);
    mvwprintw(win, alto - 1, ancho - 1, "┘");

    // Cabecera institucional UTN
    mvwprintw(win, 1, 21, "UNIVERSIDAD TECNOLOGICA NACIONAL");
    mvwprintw(win, 2, 22, "Facultad Regional Resistencia");
    mvwprintw(win, 3, 14, "Carrera: Ingenieria en Sistemas de Informacion");
    mvwprintw(win, 4, 15, "Asignatura: Algoritmos y Estructuras de Datos");
    wattroff(win, COLOR_PAIR(PAR_ATMOS_CELESTE) | A_BOLD);

    // Separador institucional fino
    wattron(win, COLOR_PAIR(PAR_ATMOS_CELESTE) | A_BOLD);
    mvwprintw(win, 5, 0, "├");
    dibujar_linea_h(win, 5, 1, ancho - 2);
    mvwprintw(win, 5, ancho - 1, "┤");
    wattroff(win, COLOR_PAIR(PAR_ATMOS_CELESTE) | A_BOLD);

    wattron(win, A_BOLD);
    mvwprintw(win, 6, 10, "Proyecto: Simulador de Misiones Artemis - Situacion 4");
    mvwprintw(win, 7, 9,  "Reingreso Atmosferico, Frenado Aerodinamico y Amerizaje");
    wattroff(win, A_BOLD);

    // Tabla de integrantes con caracteres de línea fina
    int x0 = 4;
    int x1 = 29;
    int x2 = 56;
    int x3 = 69;

    wattron(win, A_BOLD);

    // Borde superior de la tabla
    mvwprintw(win, 9, x0, "┌");
    dibujar_linea_h(win, 9, x0 + 1, x1 - 1);
    mvwprintw(win, 9, x1, "┬");
    dibujar_linea_h(win, 9, x1 + 1, x2 - 1);
    mvwprintw(win, 9, x2, "┬");
    dibujar_linea_h(win, 9, x2 + 1, x3 - 1);
    mvwprintw(win, 9, x3, "┐");

    // Fila de encabezado
    mvwprintw(win, 10, x0, "│");
    mvwprintw(win, 10, x0 + 2, "%-23s", "Integrante");
    mvwprintw(win, 10, x1, "│");
    mvwprintw(win, 10, x1 + 2, "%-25s", "Usuario GitHub");
    mvwprintw(win, 10, x2, "│");
    mvwprintw(win, 10, x2 + 2, "%-11s", "Estado");
    mvwprintw(win, 10, x3, "│");

    // Separador de encabezado
    mvwprintw(win, 11, x0, "├");
    dibujar_linea_h(win, 11, x0 + 1, x1 - 1);
    mvwprintw(win, 11, x1, "┼");
    dibujar_linea_h(win, 11, x1 + 1, x2 - 1);
    mvwprintw(win, 11, x2, "┼");
    dibujar_linea_h(win, 11, x2 + 1, x3 - 1);
    mvwprintw(win, 11, x3, "┤");

    // Filas de datos de integrantes
    const char *integrantes[5][3] = {
        {"Brian Kalbermatter",      "@BrianKalbermatter",        "Activo"},
        {"Emiliano Elst",           "@emilianoelst",             "Activo"},
        {"Enzo Solis",              "@EnzoSolis",                "Activo"},
        {"Jesus Miranda Arce",      "@EzXdlol",                  "Activo"},
        {"Ciro Alejandro Villalba", "@Gravitated-ribbit-ribbit", "Activo"}
    };

    for (int i = 0; i < 5; i++) {
        int y = 12 + i;
        mvwprintw(win, y, x0, "│");
        wattron(win, A_BOLD);
        mvwprintw(win, y, x0 + 2, "%-23s", integrantes[i][0]);
        wattroff(win, A_BOLD);

        mvwprintw(win, y, x1, "│");
        wattron(win, COLOR_PAIR(PAR_CYAN_NEGRO) | A_BOLD);
        mvwprintw(win, y, x1 + 2, "%-25s", integrantes[i][1]);
        wattroff(win, COLOR_PAIR(PAR_CYAN_NEGRO) | A_BOLD);

        mvwprintw(win, y, x2, "│");
        wattron(win, COLOR_PAIR(PAR_EXITO_VERDE) | A_BOLD);
        mvwprintw(win, y, x2 + 2, "%-11s", integrantes[i][2]);
        wattroff(win, COLOR_PAIR(PAR_EXITO_VERDE) | A_BOLD);

        mvwprintw(win, y, x3, "│");
    }

    // Borde inferior de la tabla
    mvwprintw(win, 17, x0, "└");
    dibujar_linea_h(win, 17, x0 + 1, x1 - 1);
    mvwprintw(win, 17, x1, "┴");
    dibujar_linea_h(win, 17, x1 + 1, x2 - 1);
    mvwprintw(win, 17, x2, "┴");
    dibujar_linea_h(win, 17, x2 + 1, x3 - 1);
    mvwprintw(win, 17, x3, "┘");

    wattroff(win, A_BOLD);

    // Barra inferior
    wattron(win, A_REVERSE | A_BOLD);
    mvwprintw(win, 18, 13, " [ENTER / ESPACIO / Q] Volver al Menu Principal ");
    wattroff(win, A_REVERSE | A_BOLD);

    wrefresh(win);

    wgetch(win);

    delwin(win);
    clear();
    refresh();
}
