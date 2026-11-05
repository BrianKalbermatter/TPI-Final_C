/**
 * @file simulador.h
 * @brief 
 * @date 2026-11-04
 * 
 */
#ifndef SIMULADOR_H
#define SIMULADOR_H

// -- Interfaz de ejemplo
typedef struct {
  int dummy;
} SimuladorDatos;

int simulador_iniciar(const SimuladorDatos* sim);
int simulador_liberar(const SimuladorDatos* sim);

#endif//SIMULADOR_H