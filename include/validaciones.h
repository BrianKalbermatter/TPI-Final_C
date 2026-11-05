/**
 * @file validaciones.h
 * @brief 
 * @date 2026-11-04
 *
 */
#ifndef VALIDACIONES_H
#define VALIDACIONES_H

/**
 * @brief 
 * 
 * @param texto 
 * @return int 
 */
int es_entero(const char *const texto);

/**
 * @brief 
 * 
 * @param texto 
 * @return int 
 */
int es_real(const char *const texto);

/**
 * @brief 
 * 
 * @param texto 
 * @return int 
 */
int es_confirmacion(const char *const texto);

/**
 * @brief 
 * 
 * @param num 
 * @param min 
 * @param max 
 * @return int 
 */
int validar_rango_entero(int num, int min, int max);

#endif//VALIDACIONES_H