/*
 * FOCP target binding for the Simulink/Embedded Coder firmware (TI C28x).
 *
 * Thin, global wrapper around focp_dev so the protocol can be used from Simulink
 * "C Caller" blocks without passing structs around. Add to the model via
 * Configuration Parameters -> Code Generation -> Custom Code:
 *     Include dirs : protocol/include, firmware
 *     Source files : protocol/src/focp.c, protocol/src/focp_dev.c, firmware/focp_target.c
 *
 * Call pattern (rates as in the concept):
 *   System Initialize ...... focp_target_init(node)
 *   CAN RX (eCAN Receive) .. focp_target_rx_can(id, data, len)   for every received frame
 *   CAN TX (eCAN Transmit).. while (focp_target_tx_pop(&id, data, &len)) send   (background task)
 *   current ISR (20 kHz) ... focp_target_set_signal(FOCP_S_..., value) ... ; focp_target_sample()
 *   1 ms task .............. comm_fault = focp_target_tick_1ms()
 *   100 ms task ............ focp_target_status(state, mode, sensor, faults, udc, temp)
 *   anywhere ............... focp_target_param(FOCP_P_CUR_KP)
 *   commands ............... focp_target_cmd_pending() / focp_target_cmd_take()
 *
 * Status: written and host-compiled, not yet run on the C28x.
 */
#ifndef FOCP_TARGET_H
#define FOCP_TARGET_H

#include "focp/focp_dev.h"

#ifdef __cplusplus
extern "C" {
#endif

#define FOCP_TARGET_TXQ 64u   /* frames; must be a power of two */

void     focp_target_init(uint16_t node);

/* RX: CAN frame -> protocol. data[] holds one byte per element (C28x: 16-bit elements). */
void     focp_target_rx_can(uint32_t can_id, const uint16_t *data, uint16_t len);

/* TX: fetch next frame for the eCAN Transmit block. Returns 1 if a frame was written. */
uint16_t focp_target_tx_pop(uint32_t *can_id, uint16_t *data, uint16_t *len);

/* Telemetry */
void     focp_target_set_signal(uint16_t sig_idx, float value);
void     focp_target_sample(void);

/* Supervision */
uint16_t focp_target_tick_1ms(void);                 /* 1 = heartbeat timeout */
void     focp_target_status(uint16_t state, uint16_t mode, uint16_t sensor,
                            uint16_t faults, float udc, float temp);
void     focp_target_event(uint16_t ev, uint16_t code, uint32_t t_ms);
void     focp_target_set_locked(uint16_t locked);    /* 1 while RUN/CALIB */

/* Parameters */
float    focp_target_param(uint16_t param_idx);
void     focp_target_param_set(uint16_t param_idx, float value);   /* for RO values, e.g. ISR load */

/* Application commands received from the host (ENABLE, DISABLE, ACK_FAULT, SET_REF, CALIBRATE, SAVE) */
uint16_t focp_target_cmd_pending(void);              /* number of queued commands */
uint16_t focp_target_cmd_take(float *arg);           /* returns focp_cmd_t, arg = f32 or u8 argument */
float    focp_target_reference(void);                /* last SET_REF value */

#ifdef __cplusplus
}
#endif
#endif
