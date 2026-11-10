#include <curses.h>
#include <tui.h>
#include <math.h>
#include <stdlib.h>
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
    PAR_EXITO_VERDE      // Verde sobre negro (Misión cumplida)
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

int menu_principal(void) {
  const char *opciones[] = {
    "1. Iniciar Simulación Interactiva (Modo Piloto)",
    "2. Análisis Teórico (Preguntas Artemis TP)",
    "3. Ver Reporte de Telemetría",
    "4. Manual de Vuelo y Fórmulas Físicas",
    "5. Integrantes del Equipo",
    "6. Test: Render de Fondo por Altura",
    "7. Salir de la Misión"
  };

  int total_opciones = 7;
  int seleccion = 0; // Índice de la opción actualmente seleccionada
  int tecla;

  // Crear una ventana centrada de 13 filas por 60 columnas
  int alto = 13, ancho = 60;
  int inicio_y = (LINES - alto) / 2;
  int inicio_x = (COLS - ancho) / 2;
  
  WINDOW *menu_win = newwin(alto, ancho, inicio_y, inicio_x);
  keypad(menu_win, TRUE);

  while (1) {
    // 1. Dibujar el marco y el título
    box(menu_win, 0, 0);
    mvwprintw(menu_win, 0, (ancho - 22) / 2, " [ MENÚ DE CONTROL ] ");
    
    // 2. Dibujar las opciones resaltando solo la actual
    for (int i = 0; i < total_opciones; i++) {
      if (i == seleccion) {
        wattron(menu_win, A_REVERSE | A_BOLD); // Resaltar
        mvwprintw(menu_win, i + 2, 4, "-> %s", opciones[i]);
        wattroff(menu_win, A_REVERSE | A_BOLD);
      } else {
        mvwprintw(menu_win, i + 2, 4, "   %s", opciones[i]);
      }
    }

    // Pie de ayuda
    mvwprintw(menu_win, alto - 2, 4, "[↑/↓] Navegar   [ENTER] Seleccionar   [Q] Salir");
    
    wrefresh(menu_win); // Refrescar para mostrar los cambios en pantalla

    // 3. Capturar la tecla pulsada
    tecla = wgetch(menu_win);
    switch (tecla) {
    case KEY_UP:
      seleccion--;
      if (seleccion < 0) seleccion = total_opciones - 1; // Ciclar hacia el final
      break;
    case KEY_DOWN:
      seleccion++;
      if (seleccion >= total_opciones) seleccion = 0;    // Ciclar hacia el principio
      break;
    case 10: // Tecla ENTER
    case 13:
    case KEY_ENTER:
      delwin(menu_win);
      return seleccion + 1; // Retorna la opción (1 al 7)
    case 'q':
    case 'Q':
      delwin(menu_win);
      return 7; // Atajo rápido para salir
    }
  }

  return -1;
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

void rellenar_fondo_escena(WINDOW *win, double altitud) {
    int max_y, max_x;
    getmaxyx(win, max_y, max_x);

    // 1. Geometría vertical
    double fila_centro = max_y / 2.0;
    int filas_abajo    = (int)((max_y - 2) - fila_centro);
    double escala      = escala_vertical(altitud, filas_abajo);
    double curvatura   = curvatura_filas(altitud);
    double semi_ancho  = max_x / 2.0;

    // 2. Horizonte marino: asoma por el borde inferior a 2500 m y sube hasta 2/3 de la pantalla
    int y_amerizaje  = max_y - (max_y / 3);
    int filas_mar_finales = (max_y - 1) - y_amerizaje;   // Filas de mar visibles al amerizar
    if (filas_mar_finales < 1) filas_mar_finales = 1;

    int linea_agua = max_y + 1;                          // Fuera de pantalla por defecto
    if (altitud <= H_INICIO_OCEANO) {
        double ratio = altitud / H_INICIO_OCEANO;        // 1.0 a 2500 m, 0.0 a 0 m
        linea_agua = y_amerizaje + (int)(ratio * (max_y - y_amerizaje));
    }

    // 3. Pintar fila por fila desde el techo hasta el piso
    for (int y = 1; y < max_y - 1; y++) {

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

    // 4. Ondas de amerizaje cuando la cápsula está en el agua
    if (altitud <= 50.0) {
        int y_ondas = linea_agua + (filas_mar_finales * 2) / 3;
        if (y_ondas >= 1 && y_ondas < max_y - 1) {
            dibujar_ondas_amerizaje(win, y_ondas, max_x);
        }
    }
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
