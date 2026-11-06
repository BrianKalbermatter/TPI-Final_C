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

#include <stdio.h>
#include <math.h>
#include "simulador.h"

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
double calcularTrabajoRozamiento(double masa, double vi, double vf,
                                 double hi, double hf)
{
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
    trabajoGravedad = masa * GRAVEDAD * (hi - hf);
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
double calcularDesaceleracionMedia(double vi, double vf, double tiempo)
{
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
double calcularFactorG(double aceleracion)
{
    if (isnan(aceleracion)) {
        fprintf(stderr, "Error: aceleracion invalida, no se calcula el factor g.\n");
        return NAN;
    }
    return fabs(aceleracion) / GRAVEDAD;
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
double calcularFuerzaAmerizaje(double masa, double velocidadImpacto,
                               double distanciaFrenado)
{
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
double calcularMultiploPeso(double fuerza, double masa)
{
    if (isnan(fuerza)) {
        fprintf(stderr, "Error: fuerza invalida, no se calcula el multiplo del peso.\n");
        return NAN;
    }
    if (!(masa > 0.0)) {
        fprintf(stderr, "Error: la masa debe ser mayor que 0.\n");
        return NAN;
    }
    return fuerza / (masa * GRAVEDAD);
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
        fprintf(stderr, "Error: el angulo de reingreso debe estar en (0, 90] grados.\n");
        return;
    }

    printf("Angulo de reingreso: %.2f grados\n", gamma);

    if (gamma > GAMMA_MAX) {
        printf("  Angulo demasiado pronunciado:\n");
        printf("   - desaceleracion excesiva\n");
        printf("   - calentamiento extremo\n");
        printf("   - riesgo estructural\n");
        printf("   - peligro para la tripulacion\n");
    } else if (gamma < GAMMA_MIN) {
        printf("  Angulo demasiado superficial:\n");
        printf("   - rebote atmosferico\n");
        printf("   - perdida de frenado\n");
        printf("   - posible retorno al espacio\n");
        printf("   - desviacion de la zona de amerizaje\n");
    } else {
        printf("  Trayectoria segura\n");
    }
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
        calcularTrabajoRozamiento(masa, vi, vf, hi, hf);

    /* Pregunta 2 */
    resultado->desaceleracion =
        calcularDesaceleracionMedia(vi, vf, tiempo);
    resultado->factorG = calcularFactorG(resultado->desaceleracion);

    /* Pregunta 3 */
    resultado->fuerzaAmerizaje =
        calcularFuerzaAmerizaje(masa, velocidadImpacto, distanciaFrenado);
    resultado->multiploPeso =
        calcularMultiploPeso(resultado->fuerzaAmerizaje, masa);
}
