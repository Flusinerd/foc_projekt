/*
 * FOCP application glue for app_speed (TMS320F28386D, eCAN-A).
 *
 * Called once per speed-control cycle (Ts_speed, 2 kHz) from a MATLAB Function block via
 * coder.ceval (code generation only; the simulation keeps the legacy behaviour).
 *
 *  - CAN: own message objects 20..31 next to the blockset's (2, 3 TX; 10, 16 RX).
 *    RX FIFO 20..27 accepts CMD (0x201) and PARAM_REQ (0x211) for node 1, TX round robin 28..31.
 *  - Switching (decision 2026-11-04): until the first FOCP frame arrives the drive runs from
 *    the potentiometer as before. Afterwards the speed reference comes from SET_REF, the drive
 *    stays in Freemode until ENABLE, and a heartbeat timeout forces Freemode until ACK_FAULT.
 *  - STATUS every 100 ms (with Udc), telemetry on request by the host: speed, speed_ref, udc,
 *    id/iq and their references (A), hall_state. Temperature is still reported as 0.
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

/* Inputs: legacy signals (0/1), speedMode 1..3, measured speed in PU, N_base in rpm,
 * Udc in V, [id iq] reference and measurement in PU, I_base in A, Hall state (C|B|A).
 * Outputs: active (FOCP owns the reference), freemode request, speed reference 0..1
 * (same scaling as the potentiometer value Speed_Ref). */
void focp_c28x_step(uint16_t undervoltage, uint16_t freemode_can, uint16_t hot_temp,
                    uint16_t safe_state, uint16_t speed_mode, float speed_pu, float n_base,
                    float udc, const float *idq_ref_pu, const float *idq_pu, float i_base,
                    uint16_t hall_state, uint16_t *active, uint16_t *freemode, float *speed_ref);

/* Host tests only: reset the internal state (the target initialises on the first step). */
void focp_c28x_reset(void);

#ifdef __cplusplus
}
#endif
#endif
