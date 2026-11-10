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
| Ciro Alejandro Villalba | [Ciro Alejandro Villalba](https://github.com/Gravitated-ribbit-ribbit) |

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
| `render.c` / `render.h` | Mini motor de render ASCII en pantalla completa: lienzo de celdas, rampa de brillo, color de 24 bits y cuadros sin parpadeo |
| `visualizacion.c` / `visualizacion.h` | Centro de control: vista animada del reingreso (cielo, océano, estela, plasma, paracaídas y HUD) que se juega en vivo turno a turno |
| `terminal_mcc.c` / `terminal_mcc.h` | Consola del Centro de Control de Misión dentro de la pantalla: historial de mensajes y línea de comandos del operador |
| `Makefile` | Compilación del proyecto |

## Escenario Artemis

### Situación Problemática 4: Reingreso Atmosférico, Frenado Aerodinámico y Amerizaje (Trabajo, Energía e Impacto)

Tras completar el sobrevuelo lunar, el módulo de tripulación Orion ($m = 10.400,0\text{ kg}$) se separa del módulo de servicio y reingresa a la atmósfera terrestre a una velocidad hiperveloz de $v_i = 11,0\text{ km/s}$ ($11.000,0\text{ m/s}$ ~ Mach 32) a una altitud de $h_i = 120,0\text{ km}$ ($120.000,0\text{ m}$). 

El escudo térmico de ablación disipa la inmensa energía cinética de la nave mediante el trabajo de fricción aerodinámica del aire ($W_{\text{roz}}$), reduciendo su rapidez hasta $v_f = 25,0\text{ m/s}$ al momento de desplegar los paracaídas principales a una altitud de $h_f = 2,50\text{ km}$ ($2.500,0\text{ m}$). Finalmente, la nave ameriza en la superficie del Océano Pacífico a una rapidez segura de $v_{\text{impacto}} = 8,0\text{ m/s}$, penetrando $d = 1,50\text{ m}$ en el agua hasta detenerse momentáneamente antes de flotar.

### Reglas de la Simulación y Condiciones de Misión

* **Rol del usuario:** Director de Vuelo y Guiado en Houston / Piloto de la cápsula Orion.
* **Objetivo:** Calibrar las maniobras de descenso para disipar más de $600\text{ GJ}$ de energía mecánica, manteniendo las fuerzas G y la temperatura del escudo térmico dentro de los límites de supervivencia humana y estructural.
* **Ventana de reingreso seguro ($\gamma$):**
  * $5,5^\circ \le \gamma \le 7,0^\circ$: Reingreso nominal controlado.
  * Si $\gamma < 5,5^\circ$ (**Rebote atmosférico / *Skip-off*):** La atmósfera es insuficiente para frenar la cápsula; la nave rebota hacia el espacio exterior y se pierde en órbita heliocéntrica sin retorno.
  * Si $\gamma > 7,0^\circ$ (**Reingreso escarpado):** La desaceleración supera los $15\text{--}20\text{ g}$ y la temperatura extrema destruye el escudo térmico, causando la pérdida de la nave y su tripulación.
* **Despliegue de paracaídas:** Debe efectuarse en la ventana segura de altitud ($\approx 2.500\text{ m}$) y velocidad subsónica. Si se abren antes, se desgarran por presión dinámica; si se abren tarde, la nave colisiona a velocidad mortal contra el océano.
* **Condición de victoria:** Amerizaje exitoso a $v \le 8,0\text{ m/s}$, integridad estructural conservada y tripulación con vida.

## Funcionamiento

El simulador se ejecuta nativamente en la terminal de comandos y se organiza en un ciclo interactivo estructurado:

```text
[ MENÚ PRINCIPAL ]
  ├── 1. Iniciar Simulación Interactiva (Modo Juego / Piloto)
  ├── 2. Análisis Teórico y Preguntas del TP (Caso Oficial Artemis)
  ├── 3. Ver Reporte y Telemetría de la Última Misión
  ├── 4. Manual de Vuelo y Fórmulas Físicas
  ├── 5. Integrantes del Equipo de Desarrollo
  └── 6. Salir del Programa
```

### Dinámica de Ejecución y Módulos

1. **Modo Juego Interactivo (Ciclo por Fases y Eventos):**
   * **Fase 1 (Inyección Atmosférica - 120 a 60 km):** Calibración del ángulo de reingreso $\gamma$ mediante propulsores de control de reacción (RCS).
   * **Fase 2 (Frenado Hipersónico - 60 km a 2.500 m):** Control de ángulo de alabeo (*roll angle*) para disipar energía y gestionar las cargas G sobre la tripulación, atravesando el periodo de *blackout* de comunicaciones por plasma ionizado.
   * **Fase 3 (Secuencia de Paracaídas y Amerizaje - 2.500 m a 0 m):** Activación secuencial de paracaídas de frenado (*Drogue chutes*) y principales (*Main chutes*), y absorción de impacto hidrodinámico en el agua.

2. **Resolución Analítica Teórica:**
   * Resuelve paso a paso y con rigor físico las 4 preguntas analíticas de la cátedra (Trabajo de rozamiento $W_{\text{roz}} = \Delta E_m$, desaceleración media y cargas G en $\Delta t = 400\text{ s}$, fuerza hidrodinámica promedio $F_{\text{prom}}$ y evaluación física del ángulo $\gamma$), permitiendo ingresar parámetros personalizados o usar los valores oficiales de la misión.

3. **Reporte Final y Telemetría de Vuelo:**
   * Cada segundo y evento relevante del vuelo se almacena en memoria dentro de un arreglo de estructuras `PuntoTelemetria historial[MAX_HISTORIAL_TELEMETRIA]` (cumpliendo con el requisito obligatorio de punteros y arreglos).
   * Al finalizar la misión se expone una tabla completa de telemetría (tiempo, altitud, velocidad, carga G, energía disipada) y el dictamen oficial de la misión.

4. **Interfaz y Experiencia de Usuario:**
   * Soporte de codificación UTF-8 para símbolos y caracteres en español.
   * Navegación limpia de consola con borrado de pantalla (`limpiar_pantalla()`) y retorno intuitivo con confirmación (`volver_atras()`).
   * Validación defensiva de entradas (`validaciones.c`) para evitar cierres inesperados o bucles infinitos ante ingresos alfanuméricos en campos numéricos.
   * Trazabilidad completa con registros de telemetría tipo *Mission Control* de Houston.

## Compilación y ejecución

El proyecto cuenta con soporte de compilación tanto mediante **Make** como mediante **CMake**. Los binarios ejecutables se generan en la carpeta `out/<BUILD_TYPE>/bin/`.

### Opción 1: Compilación con Make (Recomendado)

En **Windows** (usando MinGW / GCC):
```bash
# Compilar en modo Debug (por defecto)
mingw32-make

# Ejecutar el simulador
.\out\Debug\bin\simulador.exe

# Limpiar archivos objeto y binarios generados
mingw32-make clean
```

En **Linux / macOS**:
```bash
# Compilar en modo Debug
make

# Ejecutar el simulador
./out/Debug/bin/simulador

# Limpiar archivos objeto y binarios generados
make clean
```

> **Nota:** Para compilar en modo optimizado de producción: `mingw32-make BUILD_TYPE=Release` o `make BUILD_TYPE=Release`.

---

### Opción 2: Compilación con CMake

Funciona en Windows y Linux:

```bash
# 1. Configurar y generar los archivos de construcción
cmake -B build -DCMAKE_BUILD_TYPE=Debug

# 2. Compilar el proyecto
cmake --build build

# 3. Ejecutar el simulador
# En Windows:
.\out\Debug\bin\simulador.exe

# En Linux / macOS:
./out/Debug/bin/simulador
```

## Registro de uso de Inteligencia Artificial

Registro obligatorio. Cada integrante agrega una fila por cada uso relevante.

| Integrante | Herramienta | Tarea | Prompt o consulta | Cómo se validó o adaptó |
|---|---|---|---|---|
| Brian Kalbermatter | Claude Code | Crear el repositorio, las reglas de trabajo y los tickets | Pedido de armar el tablero de tickets por áreas a partir del enunciado | Revisión de los requisitos contra el enunciado |
| Enzo Solis | Gemini Flash (Antigravity) | Diseño de estructuras Situación 4, menú interactivo, validaciones y telemetría | Modelar structs, firmas físicas, menú con limpiar/volver y logs con voz de misión | Adaptación a C11, paso por referencia y compilación estricta con GCC (-Wall -Wextra) |
| Brian Kalbermatter | Claude Code | Validar el ingreso de enteros en `validaciones.c` y `validaciones.h` | Consulta sobre cómo verificar si una entrada es un entero y cómo armar `leer_entero` | Se probó con letras, línea vacía, `12abc`, `3.5`, valores fuera de rango, 200 dígitos y fin de entrada; se revisó y se documentó el recorrido de la función |
| Brian Kalbermatter | Claude Code | Unir las ramas del equipo en una rama de integración, completar el ciclo de juego, el modo teórico y el reporte | Pedido de unificar todas las ramas según la consigna sin quitar lo escrito por el equipo | Se jugaron todos los finales, se probaron entradas erróneas y fin de entrada, y se compiló con `-Wall -Wextra` en Debug y Release |
| Emiliano Elst | Gemini | Ciclo de simulación por turnos, historial en arreglos y reporte final (Ticket #5) | Solicitud de diseño de estado de la misión, bucle de turnos con condiciones de éxito/fracaso del Escenario 4 y reporte de desempeño | Verificación contra las fórmulas físicas del reingreso atmosférico y adaptación al código integrado del equipo |
| Ciro Alejandro Villalba | Claude Code | Controlar y editar el video | Pedido de corregir la discontinuidad brusca en algunas partes del video | Revisión del video |

## Entregables

- [ ] Repositorio público con el código modularizado, el `Makefile` y el historial de cambios.
- [ ] README completo: integrantes y comisión, escenario y reglas, funcionamiento, compilación y ejecución, y registro de IA.
- [ ] Video de hasta 5 minutos con todos los integrantes, usando su voz real y apareciendo en cámara. Incluye presentación, arquitectura, un caso exitoso y las validaciones ante entradas erróneas. No se permite usar IA para generar voces, avatares ni personas.
