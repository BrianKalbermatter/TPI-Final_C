#include <curses.h>
#include <tui.h>

void tui_iniciar(void) {
  initscr();
  cbreak();
  noecho();
  keypad(stdscr, TRUE);
  curs_set(0);

  if (has_colors()) {
    start_color();
    use_default_colors();
  }
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
    "6. Salir de la Misión"
  };

  int total_opciones = 6;
  int seleccion = 0; // Índice de la opción actualmente seleccionada
  int tecla;

  // Crear una ventana centrada de 12 filas por 60 columnas
  int alto = 12, ancho = 60;
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
    case KEY_ENTER:
      delwin(menu_win);
      return seleccion + 1; // Retorna la opción (1 al 6) igual que tu antiguo menú
    case 'q':
    case 'Q':
      delwin(menu_win);
      return 6; // Atajo rápido para salir
    }
  }

  return 0;
}
