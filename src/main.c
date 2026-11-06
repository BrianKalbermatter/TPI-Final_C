#include <simulador.h>
#include <utilidades.h>
#include <validaciones.h>
#include <stdio.h>

static void mostrar_banner_bienvenida(void) {
  printf("╔══════════════════════════════════════════════════════════════════════╗\n");
  printf("║         SIMULADOR DE MISIONES ARTEMIS - SITUACIÓN 4                  ║\n");
  printf("║      Reingreso Atmosférico, Frenado Aerodinámico y Amerizaje         ║\n");
  printf("║            Cátedra de Algoritmos y Estructuras de Datos              ║\n");
  printf("╚══════════════════════════════════════════════════════════════════════╝\n");
}

static void mostrar_menu_principal(void) {
  printf("\n[ MENÚ PRINCIPAL ]\n");
  printf("1. Iniciar Simulación Interactiva (Modo Juego / Piloto)\n");
  printf("2. Análisis Teórico y Preguntas del TP (Caso Oficial Artemis)\n");
  printf("3. Ver Reporte y Telemetría de la Última Misión\n");
  printf("4. Manual de Vuelo y Fórmulas Físicas\n");
  printf("5. Integrantes del Equipo de Desarrollo\n");
  printf("6. Salir del Programa\n\n");
}

int main(void) {
  activar_utf8();
  log_set_min_level(LOG_LEVEL_INFO);

  MisionArtemis mision_actual;
  simulador_inicializar(&mision_actual);

  int opcion = 0;

  do {
    limpiar_pantalla();
    mostrar_banner_bienvenida();
    LOG_INFO("[HOUSTON] Canal de telemetria Orion en linea | Estado de vuelo: NOMINAL");
    mostrar_menu_principal();

    opcion = leer_entero_rango("Seleccione una opción (1-6): ", 1, 6);
    ASSERT(opcion >= 1 && opcion <= 6, "Opcion de menu fuera de rango valido");
    LOG_DEBUG("[CONSOLA] Comando ingresado por el operador: %d", opcion);

    switch (opcion) {
      case 1:
        limpiar_pantalla();
        LOG_INFO("[CAPCOM] Iniciando simulacion interactiva de vuelo...");
        LOG_INFO("[ORION-OBC] Verificando subsistemas de guiado, navegacion y control (GNC)...");
        printf("\n");
        simulador_ejecutar_modo_interactivo(&mision_actual);
        volver_atras();
        break;

      case 2:
        limpiar_pantalla();
        LOG_INFO("[FIDO] Cargando parametros orbitales y modelo fisico de la Situacion 4...");
        printf("\n");
        simulador_ejecutar_modo_teorico();
        volver_atras();
        break;

      case 3:
        limpiar_pantalla();
        LOG_INFO("[TELEMETRIA] Descargando registros de telemetria y sensores inerciales...");
        printf("\n");
        simulador_mostrar_reporte(&mision_actual);
        volver_atras();
        break;

      case 4:
        limpiar_pantalla();
        LOG_INFO("[MANUAL] Accediendo a los protocolos de dinamica de reingreso NASA/JSC...");
        printf("\n");
        simulador_mostrar_manual();
        volver_atras();
        break;

      case 5:
        limpiar_pantalla();
        LOG_INFO("[DIRECTOR DE VUELO] Convocando a los integrantes del equipo de desarrollo...");
        printf("\n");
        simulador_mostrar_integrantes();
        volver_atras();
        break;

      case 6:
        limpiar_pantalla();
        LOG_INFO("[HOUSTON] Cerrando enlace de datos con Orion. Mision finalizada.");
        printf("╔══════════════════════════════════════════════════════════════════════╗\n");
        printf("║     Gracias por utilizar el Simulador de Misiones Artemis.           ║\n");
        printf("║                      ¡Fin de la transmisión!                         ║\n");
        printf("╚══════════════════════════════════════════════════════════════════════╝\n\n");
        break;

      default:
        LOG_WARNING("[CONSOLA] Alerta: Opcion imprevista recibida en el centro de control: %d", opcion);
        break;
    }

  } while (opcion != 6);

  LOG_INFO("[SISTEMA] Terminal desconectada. Operacion concluida con exito.");
  return 0;
}
