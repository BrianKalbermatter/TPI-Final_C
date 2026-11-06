#ifndef SIMULADOR_H
#define SIMULADOR_H

// Constantes del Escenario 4: Reingreso Atmosferico y Amerizaje
#define MAX_TURNOS 10
#define MASA_ORION 10400.0f       // kg
#define GRAVEDAD 9.80f            // m/s^2
#define ALTITUD_INICIAL 120000.0f // 120 km en metros
#define VEL_INICIAL 11000.0f      // 11 km/s en m/s
#define ALT_PARACAIDAS 2500.0f    // 2.5 km en metros
#define VEL_IMPACTO_SEGURA 8.0f   // m/s

// Estados posibles de la mision
typedef enum {
    EN_CURSO = 0,
    EXITO = 1,
    FRACASO_REBOTE = -1,
    FRACASO_DESINTEGRACION = -2,
    FRACASO_IMPACTO = -3
} EstadoMision;

// Estructura para cada posicion del arreglo de historial de turnos
typedef struct {
    int numero_turno;
    float altitud_m;
    float velocidad_ms;
    float angulo_gamma;
    float fuerzas_g;
    float trabajo_rozamiento_j;
    char evento[80];
} RegistroTurno;

// Estructura principal del estado de la mision
typedef struct {
    float altitud_actual;
    float velocidad_actual;
    float integridad_escudo;
    float max_g_soportada;
    float trabajo_total_rozamiento;
    int paracaidas_desplegados;
    int cantidad_turnos;
    EstadoMision estado;
    RegistroTurno historial[MAX_TURNOS];
} MisionArtemis;

// Prototipos publicos del simulador (Ticket #5)
void inicializar_mision(MisionArtemis *mision);
void iniciar_ciclo_simulacion(MisionArtemis *mision);
void actualizar_estado_turno(MisionArtemis *mision, float angulo_elegido, int accion_paracaidas, float dt);
void guardar_turno_historial(MisionArtemis *mision, float angulo, float g_actual, float w_roz, const char *desc_evento);
void mostrar_reporte_final(const MisionArtemis *mision);

#endif
