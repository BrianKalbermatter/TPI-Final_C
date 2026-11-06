#include <stdio.h>
#include <string.h>
#include <math.h>
#include "simulador.h"

void inicializar_mision(MisionArtemis *mision) {
    mision->altitud_actual = ALTITUD_INICIAL;
    mision->velocidad_actual = VEL_INICIAL;
    mision->integridad_escudo = 100.0f;
    mision->max_g_soportada = 0.0f;
    mision->trabajo_total_rozamiento = 0.0f;
    mision->paracaidas_desplegados = 0;
    mision->cantidad_turnos = 0;
    mision->estado = EN_CURSO;
}

void guardar_turno_historial(MisionArtemis *mision, float angulo, float g_actual, float w_roz, const char *desc_evento) {
    if (mision->cantidad_turnos < MAX_TURNOS) {
        int idx = mision->cantidad_turnos;
        mision->historial[idx].numero_turno = idx + 1;
        mision->historial[idx].altitud_m = mision->altitud_actual;
        mision->historial[idx].velocidad_ms = mision->velocidad_actual;
        mision->historial[idx].angulo_gamma = angulo;
        mision->historial[idx].fuerzas_g = g_actual;
        mision->historial[idx].trabajo_rozamiento_j = w_roz;
        strncpy(mision->historial[idx].evento, desc_evento, 79);
        mision->historial[idx].evento[79] = '\0';
        mision->cantidad_turnos++;
    }
}

void actualizar_estado_turno(MisionArtemis *mision, float angulo_elegido, int accion_paracaidas, float dt) {
    float v_inicial_turno = mision->velocidad_actual;
    float v_nueva = v_inicial_turno;
    float dh = 0.0f;
    char evento_desc[80] = "Frenado aerodinamico nominal";

    if (mision->altitud_actual > 10000.0f) {
        if (angulo_elegido < 5.5f) {
            mision->estado = FRACASO_REBOTE;
            guardar_turno_historial(mision, angulo_elegido, 0.0f, 0.0f, "CRITICO: Rebote atmosferico (gamma < 5.5)");
            return;
        } else if (angulo_elegido > 7.0f) {
            mision->integridad_escudo -= 65.0f;
            v_nueva = v_inicial_turno * 0.20f;
            dh = 45000.0f;
            strcpy(evento_desc, "ALERTA: Sobrecarga G y dano termico (gamma > 7.0)");
        } else {
            float factor_frenado = 0.22f + ((angulo_elegido - 5.5f) * 0.05f);
            v_nueva = v_inicial_turno * factor_frenado;
            dh = (mision->altitud_actual > 40000.0f) ? 55000.0f : 37500.0f;
            mision->integridad_escudo -= 12.0f;
        }
    } else {
        if (accion_paracaidas == 1) {
            if (mision->velocidad_actual > 120.0f) {
                mision->integridad_escudo -= 50.0f;
                strcpy(evento_desc, "Paracaidas rasgados por exceso de velocidad");
                v_nueva = 45.0f;
            } else {
                mision->paracaidas_desplegados = 1;
                v_nueva = VEL_IMPACTO_SEGURA;
                strcpy(evento_desc, "Paracaidas principales desplegados con exito");
            }
        } else {
            v_nueva = (mision->paracaidas_desplegados) ? VEL_IMPACTO_SEGURA : 25.0f;
            strcpy(evento_desc, "Descenso final hacia el Oceano Pacifico");
        }
        dh = mision->altitud_actual;
    }

    if (mision->altitud_actual - dh <= 2500.0f && mision->altitud_actual > 2500.0f) {
        mision->altitud_actual = ALT_PARACAIDAS;
        v_nueva = 25.0f;
    } else {
        mision->altitud_actual -= dh;
        if (mision->altitud_actual < 0.0f) mision->altitud_actual = 0.0f;
    }

    float k_inicial = 0.5f * MASA_ORION * (v_inicial_turno * v_inicial_turno);
    float k_final = 0.5f * MASA_ORION * (v_nueva * v_nueva);
    float w_roz_turno = k_final - k_inicial;
    mision->trabajo_total_rozamiento += w_roz_turno;

    float desaceleracion_ms2 = fabs(v_nueva - v_inicial_turno) / dt;
    float g_turno = desaceleracion_ms2 / GRAVEDAD;
    if (g_turno > mision->max_g_soportada) {
        mision->max_g_soportada = g_turno;
    }

    mision->velocidad_actual = v_nueva;

    if (mision->integridad_escudo <= 0.0f || g_turno > 15.0f) {
        mision->integridad_escudo = 0.0f;
        mision->estado = FRACASO_DESINTEGRACION;
        guardar_turno_historial(mision, angulo_elegido, g_turno, w_roz_turno, "CATACLISMO: Fallo estructural / Escudo destruido");
        return;
    }

    if (mision->altitud_actual <= 0.0f) {
        if (mision->paracaidas_desplegados == 1 && mision->velocidad_actual <= 12.0f) {
            mision->estado = EXITO;
            strcpy(evento_desc, "AMERIZAJE EXITOSO en el Oceano Pacifico");
        } else {
            mision->estado = FRACASO_IMPACTO;
            strcpy(evento_desc, "IMPACTO CRITICO: Sin paracaidas en el amerizaje");
        }
    }

    guardar_turno_historial(mision, angulo_elegido, g_turno, w_roz_turno, evento_desc);
}


static float leer_flotante_rango(const char *mensaje, float min, float max) {
    float valor;
    int leidos;
    do {
        printf("%s (%.1f - %.1f): ", mensaje, min, max);
        leidos = scanf("%f", &valor);
        while (getchar() != '\n');
        if (leidos != 1 || valor < min || valor > max) {
            printf("  [!] Entrada invalida. Ingrese un valor entre %.1f y %.1f.\n", min, max);
        }
    } while (leidos != 1 || valor < min || valor > max);
    return valor;
}

static int leer_entero_rango(const char *mensaje, int min, int max) {
    int valor;
    int leidos;
    do {
        printf("%s (%d - %d): ", mensaje, min, max);
        leidos = scanf("%d", &valor);
        while (getchar() != '\n');
        if (leidos != 1 || valor < min || valor > max) {
            printf("  [!] Opcion invalida. Ingrese un entero entre %d y %d.\n", min, max);
        }
    } while (leidos != 1 || valor < min || valor > max);
    return valor;
}

void iniciar_ciclo_simulacion(MisionArtemis *mision) {
    inicializar_mision(mision);

    printf("\n======================================================================\n");
    printf("   SIMULADOR ARTEMIS - FASE DE REINGRESO ATMOSFERICO Y AMERIZAJE\n");
    printf("======================================================================\n");
    printf("Nave: Capsula Orion (Masa: %.0f kg)\n", MASA_ORION);
    printf("Condiciones iniciales: Altitud = %.0f m | Velocidad = %.0f m/s (Mach 32)\n",
           mision->altitud_actual, mision->velocidad_actual);
    printf("Ventana de seguridad del angulo de reingreso (gamma): [5.5 a 7.0 grados]\n");
    printf("----------------------------------------------------------------------\n");

    while (mision->estado == EN_CURSO && mision->cantidad_turnos < MAX_TURNOS) {
        printf("\n>>> TURNO %d | Altitud: %.1f m | Velocidad: %.1f m/s | Escudo: %.1f%% <<<\n",
               mision->cantidad_turnos + 1,
               mision->altitud_actual,
               mision->velocidad_actual,
               mision->integridad_escudo);

        float angulo = 6.0f;
        int abrir_paracaidas = 0;
        float dt_turno = 130.0f;

        if (mision->altitud_actual > 10000.0f) {
            angulo = leer_flotante_rango("Ingrese angulo de ataque gamma en grados", 3.0f, 10.0f);
            dt_turno = leer_flotante_rango("Ingrese duracion de la maniobra termica (s)", 80.0f, 250.0f);
        } else {
            printf("  [ALERTA DE ALTITUD BAJA: %.1f m - Velocidad: %.1f m/s]\n",
                   mision->altitud_actual, mision->velocidad_actual);
            abrir_paracaidas = leer_entero_rango("¿Desplegar paracaidas principales? (1 = SI, 0 = NO)", 0, 1);
            dt_turno = 60.0f;
        }

        actualizar_estado_turno(mision, angulo, abrir_paracaidas, dt_turno);
    }

    mostrar_reporte_final(mision);
}

void mostrar_reporte_final(const MisionArtemis *mision) {
    printf("\n\n================================================================================\n");
    printf("                    REPORTE FINAL DE DESEMPENO - MISION ARTEMIS                 \n");
    printf("================================================================================\n");

    printf("ESTADO DE LA MISION: ");
    switch (mision->estado) {
        case EXITO:
            printf("[ EXITO - TRIPULACION RECUPERADA SANA Y SALVA ]\n");
            break;
        case FRACASO_REBOTE:
            printf("[ FRACASO - REBOTE ATMOSFERICO (Angulo gamma < 5.5 grados) ]\n");
            break;
        case FRACASO_DESINTEGRACION:
            printf("[ FRACASO - DESINTEGRACION TERMICA / SOBRECARGA G (Angulo gamma > 7.0 grados) ]\n");
            break;
        case FRACASO_IMPACTO:
            printf("[ FRACASO - IMPACTO SEVERO CONTRA EL OCEANO (Fallo de paracaidas) ]\n");
            break;
        default:
            printf("[ MISION INCOMPLETA ]\n");
            break;
    }

    printf("\n--- HISTORIAL DE TELEMETRIA POR TURNOS ---\n");
    printf("%-6s | %-10s | %-10s | %-7s | %-7s | %-32s\n",
           "Turno", "Altitud(m)", "Vel.(m/s)", "Gamma", "Pico G", "Evento Registrado");
    printf("--------------------------------------------------------------------------------\n");
    for (int i = 0; i < mision->cantidad_turnos; i++) {
        printf("%-6d | %-10.1f | %-10.1f | %-7.2f | %-7.2f | %-32s\n",
               mision->historial[i].numero_turno,
               mision->historial[i].altitud_m,
               mision->historial[i].velocidad_ms,
               mision->historial[i].angulo_gamma,
               mision->historial[i].fuerzas_g,
               mision->historial[i].evento);
    }

    float peso_nave = MASA_ORION * GRAVEDAD;
    float distancia_hundimiento = 1.50f;
    float energia_impacto = 0.5f * MASA_ORION * (mision->velocidad_actual * mision->velocidad_actual);
    float f_prom_agua = (energia_impacto / distancia_hundimiento) + peso_nave;
    float multiplo_peso = f_prom_agua / peso_nave;

    printf("\n--- METRICAS FISICAS Y ANALITICAS FINALES ---\n");
    printf(" * Trabajo Total de Rozamiento Atmosferico (W_roz): %.3e Joules (%.2f GJ)\n",
           mision->trabajo_total_rozamiento, mision->trabajo_total_rozamiento / 1e9f);
    printf(" * Desaceleracion Maxima Experimentada:             %.2f m/s^2 (%.2f G)\n",
           mision->max_g_soportada * GRAVEDAD, mision->max_g_soportada);
    printf(" * Integridad Final del Escudo Termico:             %.1f %%\n", mision->integridad_escudo);
    printf(" * Velocidad de Amerizaje:                          %.2f m/s\n", mision->velocidad_actual);
    printf(" * Fuerza Promedio del Agua (d = 1.50 m):           %.2f N (%.2f veces el peso de Orion)\n",
           f_prom_agua, multiplo_peso);
    printf("================================================================================\n\n");
}
