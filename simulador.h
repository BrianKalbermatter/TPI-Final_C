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

/* ------------------------------------------------------------------ */
/* Constantes fisicas y limites del escenario                          */
/* ------------------------------------------------------------------ */

/* Aceleracion de la gravedad terrestre [m/s^2]. Se la considera
 * constante durante todo el reingreso (simplificacion del enunciado). */
#define GRAVEDAD 9.81

/* Ventana segura del angulo de reingreso [grados] */
#define GAMMA_MIN 5.5
#define GAMMA_MAX 7.0

/* Marca de resultado invalido: se usa NAN (C99, <math.h>) cuando una
 * funcion detecta un caso fisicamente imposible. Verificar con isnan(). */

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

/* ------------------------------------------------------------------ */
/* Pregunta 1: trabajo de la fuerza de rozamiento                      */
/* ------------------------------------------------------------------ */
double calcularTrabajoRozamiento(double masa, double vi, double vf,
                                 double hi, double hf);

/* ------------------------------------------------------------------ */
/* Pregunta 2: desaceleracion media y factor g                         */
/* ------------------------------------------------------------------ */
double calcularDesaceleracionMedia(double vi, double vf, double tiempo);
double calcularFactorG(double aceleracion);

/* ------------------------------------------------------------------ */
/* Pregunta 3: fuerza media de amerizaje y multiplo del peso           */
/* ------------------------------------------------------------------ */
double calcularFuerzaAmerizaje(double masa, double velocidadImpacto,
                               double distanciaFrenado);
double calcularMultiploPeso(double fuerza, double masa);

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

#endif /* SIMULADOR_H */
