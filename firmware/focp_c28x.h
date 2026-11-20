/*
 * FOCP application glue for app_speed (TMS320F28386D, eCAN-A).
 *
 * focp_c28x_step is called once per speed-control cycle (Ts_speed, 2 kHz) and focp_c28x_isr in the
 * current ISR (20 kHz), both from MATLAB Function blocks via coder.ceval (code generation only;
 * the simulation keeps the legacy behaviour).
 *
 *  - CAN: own message objects 4..9 and 17..31 next to the blockset's (2, 3 TX; 10, 16 RX).
 *    RX FIFO 17..31 accepts CMD (0x201) and PARAM_REQ (0x211) for node 1, TX round robin 4..9.
 *  - Switching (decision 2026-11-04): until the first FOCP frame arrives the drive runs from
 *    the potentiometer as before. Afterwards the speed reference comes from SET_REF, the drive
 *    stays in Freemode until ENABLE, and a heartbeat timeout forces Freemode until ACK_FAULT.
 *  - STATUS every 100 ms (Udc, temperature). Telemetry is sampled in the 20 kHz current ISR:
 *    ia/ib/ic, id/iq, ud/uq, theta_el; slow signals (speed, references, udc, temp, hall) are held.
 *  - Controller gains and current limits can be written from FOC Studio (tunable parameters).
 *  - Overcurrent: one-shot trip of ePWM6..8 by CMPSS3 (I_B, configured in the model) or by the
 *    software check |i| > inv.I_max in focp_c28x_isr; reported as OC_HW / OC_SW, cleared by
 *    ACK_FAULT once the currents are below the threshold.
 *
 * Status: handwritten firmware C code, deliberate deviation from AGENTS.md (human decision,
 * matlab/reports/08_FOCP_Einbau_Plan_2026-11-04.md).
 */
#ifndef FOCP_C28X_H
#define FOCP_C28X_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FOCP_C28X_NODE 1u

/* Speed task (2 kHz). Inputs: legacy signals (0/1, incl. overvoltage), speedMode 1..3, measured speed in PU,
 * N_base in rpm, Udc in V, [id iq] reference in PU, I_base in A, V_base in V, temperature in degC,
 * Hall state (C|B|A), software overcurrent threshold in A (default for inv.I_max, used on the first step),
 * plausibility threshold for |ia+ib+ic| in A (0 = off).
 * Outputs: active (FOCP owns the reference), freemode request, speed reference 0..1
 * (same scaling as the potentiometer value Speed_Ref). */
void focp_c28x_step(uint16_t undervoltage, uint16_t overvoltage, uint16_t freemode_can, uint16_t hot_temp,
                    uint16_t safe_state, uint16_t speed_mode, float speed_pu, float n_base,
                    float udc, const float *idq_ref_pu, float i_base, float v_base,
                    float temp, uint16_t hall_state, float i_trip, float i_sum_max, uint16_t *active,
                    uint16_t *freemode, float *speed_ref);

/* Current ISR (20 kHz): [ia ib], electrical position, [id iq], [Vd Vq] in PU, raw ADC count of I_C
 * (ADCINB3). Software overcurrent and current-sum checks, fast telemetry signals, samples the
 * telemetry (divider 1 = every PWM period). */
void focp_c28x_isr(const float *iab_pu, float pos_pu, const float *idq_pu, const float *vdq_pu,
                   uint16_t ic_raw);

/* Tunable controller parameters (PU), written when FOC Studio changes cur.Kp/Ki, spd.Kp/Ki,
 * spd.Iq_max/Iq_min (PU) and inv.Udc_max (V). Defined by the generated model code (ExportedGlobal). */
extern float focp_tune_cur_Kp, focp_tune_cur_Ki, focp_tune_spd_Kp, focp_tune_spd_Ki;
extern float focp_tune_iq_max, focp_tune_iq_min, focp_tune_udc_max;
extern float focp_tune_i_trip;   /* software overcurrent threshold |i| in A (inv.I_max), defined in focp_c28x.c */

/* Host tests only: reset the internal state (the target initialises on the first step). */
void focp_c28x_reset(void);
#if !defined(__TMS320C28XX__)
void focp_c28x_host_cmpss(uint16_t on);        /* host tests: emulate the CMPSS3 comparator */
#endif

#ifdef __cplusplus
}
#endif
#endif
