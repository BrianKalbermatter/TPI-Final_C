#include <simulador.h>
#include <utilidades.h>
#include <validaciones.h>
#include <tui.h>

int main(void) {
  activar_utf8();
  log_set_min_level(LOG_LEVEL_INFO);

  MisionArtemis mision_actual;
  simulador_inicializar(&mision_actual);

  tui_iniciar();

  int opcion = 0;
  do {
    opcion = menu_principal();

    switch (opcion) {
      case 1:
        // TODO: Migrar simulación interactiva a TUI
        break;
      case 2:
        // TODO: Migrar análisis teórico a TUI
        break;
      case 3:
        // TODO: Migrar reporte de telemetría a TUI
        break;
      case 4:
        // TODO: Migrar manual de vuelo a TUI
        break;
      case 5:
        // TODO: Migrar integrantes del equipo a TUI
        break;
      case 6:
        tui_test_fondo();
        break;
      case 7:
        // Salir de la misión
        break;
      default:
        break;
    }
  } while (opcion != 7);

  tui_liberar();

  return 0;
}
