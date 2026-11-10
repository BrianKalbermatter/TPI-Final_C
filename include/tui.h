#ifndef TUI_H
#define TUI_H

#ifdef MOUSE_MOVED
#undef MOUSE_MOVED
#endif
#include <curses.h>

// -- Ciclo de vida de la interfaz
void tui_iniciar(void);
void tui_liberar(void);

// -- Ventanas y Vistas
int menu_principal(void);
void tui_test_fondo(void);
void rellenar_fondo_escena(WINDOW *win, double altitud);

#endif // TUI_H
