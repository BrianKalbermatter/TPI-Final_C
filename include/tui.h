#ifndef TUI_H
#define TUI_H

#ifdef MOUSE_MOVED
#undef MOUSE_MOVED
#endif
#include <curses.h>
#include <simulador.h>

// -- Opciones del Menú Principal
typedef enum {
    MENU_OPCION_INVALIDA = 0,
    MENU_SIMULACION_INTERACTIVA = 1,
    MENU_ANALISIS_TEORICO = 2,
    MENU_REPORTE_TELEMETRIA = 3,
    MENU_MANUAL_VUELO = 4,
    MENU_INTEGRANTES = 5,
    MENU_TEST_FONDO = 6,
    MENU_TEST_CAPSULA = 7,
    MENU_SALIR = 8
} OpcionMenu;

// -- Ciclo de vida de la interfaz
void tui_iniciar(void);
void tui_liberar(void);
void tui_set_modo_depuracion(int activo);
int tui_get_modo_depuracion(void);

// -- Ventanas y Vistas del Sistema
int menu_principal(void);
void tui_simulacion_interactiva(MisionArtemis *mision);
void tui_analisis_teorico(void);
void tui_reporte_telemetria(const MisionArtemis *mision);
void tui_manual_vuelo(void);
void tui_integrantes(void);
void tui_test_fondo(void);
void tui_test_capsula(void);
void rellenar_fondo_escena(WINDOW *win, double altitud);

#endif // TUI_H
