/**
 * @file visualizacion.h
 * @brief Centro de control en pantalla completa: vista grafica del reingreso + terminal MCC.
 *
 * La pantalla se divide en dos paneles:
 *  - Izquierda: vista telemetrica animada (cielo, oceano, estela, capsula, plasma y HUD).
 *  - Derecha: terminal del Centro de Control donde el operador ingresa los comandos.
 *
 * Cada turno se juega en vivo: el operador ingresa el dato en la terminal y la
 * vista anima el tramo recorrido usando el historial de telemetria de la mision.
 */
#ifndef VISUALIZACION_H
#define VISUALIZACION_H

#include "simulador.h"

/**
 * @brief Abre el centro de control en pantalla completa.
 * @param mision Mision recien inicializada (se lee durante toda la sesion).
 * @return 1 si la vista grafica quedo activa; 0 si la terminal no la soporta
 *         (en ese caso se debe seguir en modo texto).
 */
int visualizacion_iniciar(const MisionArtemis *mision);

/**
 * @brief Pide un numero real en la terminal del centro de control hasta que sea valido.
 * @param pregunta Texto de la pregunta.
 * @param minimo Valor minimo aceptado.
 * @param maximo Valor maximo aceptado.
 * @return Valor ingresado, dentro de [minimo, maximo].
 */
double visualizacion_leer_real(const char *pregunta, double minimo, double maximo);

/**
 * @brief Pide una confirmacion S/N en la terminal del centro de control.
 * @return 1 si la respuesta es afirmativa, 0 si es negativa.
 */
int visualizacion_leer_confirmacion(const char *pregunta);

/** @brief Anima el ultimo turno registrado en el historial y lo informa en la terminal. */
void visualizacion_animar_turno(void);

/** @brief Muestra el desenlace de la mision, espera ENTER y restaura la consola. */
void visualizacion_finalizar(void);

#endif // VISUALIZACION_H
