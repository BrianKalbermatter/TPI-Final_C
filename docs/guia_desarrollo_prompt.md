# Guía de Continuación y Prompt para el Equipo de Desarrollo

Este documento contiene las directivas arquitectónicas, diseño de interfaz, convenciones de logging/aserciones y el **prompt estructurado** para que los integrantes del equipo puedan continuar el desarrollo del simulador utilizando herramientas de Inteligencia Artificial o de forma manual, manteniendo la coherencia técnica del proyecto.

---

```markdown
### CONTEXTO DEL PROYECTO
Estamos desarrollando el Trabajo Práctico Integrador Final de la cátedra Algoritmos y Estructuras de Datos (UTN FRRe) en lenguaje C (estándar C11).
El proyecto simula la **Situación 4 de la misión Artemis: Reingreso Atmosférico, Frenado Aerodinámico y Amerizaje de la cápsula Orion**.
Repositorio: modularizado con compilación cruzada mediante `Makefile` (`gcc -Wall -Wextra -std=c11 -lm`).

---

### ARQUITECTURA BASE EXISTENTE (NO REINVENTAR LA RUEDA)

Ya se encuentra construida y testeada la infraestructura base del proyecto:
1. `include/simulador.h` y `src/simulador.c`:
   - Constantes físicas del dominio: `MASA_ORION_KG` (10400.0 kg), `ALTITUD_REINGRESO_M` (120 km), `VELOCIDAD_REINGRESO_MS` (11.000 m/s), `ALTITUD_PARACAIDAS_M` (2.500 m), `VELOCIDAD_PARACAIDAS_MS` (25 m/s), `VELOCIDAD_AMERIZAJE_MS` (8 m/s), `DISTANCIA_HUNDIMIENTO_M` (1.5 m), `DELTA_T_DESACELERACION_S` (400 s), ventana de reingreso [5.5°, 7.0°].
   - Estructuras principales:
     * `PuntoTelemetria`: registro de instante, altitud, velocidad, aceleración G, energía mecánica y fase.
     * `MisionArtemis`: almacena el estado completo de la cápsula, integridad del escudo térmico, métricas calculadas, resultado de misión y el historial como arreglo estático: `PuntoTelemetria historial[MAX_HISTORIAL_TELEMETRIA]` (cumple el requisito obligatorio de ARREGLOS).
   - Las firmas de funciones físicas ya están declaradas en `.h` y tienen stubs con validaciones en `.c`.

2. `include/utilidades.h` y `src/utilidades.c`:
   - Manejo de consola multiplataforma: `limpiar_pantalla()` y `volver_atras()` (pausa interactiva que espera ENTER y limpia la pantalla).
   - `activar_utf8()` para caracteres especiales y acentos en Windows/Linux.
   - Macros `ASSERT(condicion, mensaje)` para verificación estricta de invariantes y precondiciones.
   - Sistema de logs por niveles: `LOG_TRACE`, `LOG_DEBUG`, `LOG_INFO`, `LOG_WARNING`, `LOG_ERROR`.

3. `include/validaciones.h` y `src/validaciones.c`:
   - Lectura segura de datos desde `stdin` con `fgets` (NUNCA usar `scanf` directo para evitar bucles infinitos ante letras):
     * `int leer_entero_rango(const char *mensaje, int min, int max);`
     * `double leer_real_rango(const char *mensaje, double min, double max);`
     * `int es_entero(...)`, `int es_real(...)`, `int es_confirmacion(...)`.

4. `src/main.c`:
   - Menú interactivo estructurado con opciones:
     1. Modo Juego Interactivo (Piloto / Reingreso)
     2. Análisis Teórico y Preguntas del TP (Caso Oficial Artemis)
     3. Ver Reporte y Telemetría de la Última Misión
     4. Manual de Vuelo y Fórmulas Físicas
     5. Integrantes del Equipo de Desarrollo
     6. Salir

---

### DIRECTIVAS Y CONVENCIONES DE ESTILO OBLIGATORIAS

1. **Uso de Punteros y Modularización (Rúbrica de Evaluación):**
   - El estado de la misión se pasa SIEMPRE por referencia: `MisionArtemis *mision`.
   - Prohibido crear variables globales de estado.
   - Mantener las funciones pequeñas, cohesivas y sin código duplicado.

2. **Aserciones (`ASSERT`):**
   - Validar precondiciones matemáticas y físicas al inicio de cada función (ej. `ASSERT(mision != NULL, ...);`, `ASSERT(masa > 0.0, ...);`, `ASSERT(delta_t > 0.0, ...);`).

3. **Voz de Misión y Telemetría en el Logging:**
   - Todo log debe redactarse como transmisiones del Centro de Control de Houston o de la computadora de abordo de Orion:
     * `LOG_INFO("[HOUSTON] ...")` o `LOG_INFO("[ORION-OBC] ...")` para eventos nominales y cambios de fase.
     * `LOG_WARNING("[BIOMÉTRICA] ...")` o `LOG_WARNING("[CONSOLA] ...")` para advertencias de sobrecarga G o parámetros fuera de especificación.
     * `LOG_ERROR("[TRAYECTORIA] ...")` para fallos fatales (rebote atmosférico o escudo destruido).
     * `LOG_DEBUG("[FIDO] ...")` o `LOG_TRACE("[AERODINÁMICA] ...")` para traza interna de cálculos numéricos.

4. **Navegación e Interfaz:**
   - Cada pantalla que muestres debe comenzar con `limpiar_pantalla()` y terminar con `volver_atras()` antes de retornar al menú principal.

5. **Compilación Limpia:**
   - El código debe compilar con `mingw32-make` (o `make`) con banderas `-Wall -Wextra -std=c11` arrojando CERO advertencias y CERO errores.

---

### TAREAS PENDIENTES A IMPLEMENTAR (ELEGIR SEGÚN EL TICKET ASIGNADO)

Por favor, ayúdame a implementar la siguiente tarea respetando rigurosamente toda la arquitectura anterior:

- [ ] **Tarea 1: Implementar las Fórmulas Físicas en `src/simulador.c`**:
  - `calcular_trabajo_rozamiento(...)`: W_roz = Delta E_m = (K_f - K_i) + (U_f - U_i) con E_m = 0.5*m*v^2 + m*g*h.
  - `calcular_desaceleracion_media(...)`: a_med = |v_f - v_i| / delta_t.
  - `calcular_fuerza_g(...)`: Carga_G = a_med / g (con g = 9.80 m/s^2).
  - `calcular_fuerza_impacto(...)`: F_prom = (m * v_impacto^2) / (2 * distancia_frenado).
  - `calcular_multiplo_peso(...)`: Relacion = F / (m * g).
  - `evaluar_angulo_reingreso(...)`: Determinar si gamma < 5.5° (rebote), entre 5.5° y 7.0° (óptimo) o > 7.0° (destrucción).

- [ ] **Tarea 2: Implementar la Resolución Analítica Teórica (`simulador_ejecutar_modo_teorico`)**:
  - Presentar la resolución paso a paso de las 4 preguntas del TP (a, b, c, d) permitiendo al usuario ingresar valores personalizados (validados con `leer_real_rango`) o usar los valores nominales de la misión Artemis.

- [ ] **Tarea 3: Implementar el Ciclo del Modo Juego Interactivo (`simulador_ejecutar_modo_interactivo`) y Reporte (`simulador_mostrar_reporte`)**:
  - Ciclo por 3 fases críticas:
    1. Calibración de ángulo gamma con propulsores RCS (ventana 5.5° - 7.0°).
    2. Frenado aerodinámico hipersónico con control de alabeo, disipación de W_roz, blackout de comunicaciones y control de fuerzas G sobre la tripulación.
    3. Secuencia de paracaídas (drogues y principales a 2.500 m) y amerizaje final a 8.0 m/s con cálculo de impacto hidrodinámico.
  - Registrar cada etapa en el arreglo `mision->historial[mision->total_puntos++]`.
  - En `simulador_mostrar_reporte`: mostrar tabla de telemetría resumen y veredicto final de supervivencia de la tripulación.

Por favor, antes de escribir código, confirma que comprendes la arquitectura, los structs existentes y las convenciones de logging y validación.
```
