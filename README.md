# Simulador de Misiones Artemis

Trabajo Práctico Integrador en C — UTN FRRe.

**Escenario:** Reingreso Atmosférico, Frenado Aerodinámico y Amerizaje (Trabajo, Energía e Impacto).

## Archivos

| Archivo | Contenido |
|---|---|
| `simulador.h` | Constantes físicas, estructura `ResultadoArtemis` y prototipos |
| `simulador.c` | Cálculos físicos, validaciones y mensajes de error |

Lenguaje: C únicamente (ANSI C / C99).

## Descripción del escenario

Tras completar el sobrevuelo lunar, el módulo de tripulación Orion reingresa a la atmósfera terrestre. El escudo térmico y la resistencia aerodinámica reducen su velocidad, y luego ameriza en el Océano Pacífico.

| Dato | Símbolo | Valor |
|---|---|---|
| Masa | m | 10400 kg |
| Velocidad inicial | vi | 11000 m/s |
| Altitud inicial | hi | 120000 m |
| Velocidad final | vf | 25 m/s |
| Altitud final | hf | 2500 m |
| Velocidad de impacto | vImpacto | 8 m/s |
| Distancia de frenado en el agua | d | 1,5 m |
| Tiempo de desaceleración atmosférica | t | 400 s |
| Ventana segura de reingreso | γ | 5,5° ≤ γ ≤ 7,0° |

Constante usada: g = 9,81 m/s² (definida en `simulador.h`, considerada constante durante todo el reingreso).

## Reglas del escenario

### Pregunta 1 — Trabajo de rozamiento
`double calcularTrabajoRozamiento(masa, vi, vf, hi, hf)`

Teorema trabajo-energía: `Wroz + Wg = ΔEc`, con `ΔEc = ½·m·(vf² − vi²)` y `Wg = m·g·(hi − hf)`. Se despeja `Wroz = ΔEc − Wg`.

Se valida: `masa > 0`, `hi > hf`, velocidades `>= 0`.

### Pregunta 2 — Desaceleración media y factor g
`double calcularDesaceleracionMedia(vi, vf, tiempo)` y `double calcularFactorG(aceleracion)`

`a = (vf − vi) / t` y `gCarga = |a| / g`.

Se valida: `tiempo > 0`.

### Pregunta 3 — Fuerza de amerizaje
`double calcularFuerzaAmerizaje(masa, velocidadImpacto, distanciaFrenado)` y `double calcularMultiploPeso(fuerza, masa)`

Trabajo-energía: `Fprom · d = ½·m·v²`, entonces `Fprom = (½·m·v²) / d`. El múltiplo del peso es `Fprom / (m·g)`.

Se valida: `masa > 0`, `velocidadImpacto >= 0`, `distanciaFrenado > 0`.

### Pregunta 4 — Ángulo de reingreso
`void analizarAnguloReingreso(double gamma)`

| Condición | Resultado mostrado |
|---|---|
| `γ > 7,0` | desaceleración excesiva, calentamiento extremo, riesgo estructural, peligro para la tripulación |
| `γ < 5,5` | rebote atmosférico, pérdida de frenado, posible retorno al espacio, desviación de la zona de amerizaje |
| `5,5 ≤ γ ≤ 7,0` | trayectoria segura |

### Estructura y punteros
`resolverEscenarioArtemis(ResultadoArtemis *resultado, ...)` completa mediante un puntero la estructura `ResultadoArtemis` (trabajoRozamiento, desaceleracion, factorG, fuerzaAmerizaje, multiploPeso).

### Casos sin solución física
Cuando un dato es físicamente imposible, la función informa el error por `stderr` y devuelve `NAN` en lugar de calcular. El resultado se verifica con `isnan()`. Una entrada inválida deja también en `NAN` los resultados que dependen de ella.

Validaciones agregadas, que no están en la consigna original:
- `hf >= 0`.
- `Wroz <= 0`: el rozamiento no puede entregar energía a la nave.
- `0 < γ <= 90°` en `analizarAnguloReingreso`.

## Resultados con los datos del enunciado

Ingresando `10400 11000 25 120000 2500 400 8 1.5 6` (masa, vi, vf, hi, hf, t, vImpacto, d, γ), el programa da estos valores, que coinciden con la resolución hecha a mano:

| Magnitud | Resultado |
|---|---|
| Wroz | −6,41185 · 10¹¹ J (ΔEc = −6,29197 · 10¹¹ J; Wg = 1,19878 · 10¹⁰ J) |
| a | −27,4375 m/s² |
| Factor g | 2,797 g |
| Fprom | 221 866,67 N |
| Múltiplo del peso | 2,175 veces |
