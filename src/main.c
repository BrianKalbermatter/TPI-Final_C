#include <simulador.h>
#include <utilidades.h>
#include <validaciones.h>
#include <tui.h>

#include <simulador.h>
#include <utilidades.h>
#include <validaciones.h>
#include <tui.h>
#include <string.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
  activar_utf8();

  // Detección de bandera de modo depuración por CLI o variable de entorno
  int modo_debug = 0;
  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--debug") == 0 || strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "/debug") == 0) {
      modo_debug = 1;
      break;
    }
  }

  const char *env_debug = getenv("ARTEMIS_DEBUG");
  if (env_debug != NULL && (strcmp(env_debug, "1") == 0 || strcmp(env_debug, "true") == 0)) {
    modo_debug = 1;
  }

  if (modo_debug) {
    log_set_min_level(LOG_LEVEL_DEBUG);
    tui_set_modo_depuracion(1);
  } else {
    log_set_min_level(LOG_LEVEL_INFO);
    tui_set_modo_depuracion(0);
  }

  MisionArtemis mision_actual;
  simulador_inicializar(&mision_actual);

  tui_iniciar();

  int opcion = 0;
  do {
    opcion = menu_principal();

    switch (opcion) {
      case MENU_SIMULACION_INTERACTIVA:
        simulador_ejecutar_modo_interactivo(&mision_actual);
        break;
      case MENU_ANALISIS_TEORICO:
        simulador_ejecutar_modo_teorico();
        break;
      case MENU_REPORTE_TELEMETRIA:
        simulador_mostrar_reporte(&mision_actual);
        break;
      case MENU_MANUAL_VUELO:
        simulador_mostrar_manual();
        break;
      case MENU_INTEGRANTES:
        simulador_mostrar_integrantes();
        break;
      case MENU_TEST_FONDO:
        tui_test_fondo();
        break;
      case MENU_TEST_CAPSULA:
        tui_test_capsula();
        break;
      case MENU_SALIR:
        // Salir de la misión
        break;
      default:
        break;
    }
  } while (opcion != MENU_SALIR);

  tui_liberar();

  return 0;
}
