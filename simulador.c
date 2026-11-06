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
