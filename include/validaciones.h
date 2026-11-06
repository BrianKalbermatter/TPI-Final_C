/**
 * @file validaciones.h
 * @brief Funciones de validación de entradas de usuario y rangos numéricos.
 * @date 2026-11-04
 */
#ifndef VALIDACIONES_H
#define VALIDACIONES_H

/**
 * @brief Comprueba si una cadena de texto representa un número entero válido.
 * @param texto Cadena a evaluar.
 * @return 1 si es entero válido, 0 en caso contrario.
 */
int es_entero(const char *const texto);

/**
 * @brief Comprueba si una cadena de texto representa un número real (flotante) válido.
 * @param texto Cadena a evaluar.
 * @return 1 si es real válido, 0 en caso contrario.
 */
int es_real(const char *const texto);

/**
 * @brief Evalúa si una cadena representa una respuesta afirmativa (s/S/y/Y/1) o negativa (n/N/0).
 * @param texto Cadena a evaluar.
 * @return 1 si es afirmativo, 0 si es negativo, -1 si no es válido.
 */
int es_confirmacion(const char *const texto);

/**
 * @brief Comprueba si un entero se encuentra dentro de un intervalo cerrado [min, max].
 * @param num Valor entero.
 * @param min Límite inferior permitido.
 * @param max Límite superior permitido.
 * @return 1 si está dentro del rango, 0 en caso contrario.
 */
int validar_rango_entero(int num, int min, int max);

/**
 * @brief Solicita un número entero por consola validando tipo y rango.
 * @param mensaje Texto de solicitud presentado al usuario.
 * @param min Límite inferior permitido.
 * @param max Límite superior permitido.
 * @return El entero válido ingresado por el usuario.
 */
int leer_entero_rango(const char *mensaje, int min, int max);

/**
 * @brief Solicita un número real por consola validando tipo y rango.
 * @param mensaje Texto de solicitud presentado al usuario.
 * @param min Límite inferior permitido.
 * @param max Límite superior permitido.
 * @return El número real válido ingresado por el usuario.
 */
double leer_real_rango(const char *mensaje, double min, double max);

#endif // VALIDACIONES_H
