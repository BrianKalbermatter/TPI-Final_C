# TPI-Final_C: Simulador de Misiones Artemis


Trabajo Práctico Integrador Final de Algoritmos y Estructuras de Datos (UTN FRRe).
Simulador interactivo por consola, escrito en C, basado en una situación problemática de la misión Artemis.

## Integrantes y comisión

**Comisión:** _a completar_

| Integrante | Usuario de GitHub |
|---|---|
| Brian Kalbermatter | [BrianKalbermatter](https://github.com/BrianKalbermatter) |
| Emiliano Elst | [Emiliano Elst](https://github.com/emilianoelst) |
| Enzo Solis | [Enzo Solis](https://github.com/EnzoSolis) |
| Jesus Miranda Arce | [Jesus Miranda Arce](https://github.com/EzXdlol)|

## Cómo trabajamos

El trabajo se organiza en el tablero de GitHub Projects. Cada ticket cubre un área completa del proyecto.

1. Elegí un ticket libre de la columna **Todo**.
2. Antes de escribir código, asignátelo y movelo a **In Progress**. Un ticket con responsable ya está tomado.
3. Creá tu rama desde `main` actualizado.
4. Trabajá con commits chicos y frecuentes.
5. Abrí un Pull Request hacia `main`. GitHub le pide la revisión a Brian automáticamente.
6. Cuando Brian lo aprueba y se integra, el ticket pasa a **Done**.

### Ramas

`main` está protegida: los cambios entran solo por Pull Request aprobado por Brian. Si subís commits después de la aprobación, hay que aprobar de nuevo. El formato de las ramas es `problema/solucion/nombre-apellido/ticket`, con palabras clave cortas, en minúsculas y sin acentos ni espacios:

```text
sin-validacion/validar-enteros/brian-kalbermatter/validaciones
```

### Commits

Cada mensaje dice qué cambió y referencia el ticket:

```text
Problema:
Solucion:
Nombre y Apellido del integrante:
Nombre del ticket:
```

Todos los integrantes tienen que commitear durante todo el período de trabajo. La cátedra evalúa que los commits sean equitativos y no acepta un único commit final.

## Requisitos obligatorios para subir código

Un Pull Request se integra a `main` solo si cumple **todos** estos puntos.

### Proceso

- [ ] La rama respeta el formato `problema/solucion/nombre-apellido/ticket`.
- [ ] El Pull Request referencia su ticket, por ejemplo `Closes #3`.
- [ ] Brian lo revisó y lo aprobó. Los Pull Requests de Brian los revisa otro integrante.
- [ ] El uso de IA de ese cambio está en el [registro de IA](#registro-de-uso-de-inteligencia-artificial).
- [ ] Quien sube el código lo entiende y puede defender cada línea.

## Estructura del proyecto

| Archivo | Responsabilidad |
|---|---|
| `main.c` | Función `main()`, menú principal e inicio del flujo del programa |
| `simulador.h` | Constantes, `struct`, `typedef` y prototipos públicos |
| `simulador.c` | Algoritmos de simulación, estado del juego, cálculos físicos y lógica de negocio |
| `validaciones.c` / `validaciones.h` | Ingreso seguro de enteros, flotantes y caracteres, y mensajes de advertencia |
| `Makefile` | Compilación del proyecto |

## Escenario Artemis

_A completar cuando la cátedra asigne el escenario: descripción detallada y reglas del juego._

## Funcionamiento

_A completar: menú principal, ciclo de simulación por turnos o eventos, y reporte final de desempeño._

## Compilación y ejecución

_A completar cuando esté el `Makefile`._

## Registro de uso de Inteligencia Artificial

Registro obligatorio. Cada integrante agrega una fila por cada uso relevante.

| Integrante | Herramienta | Tarea | Prompt o consulta | Cómo se validó o adaptó |
|---|---|---|---|---|
| Brian Kalbermatter | Claude Code | Crear el repositorio, las reglas de trabajo y los tickets | Pedido de armar el tablero de tickets por áreas a partir del enunciado | Revisión de los requisitos contra el enunciado |
| Brian Kalbermatter | Claude Code | Validar el ingreso de enteros en `validaciones.c` y `validaciones.h` | Consulta sobre cómo verificar si una entrada es un entero y cómo armar `leer_entero` | Se probó con letras, línea vacía, `12abc`, `3.5`, valores fuera de rango, 200 dígitos y fin de entrada; se revisó y se documentó el recorrido de la función |

## Entregables

- [ ] Repositorio público con el código modularizado, el `Makefile` y el historial de cambios.
- [ ] README completo: integrantes y comisión, escenario y reglas, funcionamiento, compilación y ejecución, y registro de IA.
- [ ] Video de hasta 5 minutos con todos los integrantes, usando su voz real y apareciendo en cámara. Incluye presentación, arquitectura, un caso exitoso y las validaciones ante entradas erróneas. No se permite usar IA para generar voces, avatares ni personas.
