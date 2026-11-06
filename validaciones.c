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
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "validaciones.h"

static
int es_entero(const char *texto, int *valor)
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

    if (!es_entero(linea, &valor)) {
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
