/*
 * simulador.c
 * Simulador de Misiones Artemis - Trabajo Practico Integrador (UTN FRRe)
 * Escenario: Reingreso Atmosferico, Frenado Aerodinamico y Amerizaje
 *
 * Todos los calculos fisicos se hacen aqui, a partir de los datos
 * recibidos por parametro. Las constantes estan en simulador.h.
 *
 * Convencion de errores: si los datos son fisicamente imposibles, la
 * funcion informa el motivo por stderr y devuelve NAN en lugar de
 * calcular. El llamador verifica con isnan().
 */

#include "simulador.h"
#include "utilidades.h"
#include "validaciones.h"
#include "tui.h"
#ifdef MOUSE_MOVED
#undef MOUSE_MOVED
#endif
#include <curses.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

// ============================================================================
// FIRMAS PARA EL CÁLCULO DE FÍSICAS (Situación 4)
// ============================================================================
// W_roz = Delta E_m = (K_f - K_i) + (U_f - U_i)
// a_med = |vf - vi| / delta_t
// Carga_G = aceleracion / g
// F_prom = (m * v_impacto^2) / (2 * distancia_frenado)
// Relacion = F / (m * g)

/* ================================================================== */
/* Pregunta 1                                                          */
/* ================================================================== */
/*
 * Trabajo de la fuerza de rozamiento (aerodinamica) durante el reingreso.
 *
 * Fisica: teorema trabajo-energia (suma de trabajos = variacion de Ec):
 *     Wroz + Wg = DeltaEc
 * con
 *     DeltaEc = 1/2 m (vf^2 - vi^2)
 *     Wg      = m g (hi - hf)     (positivo: la nave baja y la gravedad
 *                                  aporta trabajo a favor del movimiento)
 * Despejando:
 *     Wroz = DeltaEc - Wg
 *
 * Wroz sale negativo: el rozamiento le quita energia a la nave (se
 * transforma en calor en el escudo termico y el aire).
 */
double calcular_trabajo_rozamiento(double masa, double vi, double vf,
                                 double hi, double hf)
{
    ASSERT(masa > 0.0, "La masa de la nave debe ser positiva");
    ASSERT(vi >= 0.0 && vf >= 0.0, "Las velocidades deben ser positivas o nulas");
    ASSERT(hi >= hf, "La altitud inicial de reingreso debe ser mayor o igual a la final");

    LOG_TRACE("[AERODINÁMICA] Integrando W_roz por pérdida de Em: m=%.1f kg, vi=%.1f m/s, vf=%.1f m/s, hi=%.1f m, hf=%.1f m",
              masa, vi, vf, hi, hf);

    double deltaEc;
    double trabajoGravedad;
    double wroz;

    /* --- Validaciones --- */
    if (!(masa > 0.0)) {
        fprintf(stderr, "Error: la masa debe ser mayor que 0.\n");
        return NAN;
    }
    if (vi < 0.0 || vf < 0.0) {
        fprintf(stderr, "Error: las velocidades (modulo) no pueden ser negativas.\n");
        return NAN;
    }
    if (!(hi > hf)) {
        fprintf(stderr, "Error: la altitud inicial debe ser mayor que la final (hi > hf).\n");
        return NAN;
    }
    if (hf < 0.0) {
        fprintf(stderr, "Error: la altitud final no puede ser negativa.\n");
        return NAN;
    }

    /* --- Calculo --- */
    deltaEc         = 0.5 * masa * (vf * vf - vi * vi);
    trabajoGravedad = masa * GRAVEDAD_TIERRA * (hi - hf);
    wroz            = deltaEc - trabajoGravedad;

    /* El rozamiento solo puede disipar energia: Wroz > 0 es imposible. */
    if (wroz > 0.0) {
        fprintf(stderr, "Error: Wroz > 0, el rozamiento no puede entregar energia a la nave. "
                        "Datos fisicamente inconsistentes.\n");
        return NAN;
    }

    return wroz;
}

/* ================================================================== */
/* Pregunta 2                                                          */
/* ================================================================== */
/*
 * Desaceleracion media: a = (vf - vi) / t
 * Es la aceleracion constante que produciria el mismo cambio de
 * velocidad en el mismo tiempo. Negativa cuando la nave frena.
 */
double calcular_desaceleracion_media(double vi, double vf, double tiempo)
{
    ASSERT(tiempo > 0.0, "El tiempo delta_t debe ser estrictamente positivo");

    LOG_TRACE("[FIDO] Estimando desaceleración media: vi=%.1f m/s, vf=%.1f m/s, delta_t=%.1f s",
              vi, vf, tiempo);

    if (!(tiempo > 0.0)) {
        fprintf(stderr, "Error: el tiempo de desaceleracion debe ser mayor que 0.\n");
        return NAN;
    }
    if (vi < 0.0 || vf < 0.0) {
        fprintf(stderr, "Error: las velocidades (modulo) no pueden ser negativas.\n");
        return NAN;
    }
    return (vf - vi) / tiempo;
}

/*
 * Factor de carga en "g": gCarga = |a| / g
 * Indica cuantas veces la aceleracion supera a la gravedad terrestre;
 * es la medida habitual de la exigencia sobre la tripulacion.
 */
double calcular_fuerza_g(double aceleracion)
{
    LOG_TRACE("[BIOMÉTRICA] Evaluando carga fisiológica para aceleración: %.2f m/s²", aceleracion);

    if (isnan(aceleracion)) {
        fprintf(stderr, "Error: aceleracion invalida, no se calcula el factor g.\n");
        return NAN;
    }
    return fabs(aceleracion) / GRAVEDAD_TIERRA;
}

/* ================================================================== */
/* Pregunta 3                                                          */
/* ================================================================== */
/*
 * Fuerza media de frenado al amerizar.
 *
 * Fisica: la nave se detiene en una distancia d; el trabajo de la fuerza
 * media de frenado iguala a la energia cinetica que tenia al impactar:
 *     Fprom * d = 1/2 m v^2   =>   Fprom = (1/2 m v^2) / d
 */
double calcular_fuerza_impacto(double masa, double velocidadImpacto,
                               double distanciaFrenado)
{
    ASSERT(masa > 0.0, "La masa de la nave debe ser positiva");
    ASSERT(distanciaFrenado > 0.0, "La distancia de penetración en el agua debe ser positiva");

    LOG_TRACE("[RECUPERACIÓN] Calculando impacto hidrodinámico: m=%.1f kg, v=%.1f m/s, d=%.2f m",
              masa, velocidadImpacto, distanciaFrenado);

    if (!(masa > 0.0)) {
        fprintf(stderr, "Error: la masa debe ser mayor que 0.\n");
        return NAN;
    }
    if (velocidadImpacto < 0.0) {
        fprintf(stderr, "Error: la velocidad de impacto no puede ser negativa.\n");
        return NAN;
    }
    if (!(distanciaFrenado > 0.0)) {
        fprintf(stderr, "Error: la distancia de frenado debe ser mayor que 0 "
                        "(d = 0 implicaria fuerza infinita).\n");
        return NAN;
    }
    return (0.5 * masa * velocidadImpacto * velocidadImpacto) / distanciaFrenado;
}

/*
 * Cuantas veces la fuerza de amerizaje equivale al peso de la nave:
 *     multiplo = Fprom / (m g)
 */
double calcular_multiplo_peso(double fuerza, double masa)
{
    ASSERT(masa > 0.0, "La masa de la nave debe ser positiva");

    LOG_TRACE("[ESTRUCTURAL] Relación de carga estática F/P: F=%.2f N, masa=%.1f kg", fuerza, masa);

    if (isnan(fuerza)) {
        fprintf(stderr, "Error: fuerza invalida, no se calcula el multiplo del peso.\n");
        return NAN;
    }
    if (!(masa > 0.0)) {
        fprintf(stderr, "Error: la masa debe ser mayor que 0.\n");
        return NAN;
    }
    return fuerza / (masa * GRAVEDAD_TIERRA);
}

/* ================================================================== */
/* Pregunta 4                                                          */
/* ================================================================== */
/*
 * Analisis cualitativo del angulo de reingreso gamma (en grados,
 * medido respecto del horizonte local). Ventana segura: 5.5 a 7.0.
 */
void analizarAnguloReingreso(double gamma)
{
    /* Un angulo de entrada solo tiene sentido entre 0 (exclusivo) y 90 grados. */
    if (isnan(gamma) || gamma <= 0.0 || gamma > 90.0) {
        LOG_ERROR("Error: el angulo de reingreso debe estar en (0, 90] grados.");
        return;
    }

    printw("Angulo de reingreso: %.2f grados\n", gamma);

    if (gamma > GAMMA_MAX) {
        printw("  Angulo demasiado pronunciado:\n");
        printw("   - desaceleracion excesiva\n");
        printw("   - calentamiento extremo\n");
        printw("   - riesgo estructural\n");
        printw("   - peligro para la tripulacion\n");
    } else if (gamma < GAMMA_MIN) {
        printw("  Angulo demasiado superficial:\n");
        printw("   - rebote atmosferico\n");
        printw("   - perdida de frenado\n");
        printw("   - posible retorno al espacio\n");
        printw("   - desviacion de la zona de amerizaje\n");
    } else {
        printw("  Trayectoria segura\n");
    }
}

EstadoAngulo evaluar_angulo_reingreso(double gamma) {
  ASSERT(gamma > 0.0 && gamma < 90.0, "El angulo de reingreso gamma debe estar entre 0 y 90 grados");

  LOG_DEBUG("[RETRO] Comprobando ventana de reingreso: gamma=%.2f° (nominal seguro: 5.5° - 7.0°)", gamma);

  // Evaluar si gamma < 5.5 (rebote), entre 5.5 y 7.0 (optimo) o > 7.0 (destruccion)
  if (gamma < ANGULO_REINGRESO_MIN) return ANGULO_MUY_PLANO_REBOTE;
  if (gamma > ANGULO_REINGRESO_MAX) return ANGULO_MUY_EMPINADO_DESTRUCCION;
  return ANGULO_OPTIMO;
}

/* ================================================================== */
/* Funcion principal del escenario                                     */
/* ================================================================== */
/*
 * Resuelve las preguntas 1 a 3 y guarda los resultados en la estructura
 * apuntada por "resultado". Si un dato es invalido, el campo
 * correspondiente queda en NAN (y los dependientes tambien).
 */
void resolverEscenarioArtemis(ResultadoArtemis *resultado,
                              double masa, double vi, double vf,
                              double hi, double hf, double tiempo,
                              double velocidadImpacto,
                              double distanciaFrenado)
{
    if (resultado == NULL) {
        fprintf(stderr, "Error: puntero a resultado nulo.\n");
        return;
    }

    /* Pregunta 1 */
    resultado->trabajoRozamiento =
        calcular_trabajo_rozamiento(masa, vi, vf, hi, hf);

    /* Pregunta 2 */
    resultado->desaceleracion =
        calcular_desaceleracion_media(vi, vf, tiempo);
    resultado->factorG = calcular_fuerza_g(resultado->desaceleracion);

    /* Pregunta 3 */
    resultado->fuerzaAmerizaje =
        calcular_fuerza_impacto(masa, velocidadImpacto, distanciaFrenado);
    resultado->multiploPeso =
        calcular_multiplo_peso(resultado->fuerzaAmerizaje, masa);
}

// ============================================================================
// FUNCIONES DE CONTROL DEL SIMULADOR Y PANTALLAS
// ESTADO DE LA MISION
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

static FaseVuelo fase_segun_altitud(double altitud) {
  if (altitud > ALTITUD_FIN_ENTRADA_M) return FASE_ENTRADA_ATMOSFERICA;
  if (altitud > ALTITUD_PARACAIDAS_M) return FASE_FRENADO_HIPERSONICO;
  if (altitud > 0.0) return FASE_PARACAIDAS;
  return FASE_AMERIZAJE;
}

void guardar_turno_historial(MisionArtemis *mision, double angulo, double carga_g,
                             double w_roz, double dt, const char *evento) {
  if (mision->total_puntos >= MAX_HISTORIAL_TELEMETRIA) return;

  PuntoTelemetria *punto = &mision->historial[mision->total_puntos];
  punto->tiempo = (mision->total_puntos + 1) * dt;
  punto->altitud = mision->altitud;
  punto->velocidad = mision->velocidad;
  punto->angulo_gamma = angulo;
  punto->aceleracion_g = carga_g;
  punto->trabajo_rozamiento = w_roz;
  punto->energia_mecanica = 0.5 * mision->masa * mision->velocidad * mision->velocidad +
                            mision->masa * GRAVEDAD_TIERRA * mision->altitud;
  punto->fase = fase_segun_altitud(mision->altitud);
  snprintf(punto->evento, sizeof(punto->evento), "%s", evento);
  mision->fase_actual = punto->fase;
  mision->total_puntos++;
}

static void cerrar_mision(MisionArtemis *mision, ResultadoMision resultado, const char *causa) {
  mision->resultado = resultado;
  mision->fase_actual = FASE_FINALIZADA;
  snprintf(mision->causa_fallo, sizeof(mision->causa_fallo), "%s", causa);
  mision->desaceleracion_media = calcular_desaceleracion_media(
      VELOCIDAD_REINGRESO_MS, mision->velocidad, mision->historial[mision->total_puntos - 1].tiempo);

  if (resultado == MISION_EXITOSA) {
    LOG_INFO("[HOUSTON] %s", causa);
  } else {
    LOG_ERROR("[TRAYECTORIA] %s", causa);
  }
}

void actualizar_estado_turno(MisionArtemis *mision, double angulo, int accion_paracaidas, double dt) {
  ASSERT(mision != NULL, "Puntero a MisionArtemis no puede ser nulo");
  ASSERT(dt > 0.0, "El tiempo de turno debe ser positivo");

  double v_inicial = mision->velocidad;
  double h_inicial = mision->altitud;
  double v_nueva = v_inicial;
  double dh = 0.0;
  char evento[TAM_EVENTO] = "Frenado aerodinamico nominal";

  mision->angulo_reingreso = angulo;

  if (mision->altitud > ALTITUD_FRENADO_M) {
    EstadoAngulo estado_angulo = evaluar_angulo_reingreso(angulo);

    if (estado_angulo == ANGULO_MUY_PLANO_REBOTE) {
      guardar_turno_historial(mision, angulo, 0.0, 0.0, dt, "CRITICO: Rebote atmosferico (gamma < 5.5)");
      cerrar_mision(mision, MISION_FALLO_REBOTE, "Rebote atmosferico: la nave se perdio en el espacio");
      return;
    }

    if (estado_angulo == ANGULO_MUY_EMPINADO_DESTRUCCION) {
      mision->integridad_escudo -= DANO_ESCUDO_EMPINADO;
      v_nueva = v_inicial * FRENADO_EMPINADO;
      dh = DESCENSO_EMPINADO_M;
      strcpy(evento, "ALERTA: Sobrecarga G y dano termico (gamma > 7.0)");
    } else {
      v_nueva = v_inicial * (FRENADO_BASE + (angulo - ANGULO_REINGRESO_MIN) * FRENADO_POR_GRADO);
      dh = (mision->altitud > ALTITUD_TRANSICION_M) ? DESCENSO_ALTO_M : DESCENSO_BAJO_M;
      mision->integridad_escudo -= DANO_ESCUDO_NOMINAL;
    }
  } else {
    if (accion_paracaidas) {
      if (mision->velocidad > VELOCIDAD_MAX_PARACAIDAS_MS) {
        mision->integridad_escudo -= 50.0;
        strcpy(evento, "Paracaidas rasgados por exceso de velocidad");
        v_nueva = 45.0;
      } else {
        mision->paracaidas_desplegados = 1;
        v_nueva = VELOCIDAD_AMERIZAJE_MS;
        strcpy(evento, "Paracaidas principales desplegados con exito");
      }
    } else {
      v_nueva = mision->paracaidas_desplegados ? VELOCIDAD_AMERIZAJE_MS : VELOCIDAD_PARACAIDAS_MS;
      strcpy(evento, "Descenso final hacia el Oceano Pacifico");
    }
    dh = mision->altitud;
  }

  // Al llegar a la altitud de paracaidas la nave queda a la rapidez del enunciado.
  if (mision->altitud - dh <= ALTITUD_PARACAIDAS_M && mision->altitud > ALTITUD_PARACAIDAS_M) {
    mision->altitud = ALTITUD_PARACAIDAS_M;
    v_nueva = VELOCIDAD_PARACAIDAS_MS;
  } else {
    mision->altitud -= dh;
    if (mision->altitud < 0.0) mision->altitud = 0.0;
  }

  double w_turno = calcular_trabajo_rozamiento(mision->masa, v_inicial, v_nueva, h_inicial, mision->altitud);
  if (!isnan(w_turno)) mision->trabajo_rozamiento += w_turno;

  double carga_g = calcular_fuerza_g(calcular_desaceleracion_media(v_inicial, v_nueva, dt));
  if (carga_g > mision->desaceleracion_max_g) mision->desaceleracion_max_g = carga_g;

  mision->velocidad = v_nueva;

  if (mision->integridad_escudo <= 0.0 || carga_g > CARGA_G_MAXIMA) {
    ResultadoMision fallo = (mision->integridad_escudo <= 0.0) ? MISION_FALLO_TERMICO : MISION_FALLO_GS;
    mision->integridad_escudo = 0.0;
    guardar_turno_historial(mision, angulo, carga_g, w_turno, dt, "CATACLISMO: Fallo estructural / Escudo destruido");
    cerrar_mision(mision, fallo, "Fallo estructural: la nave se desintegro");
    return;
  }

  if (mision->altitud <= 0.0) {
    mision->fuerza_impacto_n = calcular_fuerza_impacto(mision->masa, mision->velocidad, DISTANCIA_HUNDIMIENTO_M);
    mision->fuerza_impacto_multiplo_peso = calcular_multiplo_peso(mision->fuerza_impacto_n, mision->masa);

    if (mision->paracaidas_desplegados == 1 && mision->velocidad <= 12.0) {
      guardar_turno_historial(mision, angulo, carga_g, w_turno, dt, "AMERIZAJE EXITOSO en el Oceano Pacifico");
      cerrar_mision(mision, MISION_EXITOSA, "Amerizaje exitoso: tripulacion a salvo");
    } else {
      guardar_turno_historial(mision, angulo, carga_g, w_turno, dt, "IMPACTO CRITICO: Sin paracaidas en el amerizaje");
      cerrar_mision(mision, MISION_FALLO_IMPACTO, "Impacto contra el oceano sin paracaidas");
    }
    return;
  }

  guardar_turno_historial(mision, angulo, carga_g, w_turno, dt, evento);
}

// ============================================================================
// MODOS DE EJECUCION Y PANTALLAS
// ============================================================================

void simulador_ejecutar_modo_interactivo(MisionArtemis *mision) {
  ASSERT(mision != NULL, "Puntero a MisionArtemis no puede ser nulo en modo interactivo");
  LOG_DEBUG("[ORION-OBC] Módulo de vuelo interactivo activado en curses.");
  tui_simulacion_interactiva(mision);
}

void simulador_ejecutar_modo_teorico(void) {
  LOG_DEBUG("[FIDO] Módulo de análisis físico y cálculo analítico activado en curses.");
  tui_analisis_teorico();
}

void simulador_mostrar_reporte(const MisionArtemis *mision) {
  ASSERT(mision != NULL, "Puntero a MisionArtemis no puede ser nulo al mostrar reporte");
  LOG_DEBUG("[TELEMETRÍA] Descarga de registros de caja negra (VDR) en curses.");
  tui_reporte_telemetria(mision);
}

void simulador_mostrar_manual(void) {
  LOG_DEBUG("[MANUAL] Visualización de especificaciones técnicas en curses.");
  tui_manual_vuelo();
}

void simulador_mostrar_integrantes(void) {
  LOG_DEBUG("[DIRECTOR DE VUELO] Consola de personal de desarrollo en curses.");
  tui_integrantes();
}
