#include "simulador.h"
#include "utilidades.h"
#include <stdio.h>
#include <string.h>

// ============================================================================
// STUBS DE LAS FIRMAS PARA EL CÁLCULO DE FÍSICAS (Situación 4)
// ============================================================================

double calcular_trabajo_rozamiento(double masa, double vi, double vf, double hi, double hf) {
  ASSERT(masa > 0.0, "La masa de la nave debe ser positiva");
  ASSERT(vi >= 0.0 && vf >= 0.0, "Las velocidades deben ser positivas o nulas");
  ASSERT(hi >= hf, "La altitud inicial de reingreso debe ser mayor o igual a la final");

  LOG_TRACE("[AERODINÁMICA] Integrando W_roz por pérdida de Em: m=%.1f kg, vi=%.1f m/s, vf=%.1f m/s, hi=%.1f m, hf=%.1f m",
            masa, vi, vf, hi, hf);

  // TODO: Implementar W_roz = Delta E_m = (K_f - K_i) + (U_f - U_i)
  return 0.0;
}

double calcular_desaceleracion_media(double vi, double vf, double delta_t) {
  ASSERT(delta_t > 0.0, "El tiempo delta_t debe ser estrictamente positivo");

  LOG_TRACE("[FIDO] Estimando desaceleración media: vi=%.1f m/s, vf=%.1f m/s, delta_t=%.1f s",
            vi, vf, delta_t);

  // TODO: Implementar a_med = |vf - vi| / delta_t
  return 0.0;
}

double calcular_fuerza_g(double aceleracion) {
  LOG_TRACE("[BIOMÉTRICA] Evaluando carga fisiológica para aceleración: %.2f m/s²", aceleracion);

  // TODO: Implementar Carga_G = aceleracion / g
  return 0.0;
}

double calcular_fuerza_impacto(double masa, double v_impacto, double distancia_frenado) {
  ASSERT(masa > 0.0, "La masa de la nave debe ser positiva");
  ASSERT(distancia_frenado > 0.0, "La distancia de penetración en el agua debe ser positiva");

  LOG_TRACE("[RECUPERACIÓN] Calculando impacto hidrodinámico: m=%.1f kg, v=%.1f m/s, d=%.2f m",
            masa, v_impacto, distancia_frenado);

  // TODO: Implementar F_prom = (m * v_impacto^2) / (2 * distancia_frenado)
  return 0.0;
}

double calcular_multiplo_peso(double fuerza, double masa) {
  ASSERT(masa > 0.0, "La masa de la nave debe ser positiva");

  LOG_TRACE("[ESTRUCTURAL] Relación de carga estática F/P: F=%.2f N, masa=%.1f kg", fuerza, masa);

  // TODO: Implementar Relacion = F / (m * g)
  return 0.0;
}

EstadoAngulo evaluar_angulo_reingreso(double gamma) {
  ASSERT(gamma > 0.0 && gamma < 90.0, "El angulo de reingreso gamma debe estar entre 0 y 90 grados");

  LOG_DEBUG("[RETRO] Comprobando ventana de reingreso: gamma=%.2f° (nominal seguro: 5.5° - 7.0°)", gamma);

  // TODO: Evaluar si gamma < 5.5 (rebote), entre 5.5 y 7.0 (optimo) o > 7.0 (destruccion)
  return ANGULO_OPTIMO;
}

// ============================================================================
// FUNCIONES DE CONTROL DEL SIMULADOR Y PANTALLAS
// ============================================================================

void simulador_inicializar(MisionArtemis *mision) {
  ASSERT(mision != NULL, "Puntero a MisionArtemis no puede ser nulo");
  if (mision == NULL) {
    LOG_ERROR("[ORION-OBC] Fallo crítico: puntero de telemetría MisionArtemis nulo");
    return;
  }

  memset(mision, 0, sizeof(MisionArtemis));
  mision->masa = MASA_ORION_KG;
  mision->angulo_reingreso = 6.2; // Valor nominal seguro dentro de [5.5, 7.0]
  mision->altitud = ALTITUD_REINGRESO_M;
  mision->velocidad = VELOCIDAD_REINGRESO_MS;
  mision->integridad_escudo = 100.0;
  mision->temperatura_escudo = 20.0;
  mision->paracaidas_desplegados = 0;
  mision->fase_actual = FASE_ENTRADA_ATMOSFERICA;
  mision->total_puntos = 0;
  mision->resultado = MISION_EN_CURSO;
  strcpy(mision->causa_fallo, "En curso");

  LOG_INFO("[ORION-OBC] Telemetría iniciada: masa=%.1f kg | h=%.1f m | v=%.1f m/s | gamma=%.1f°",
           mision->masa, mision->altitud, mision->velocidad, mision->angulo_reingreso);
}

void simulador_ejecutar_modo_interactivo(MisionArtemis *mision) {
  ASSERT(mision != NULL, "Puntero a MisionArtemis no puede ser nulo en modo interactivo");
  LOG_DEBUG("[ORION-OBC] Módulo de vuelo interactivo activado. Esperando comandos de guiado.");

  printf("╔══════════════════════════════════════════════════════════════════════╗\n");
  printf("║        MISIÓN ARTEMIS: RETORNO DE LA CÁPSULA ORION (MODO JUEGO)      ║\n");
  printf("╚══════════════════════════════════════════════════════════════════════╝\n\n");
  printf("[INFO] Modo interactivo en construccion.\n");
  printf("       Aqui se desarrollaran las 3 fases de decision del usuario:\n");
  printf("       1. Calibracion de angulo gamma (propulsores RCS).\n");
  printf("       2. Control de alabeo en frenado hipersonico (blackout de radio).\n");
  printf("       3. Despliegue de paracaidas y amerizaje en el Oceano Pacifico.\n");
}

void simulador_ejecutar_modo_teorico(void) {
  LOG_DEBUG("[FIDO] Módulo de análisis físico y cálculo analítico activado.");

  printf("╔══════════════════════════════════════════════════════════════════════╗\n");
  printf("║     ANÁLISIS TEÓRICO Y PREGUNTAS ANALÍTICAS - SITUACIÓN ARTEMIS 4    ║\n");
  printf("╚══════════════════════════════════════════════════════════════════════╝\n\n");
  printf("[INFO] Seccion analitica en construccion.\n");
  printf("       Aqui se mostrara la resolucion detallada de las preguntas:\n");
  printf("       a. Trabajo total de rozamiento (W_roz) desde 120 km a 2.5 km.\n");
  printf("       b. Desaceleracion media y fuerzas G durante delta_t = 400 s.\n");
  printf("       c. Fuerza promedio del agua en amerizaje y multiplo de peso.\n");
  printf("       d. Analisis fisico de la ventana del angulo de reingreso gamma.\n");
}

void simulador_mostrar_reporte(const MisionArtemis *mision) {
  ASSERT(mision != NULL, "Puntero a MisionArtemis no puede ser nulo al mostrar reporte");
  LOG_DEBUG("[TELEMETRÍA] Descarga de registros de caja negra (VDR) completada.");

  printf("╔══════════════════════════════════════════════════════════════════════╗\n");
  printf("║                REPORTE FINAL DE TELEMETRÍA DE LA MISIÓN              ║\n");
  printf("╚══════════════════════════════════════════════════════════════════════╝\n\n");
  printf("[INFO] Reporte en construccion. Mostrara la telemetria acumulada\n");
  printf("       en el arreglo de estructuras PuntoTelemetria y el estado final.\n");
}

void simulador_mostrar_manual(void) {
  LOG_DEBUG("[MANUAL] Visualización de especificaciones técnicas y fundamentos físicos.");

  printf("╔══════════════════════════════════════════════════════════════════════╗\n");
  printf("║              MANUAL DE OPERACIONES Y FÍSICA DE REINGRESO             ║\n");
  printf("╚══════════════════════════════════════════════════════════════════════╝\n\n");
  printf("1. CONSERVACIÓN Y TEOREMA DE TRABAJO Y ENERGÍA:\n");
  printf("   Delta E_m = W_fuerzas_no_conservativas = W_roz\n");
  printf("   E_m = (1/2) * m * v^2 + m * g * h\n\n");
  printf("2. DESACELERACIÓN MEDIA Y CARGAS G:\n");
  printf("   a_med = (v_f - v_i) / delta_t\n");
  printf("   Carga G = |a_med| / 9.80 m/s^2\n\n");
  printf("3. IMPACTO EN EL AGUA (AMERIZAJE):\n");
  printf("   W_frenado = Delta K => -F_prom * d = 0 - (1/2) * m * v_impacto^2\n");
  printf("   F_prom = (m * v_impacto^2) / (2 * d)\n\n");
  printf("4. VENTANA DE REINGRESO:\n");
  printf("   5.5 deg <= gamma <= 7.0 deg\n");
  printf("   - gamma < 5.5 deg: Rebote en la atmosfera (perdida en el espacio)\n");
  printf("   - gamma > 7.0 deg: Reingreso escarpado (destruccion termica y sobrecarga de Gs)\n");
}

void simulador_mostrar_integrantes(void) {
  LOG_DEBUG("[DIRECTOR DE VUELO] Consola de personal de soporte y desarrollo en pantalla.");

  printf("╔══════════════════════════════════════════════════════════════════════╗\n");
  printf("║               INTEGRANTES DEL EQUIPO DE DESARROLLO                   ║\n");
  printf("╚══════════════════════════════════════════════════════════════════════╝\n\n");
  printf(" Institucion: Universidad Tecnologica Nacional (UTN FRRe)\n");
  printf(" Asignatura:  Algoritmos y Estructuras de Datos\n");
  printf(" Carrera:     Ingenieria en Sistemas de Informacion\n");
  printf(" Proyecto:    Simulador de Misiones Artemis - Situacion 4\n\n");
  printf(" ┌─────────────────────────┬─────────────────────────┐\n");
  printf(" │ Integrante              │ Usuario GitHub          │\n");
  printf(" ├─────────────────────────┼─────────────────────────┤\n");
  printf(" │ Brian Kalbermatter      │ @BrianKalbermatter      │\n");
  printf(" │ Emiliano Elst           │ @emilianoelst           │\n");
  printf(" │ Enzo Solis              │ @EnzoSolis              │\n");
  printf(" │ Ezequiel (EzXdlol)      │ @EzXdlol                │\n");
  printf(" └─────────────────────────┴─────────────────────────┘\n");
}
