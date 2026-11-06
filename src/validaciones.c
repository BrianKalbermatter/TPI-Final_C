#include "validaciones.h"
#include "utilidades.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>

#define BUFFER_TAM 128

// Ingreso seguro de enteros, flotantes y caracteres, y mensajes de advertencia
// Pedir un entero y no dejar seguir hasta que escribas uno valido y dentro del rango.
// El recorrido en cada vuelta es:
/*
1. Muestra la pregunta y lee la linea.
2. Si no hay mas entrada, cierra el programa. Es el unico caso que no vuelve a preguntar.
3. Si la linea es demasiado larga, tira el sobrante, avisa y pregunta de nuevo
4. Si no es un entero, avisa y pregunta de nuevo.
5. Si esta fuera del minimo y maximo, avisa y pregunta de nuevo
6. Si es correcto, devuelve el numero y termina
*/

static
int es_entero_valor(const char *texto, int *valor)
{
  char *fin;
  long num;

  errno = 0;
  num = strtol(texto, &fin, 10);

  if (fin == texto) // Esto dice que no habia ningun digito
    return 0;
  while (isspace((unsigned char)*fin)) fin++; // saltea el \n que deja fgets
  if (*fin != '\0') return 0; // sobro texto: "12a"
  if (errno == ERANGE || num < INT_MIN || num > INT_MAX) return 0;
  *valor = (int)num;
  return 1;  
}

static
void descartar_linea(void)
{
  int c;
  while ((c = getchar()) != '\n' && c != EOF){
    
  }
}

int leer_entero(const char *mensaje, int min, int max)
{
  char linea[64];
  int valor;

  while (1) {
    printf("%s", mensaje);
    fflush(stdout); // No espera que se llene la memoria. Se dispara

    if(fgets(linea, sizeof linea, stdin) == NULL){
      printf("\nFin de la entrada. Saliendo del programa.\n");
      exit(EXIT_FAILURE);
    }

    if(strchr(linea, '\n') == NULL && !feof(stdin)) {
      descartar_linea();
      printf("Error: la entrada es demasiado larga.\n");
      continue;
    }

    if (!es_entero_valor(linea, &valor)) {
      printf("Error: ingrese un numero entero.\n");
      continue;
    }

    if (valor < min || valor > max) {
      printf("Error: el valor debe estar entre %d y %d.\n", min, max);
      continue;
    }


    return valor;
  }
}

static void recortar_espacios(char *dest, const char *orig, size_t max_tam) {
  ASSERT(dest != NULL, "dest no puede ser nulo en recortar_espacios");
  ASSERT(orig != NULL, "orig no puede ser nulo en recortar_espacios");
  ASSERT(max_tam > 0, "max_tam debe ser mayor a 0 en recortar_espacios");

  if (orig == NULL || dest == NULL || max_tam == 0) return;

  // Saltar espacios iniciales
  while (*orig && isspace((unsigned char)*orig)) {
    orig++;
  }

  size_t len = 0;
  while (*orig && len + 1 < max_tam) {
    dest[len++] = *orig++;
  }
  dest[len] = '\0';

  // Quitar espacios finales y saltos de linea
  while (len > 0 && isspace((unsigned char)dest[len - 1])) {
    dest[--len] = '\0';
  }
}

int es_entero(const char *const texto) {
  ASSERT(texto != NULL, "texto no puede ser nulo en es_entero");
  if (texto == NULL) return 0;

  LOG_TRACE("Evaluando es_entero para texto: \"%s\"", texto);

  char limpio[BUFFER_TAM];
  recortar_espacios(limpio, texto, sizeof(limpio));

  if (limpio[0] == '\0') return 0;

  int i = 0;
  if (limpio[0] == '+' || limpio[0] == '-') {
    i++;
    if (limpio[i] == '\0') return 0; // Solo habia un signo
  }

  for (; limpio[i] != '\0'; i++) {
    if (!isdigit((unsigned char)limpio[i])) {
      return 0;
    }
  }

  // Un numero que no entra en int tampoco es un entero valido.
  errno = 0;
  long numero = strtol(limpio, NULL, 10);
  if (errno == ERANGE || numero < INT_MIN || numero > INT_MAX) return 0;

  return 1;
}

int es_real(const char *const texto) {
  ASSERT(texto != NULL, "texto no puede ser nulo en es_real");
  if (texto == NULL) return 0;

  LOG_TRACE("Evaluando es_real para texto: \"%s\"", texto);

  char limpio[BUFFER_TAM];
  recortar_espacios(limpio, texto, sizeof(limpio));

  if (limpio[0] == '\0') return 0;

  int i = 0;
  if (limpio[0] == '+' || limpio[0] == '-') {
    i++;
    if (limpio[i] == '\0') return 0;
  }

  int puntos = 0;
  int digitos = 0;

  for (; limpio[i] != '\0'; i++) {
    if (limpio[i] == '.' || limpio[i] == ',') {
      puntos++;
      if (puntos > 1) return 0;
    } else if (isdigit((unsigned char)limpio[i])) {
      digitos++;
    } else {
      return 0;
    }
  }

  return (digitos > 0);
}

int es_confirmacion(const char *const texto) {
  ASSERT(texto != NULL, "texto no puede ser nulo en es_confirmacion");
  if (texto == NULL) return -1;

  LOG_TRACE("Evaluando es_confirmacion para texto: \"%s\"", texto);

  char limpio[BUFFER_TAM];
  recortar_espacios(limpio, texto, sizeof(limpio));

  if (limpio[0] == '\0' || limpio[1] != '\0') return -1;

  char c = (char)tolower((unsigned char)limpio[0]);
  if (c == 's' || c == 'y' || c == '1') return 1;
  if (c == 'n' || c == '0') return 0;

  return -1;
}

int validar_rango_entero(int num, int min, int max) {
  ASSERT(min <= max, "min no puede ser mayor que max en validar_rango_entero");
  LOG_TRACE("validar_rango_entero: num=%d en [%d, %d]", num, min, max);
  return (num >= min && num <= max);
}

int leer_entero_rango(const char *mensaje, int min, int max) {
  ASSERT(min <= max, "min no puede ser mayor que max en leer_entero_rango");
  LOG_DEBUG("[CONSOLA] Esperando parámetro entero en rango [%d, %d]", min, max);

  char buffer[BUFFER_TAM];
  int valor = 0;
  int valido = 0;

  do {
    if (mensaje != NULL) {
      printf("%s", mensaje);
      fflush(stdout);
    }

    if (fgets(buffer, sizeof(buffer), stdin) == NULL) {
      LOG_WARNING("[CONSOLA] Señal de fin de entrada (EOF) o fallo en buffer estándar");
      printf("\nFin de la entrada. Saliendo del programa.\n");
      exit(EXIT_FAILURE);
    }

    if (strchr(buffer, '\n') == NULL && !feof(stdin)) {
      descartar_linea();
      printf("[!] Error: La entrada es demasiado larga. Intente nuevamente.\n");
      continue;
    }

    if (es_entero(buffer)) {
      valor = (int)strtol(buffer, NULL, 10);
      if (validar_rango_entero(valor, min, max)) {
        valido = 1;
        LOG_DEBUG("[CONSOLA] Parámetro verificado por telemetría: %d", valor);
      } else {
        LOG_WARNING("[CONSOLA] Parámetro fuera de especificación de vuelo: %d ∉ [%d, %d]", valor, min, max);
        printf("[!] Error: El valor debe encontrarse entre %d y %d. Intente nuevamente.\n", min, max);
      }
    } else {
      LOG_WARNING("[CONSOLA] Comando no reconocido o sintaxis inválida: \"%s\"", buffer);
      printf("[!] Error: Entrada invalida. Por favor ingrese un numero entero.\n");
    }
  } while (!valido);

  return valor;
}

double leer_real_rango(const char *mensaje, double min, double max) {
  ASSERT(min <= max, "min no puede ser mayor que max en leer_real_rango");
  LOG_DEBUG("[CONSOLA] Esperando parámetro decimal en rango [%.2f, %.2f]", min, max);

  char buffer[BUFFER_TAM];
  double valor = 0.0;
  int valido = 0;

  do {
    if (mensaje != NULL) {
      printf("%s", mensaje);
      fflush(stdout);
    }

    if (fgets(buffer, sizeof(buffer), stdin) == NULL) {
      LOG_WARNING("[CONSOLA] Señal de fin de entrada (EOF) o fallo en buffer estándar");
      printf("\nFin de la entrada. Saliendo del programa.\n");
      exit(EXIT_FAILURE);
    }

    if (strchr(buffer, '\n') == NULL && !feof(stdin)) {
      descartar_linea();
      printf("[!] Error: La entrada es demasiado larga. Intente nuevamente.\n");
      continue;
    }

    // Reemplazar coma por punto para strtod si correspondiera
    for (int i = 0; buffer[i] != '\0'; i++) {
      if (buffer[i] == ',') buffer[i] = '.';
    }

    if (es_real(buffer)) {
      valor = strtod(buffer, NULL);
      if (valor >= min && valor <= max) {
        valido = 1;
        LOG_DEBUG("[CONSOLA] Parámetro verificado por telemetría: %.2f", valor);
      } else {
        LOG_WARNING("[CONSOLA] Parámetro fuera de especificación de vuelo: %.2f ∉ [%.2f, %.2f]", valor, min, max);
        printf("[!] Error: El valor debe encontrarse entre %.2f y %.2f. Intente nuevamente.\n", min, max);
      }
    } else {
      LOG_WARNING("[CONSOLA] Comando no reconocido o sintaxis inválida: \"%s\"", buffer);
      printf("[!] Error: Entrada invalida. Por favor ingrese un numero decimal valido.\n");
    }
  } while (!valido);

  return valor;
}

int leer_confirmacion(const char *mensaje) {
  ASSERT(mensaje != NULL, "mensaje no puede ser nulo en leer_confirmacion");

  char buffer[BUFFER_TAM];
  int respuesta = -1;

  do {
    printf("%s", mensaje);
    fflush(stdout);

    if (fgets(buffer, sizeof(buffer), stdin) == NULL) {
      LOG_WARNING("[CONSOLA] Señal de fin de entrada (EOF) o fallo en buffer estándar");
      printf("\nFin de la entrada. Saliendo del programa.\n");
      exit(EXIT_FAILURE);
    }

    if (strchr(buffer, '\n') == NULL && !feof(stdin)) {
      descartar_linea();
      printf("[!] Error: La entrada es demasiado larga. Intente nuevamente.\n");
      continue;
    }

    respuesta = es_confirmacion(buffer);
    if (respuesta == -1) {
      printf("[!] Error: Entrada invalida. Por favor responda S o N.\n");
    }
  } while (respuesta == -1);

  return respuesta;
}
