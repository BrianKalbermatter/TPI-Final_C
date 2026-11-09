/**
 * @file simulador.h
 * @brief Definiciones, constantes, estructuras y firmas del Simulador de Misión Artemis (Situación 4).
 * @date 2026-11-06
 */
/*
 * simulador.h
 * Simulador de Misiones Artemis - Trabajo Practico Integrador (UTN FRRe)
 * Escenario: Reingreso Atmosferico, Frenado Aerodinamico y Amerizaje
 *            (Trabajo, Energia e Impacto)
 *
 * Contiene: constantes fisicas, la estructura de resultados y los
 * prototipos de todas las funciones. Compatible con ANSI C / C99.
 */
#ifndef SIMULADOR_H
#define SIMULADOR_H

// Constantes del Escenario 4: Reingreso Atmosferico y Amerizaje
// ALTITUD_REINGRESO_M: 120 km en metros
// VELOCIDAD_REINGRESO_MS: 11 km/s en m/s
// ALTITUD_PARACAIDAS_M: 2.5 km en metros
// GRAVEDAD_TIERRA: m/s^2

/* Aceleracion de la gravedad terrestre [m/s^2]. Se la considera
 * constante durante todo el reingreso (simplificacion del enunciado). */
// -- Parámetros físicos constantes de la Situación 4 --
#define MASA_ORION_KG               10400.0   /**< Masa del módulo de tripulación Orion (kg). */
#define GRAVEDAD_TIERRA             9.80      /**< Aceleración gravitatoria g (m/s²). */
#define ALTITUD_REINGRESO_M         120000.0  /**< Altitud en la interfaz de reingreso (120 km). */
#define VELOCIDAD_REINGRESO_MS      11000.0   /**< Rapidez inicial de reingreso (11.000 m/s ~ Mach 32). */
#define ALTITUD_PARACAIDAS_M        2500.0    /**< Altitud de apertura de paracaídas principales (2.500 m). */
#define VELOCIDAD_PARACAIDAS_MS     25.0      /**< Rapidez al desplegar paracaídas (25,0 m/s). */
#define VELOCIDAD_AMERIZAJE_MS      8.0       /**< Rapidez final de impacto/amerizaje (8,0 m/s). */
#define DISTANCIA_HUNDIMIENTO_M     1.50      /**< Distancia de penetración/frenado en el agua (1,50 m). */
#define DELTA_T_DESACELERACION_S    400.0     /**< Duración de fase de mayor desaceleración (400 s). */
#define ANGULO_REINGRESO_MIN        5.5       /**< Límite inferior seguro de ventana de entrada (°). */
#define ANGULO_REINGRESO_MAX        7.0       /**< Límite superior seguro de ventana de entrada (°). */
#define MAX_HISTORIAL_TELEMETRIA    100       /**< Capacidad máxima de muestras de telemetría. */
#define TAM_EVENTO                  80        /**< Tamaño de la descripción de un turno. */

// -- Parámetros de la mecánica por turnos --
#define DT_TURNO_S                  60.0      /**< Duración de cada turno de juego (s). */
#define ALTITUD_FIN_ENTRADA_M       60000.0   /**< Fin de la fase de entrada atmosférica (m). */
#define ALTITUD_TRANSICION_M        40000.0   /**< Debajo de esta altitud el descenso por turno es menor (m). */
#define ALTITUD_FRENADO_M           10000.0   /**< Debajo de esta altitud se decide el paracaídas (m). */
#define DESCENSO_ALTO_M             55000.0   /**< Descenso por turno sobre la altitud de transición (m). */
#define DESCENSO_BAJO_M             37500.0   /**< Descenso por turno bajo la altitud de transición (m). */
#define DESCENSO_EMPINADO_M         45000.0   /**< Descenso por turno con gamma excesivo (m). */
#define FRENADO_BASE                0.22      /**< Fracción de velocidad conservada con gamma mínimo. */
#define FRENADO_POR_GRADO           0.05      /**< Fracción adicional conservada por grado de gamma. */
#define FRENADO_EMPINADO            0.20      /**< Fracción de velocidad conservada con gamma excesivo. */
#define DANO_ESCUDO_NOMINAL         12.0      /**< Daño de escudo por turno nominal (%). */
#define DANO_ESCUDO_EMPINADO        65.0      /**< Daño de escudo por turno con gamma excesivo (%). */
#define VELOCIDAD_MAX_PARACAIDAS_MS 120.0     /**< Rapidez máxima para abrir paracaídas (m/s). */
#define CARGA_G_MAXIMA              15.0      /**< Carga máxima soportable por la tripulación (g). */

/* Ventana segura del angulo de reingreso [grados] */
#define GAMMA_MIN ANGULO_REINGRESO_MIN
#define GAMMA_MAX ANGULO_REINGRESO_MAX

/**
 * @brief Fases del descenso y reingreso atmosférico.
 */
typedef enum {
  FASE_ENTRADA_ATMOSFERICA = 0,
  FASE_FRENADO_HIPERSONICO,
  FASE_PARACAIDAS,
  FASE_AMERIZAJE,
  FASE_FINALIZADA
} FaseVuelo;

// Estados posibles de la mision
/**
 * @brief Estados finales de la misión.
 */
typedef enum {
  MISION_EN_CURSO = 0,
  MISION_EXITOSA,
  MISION_FALLO_REBOTE,
  MISION_FALLO_TERMICO,
  MISION_FALLO_GS,
  MISION_FALLO_PARACAIDAS,
  MISION_FALLO_IMPACTO
} ResultadoMision;

/**
 * @brief Clasificación de la ventana del ángulo de reingreso.
 */
typedef enum {
  ANGULO_OPTIMO = 0,
  ANGULO_MUY_PLANO_REBOTE,
  ANGULO_MUY_EMPINADO_DESTRUCCION
} EstadoAngulo;

// Estructura para cada posicion del arreglo de historial de turnos
/**
 * @brief Muestra individual de telemetría de vuelo.
 */
typedef struct {
  double tiempo;              /**< Tiempo transcurrido (s). */
  double altitud;             /**< Altitud actual (m). */
  double velocidad;           /**< Rapidez actual (m/s). */
  double aceleracion_g;       /**< Aceleración experimentada en múltiplos de g. */
  double energia_mecanica;    /**< Energía mecánica del sistema (J). */
  FaseVuelo fase;             /**< Fase del vuelo correspondiente. */
  double angulo_gamma;        /**< Ángulo de guiado usado en el turno (°). */
  double trabajo_rozamiento;  /**< Trabajo de rozamiento del turno (J). */
  char evento[TAM_EVENTO];    /**< Descripción de lo ocurrido en el turno. */
} PuntoTelemetria;

// Estructura principal del estado de la mision
/**
 * @brief Estado integral de la cápsula Orion y de la simulación.
 */
typedef struct {
  // Parámetros y condiciones iniciales
  double masa;                             /**< Masa de la cápsula (kg). */
  double angulo_reingreso;                 /**< Ángulo de trayectoria gamma (°). */

  // Variables dinámicas
  double altitud;                          /**< Altitud actual (m). */
  double velocidad;                        /**< Rapidez actual (m/s). */
  double integridad_escudo;                /**< Salud estructural del escudo térmico (0 a 100 %). */
  double temperatura_escudo;               /**< Temperatura superficial del escudo (°C). */
  int paracaidas_desplegados;              /**< 1 si fueron desplegados, 0 si no. */
  FaseVuelo fase_actual;                   /**< Fase actual de la simulación. */

  // Métricas físicas calculadas
  double trabajo_rozamiento;               /**< Trabajo total de fricción W_roz (J). */
  double desaceleracion_media;             /**< Desaceleración media calculada (m/s²). */
  double desaceleracion_max_g;             /**< Carga G máxima registrada. */
  double fuerza_impacto_n;                 /**< Fuerza media ejercida por el agua en el amerizaje (N). */
  double fuerza_impacto_multiplo_peso;     /**< Fuerza de amerizaje como múltiplo del peso de la nave. */

  // Historial de telemetría
  PuntoTelemetria historial[MAX_HISTORIAL_TELEMETRIA];
  int total_puntos;

  // Resultado
  ResultadoMision resultado;
  char causa_fallo[128];
} MisionArtemis;

/**
 * @brief Datos de entrada de las preguntas analíticas del escenario.
 */
typedef struct {
  double masa;               /**< Masa de la nave (kg). */
  double vi;                 /**< Rapidez inicial (m/s). */
  double vf;                 /**< Rapidez final (m/s). */
  double hi;                 /**< Altitud inicial (m). */
  double hf;                 /**< Altitud final (m). */
  double delta_t;            /**< Duración de la desaceleración (s). */
  double v_impacto;          /**< Rapidez de impacto contra el agua (m/s). */
  double distancia_frenado;  /**< Distancia de frenado en el agua (m). */
  double gamma;              /**< Ángulo de reingreso (°). */
} DatosEscenario;

/* ------------------------------------------------------------------ */
/* Estructura de resultados                                            */
/* ------------------------------------------------------------------ */
typedef struct {
    double trabajoRozamiento;  /* Wroz  [J]  (negativo: energia disipada) */
    double desaceleracion;     /* a     [m/s^2] (negativa: frena)         */
    double factorG;            /* |a|/g [adimensional]                    */
    double fuerzaAmerizaje;    /* Fprom [N]                               */
    double multiploPeso;       /* Fprom / (m g) [adimensional]            */
} ResultadoArtemis;

/* Marca de resultado invalido: se usa NAN (C99, <math.h>) cuando una
 * funcion detecta un caso fisicamente imposible. Verificar con isnan(). */

// ============================================================================
// FIRMAS DE FUNCIONES PARA EL CÁLCULO DE FÍSICAS (Situación 4)
// ============================================================================

/**
 * @brief (Pregunta a) Calcula el trabajo total de rozamiento atmosférico (W_roz) mediante Delta Em.
 * @param masa Masa de la nave (kg).
 * @param vi Rapidez inicial en interfase de reingreso (m/s).
 * @param vf Rapidez al momento de apertura de paracaídas (m/s).
 * @param hi Altitud inicial de reingreso (m).
 * @param hf Altitud al momento de apertura de paracaídas (m).
 * @return Trabajo de la fuerza de rozamiento (J).
 */
double calcular_trabajo_rozamiento(double masa, double vi, double vf, double hi, double hf);

/**
 * @brief (Pregunta b) Determina la desaceleración media durante la fase de frenado intenso.
 * @param vi Rapidez al inicio del intervalo (m/s).
 * @param vf Rapidez al final del intervalo (m/s).
 * @param delta_t Duración del intervalo de desaceleración (s).
 * @return Desaceleración media en valor escalar positivo (m/s²).
 */
double calcular_desaceleracion_media(double vi, double vf, double delta_t);

/**
 * @brief Convierte una aceleración o desaceleración en múltiplos de la gravedad terrestre (g).
 * @param aceleracion Magnitud de aceleración (m/s²).
 * @return Carga en g (adimensional).
 */
double calcular_fuerza_g(double aceleracion);

/**
 * @brief (Pregunta c) Determina la fuerza promedio que el agua ejerce sobre la nave al amerizar.
 * @param masa Masa de la nave (kg).
 * @param v_impacto Rapidez al momento del choque con el agua (m/s).
 * @param distancia_frenado Distancia que penetra la cápsula en el agua (m).
 * @return Fuerza media de frenado (N).
 */
double calcular_fuerza_impacto(double masa, double v_impacto, double distancia_frenado);

/**
 * @brief Expresa una fuerza como múltiplo del peso gravitatorio de la nave (P = m * g).
 * @param fuerza Fuerza en Newtons (N).
 * @param masa Masa de la nave en kilogramos (kg).
 * @return Relación adimensional Fuerza / Peso.
 */
double calcular_multiplo_peso(double fuerza, double masa);

/**
 * @brief (Pregunta d) Evalúa la viabilidad del ángulo de reingreso atmosférico gamma.
 * @param gamma Ángulo de reingreso en grados sexagesimales (°).
 * @return ANGULO_OPTIMO, ANGULO_MUY_PLANO_REBOTE o ANGULO_MUY_EMPINADO_DESTRUCCION.
 */
EstadoAngulo evaluar_angulo_reingreso(double gamma);

/* ------------------------------------------------------------------ */
/* Pregunta 4: analisis del angulo de reingreso                        */
/* ------------------------------------------------------------------ */
void analizarAnguloReingreso(double gamma);

/* ------------------------------------------------------------------ */
/* Funcion principal: completa la estructura mediante un puntero       */
/* ------------------------------------------------------------------ */
void resolverEscenarioArtemis(ResultadoArtemis *resultado,
                              double masa, double vi, double vf,
                              double hi, double hf, double tiempo,
                              double velocidadImpacto,
                              double distanciaFrenado);

// ============================================================================
// FIRMAS DE CONTROL DEL SIMULADOR Y MODOS DE EJECUCIÓN
// ============================================================================

/**
 * @brief Inicializa el estado de la misión con los valores predeterminados de Artemis 4.
 * @param mision Puntero a la estructura de estado.
 */
void simulador_inicializar(MisionArtemis *mision);

// Prototipos publicos del simulador (Ticket #5)
void actualizar_estado_turno(MisionArtemis *mision, double angulo, int accion_paracaidas, double dt);
void guardar_turno_historial(MisionArtemis *mision, double angulo, double carga_g,
                             double w_roz, double dt, const char *evento);

/**
 * @brief Ejecuta el modo de juego interactivo por fases y decisiones.
 * @param mision Puntero a la estructura de estado.
 */
void simulador_ejecutar_modo_interactivo(MisionArtemis *mision);

/**
 * @brief Ejecuta la resolución analítica directa de las preguntas teóricas del TP.
 */
void simulador_ejecutar_modo_teorico(void);

/**
 * @brief Muestra el reporte final detallado de desempeño y telemetría de la misión.
 * @param mision Puntero a la misión evaluada.
 */
void simulador_mostrar_reporte(const MisionArtemis *mision);

/**
 * @brief Muestra el manual técnico con las fórmulas físicas y contexto de la misión.
 */
void simulador_mostrar_manual(void);

/**
 * @brief Muestra los datos académicos, cátedra e integrantes del equipo de desarrollo.
 */
void simulador_mostrar_integrantes(void);

#endif // SIMULADOR_H
