/*
 * FOCP drive side ("device") - parameter storage, command dispatch, telemetry streaming.
 *
 * Runs on the target (called from Simulink via C Caller / S-Function wrapper) and in the
 * host's virtual drive, so both execute exactly the same protocol code.
 *
 * Typical integration:
 *   init:            focp_dev_init(&dev, &cfg);
 *   CAN/UART RX:     focp_dev_rx(&dev, &msg);                 (background task)
 *   current ISR:     focp_dev_sample(&dev, signals);          (fills TX queue)
 *   1 ms task:       if (focp_dev_tick_ms(&dev, 1)) -> FOCP_FAULT_COMM
 *   100 ms task:     focp_dev_send_status(&dev, ...);
 *   control code:    focp_dev_get(&dev, FOCP_P_CUR_KP)
 */
#ifndef FOCP_DEV_H
#define FOCP_DEV_H

#include "focp.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint16_t node;
    uint16_t fw_major;
    uint16_t fw_minor;
    uint16_t board_id;
    void    *ctx;

    /* Transmit one message. Must be ISR-safe if focp_dev_sample() runs in an ISR.
     * Return 0 on success, nonzero if the TX queue is full (frame is dropped). */
    int  (*send)(void *ctx, const focp_msg_t *m);

    /* Application commands: ENABLE, DISABLE, ACK_FAULT, SET_REF, CALIBRATE, SAVE_PARAMS.
     * Telemetry, info, heartbeat and LOAD_DEFAULTS are handled internally. */
    void (*on_command)(void *ctx, uint16_t cmd, const focp_byte_t *args, uint16_t nargs);

    /* Nonzero while FOCP_F_LOCK parameters may not be changed (drive running). Optional. */
    int  (*is_locked)(void *ctx);

    /* Called after a parameter was written by the host. Optional. */
    void (*on_param_changed)(void *ctx, uint16_t idx);
} focp_dev_cfg_t;

typedef struct {
    focp_dev_cfg_t cfg;
    float    param[FOCP_NUM_PARAMS];
    uint16_t telem_slot[FOCP_TELEM_SLOTS];  /* signal index + 1, 0 = empty */
    uint16_t telem_div;
    uint16_t telem_cnt;
    uint16_t telem_run;
    uint16_t telem_seq;
    uint32_t hb_age_ms;
    uint32_t tx_dropped;
    uint32_t rx_count;
} focp_dev_t;

void  focp_dev_init(focp_dev_t *d, const focp_dev_cfg_t *cfg);
void  focp_dev_load_defaults(focp_dev_t *d);

/* Handle one message from the host (messages for other nodes are ignored). */
void  focp_dev_rx(focp_dev_t *d, const focp_msg_t *m);

/* Call once per control cycle with all signal values (index = focp_signal_idx_t). */
void  focp_dev_sample(focp_dev_t *d, const float *signals);

/* Advance heartbeat timer. Returns 1 if comm.timeout_ms elapsed without host traffic. */
int   focp_dev_tick_ms(focp_dev_t *d, uint16_t ms);

void  focp_dev_send_status(focp_dev_t *d, uint16_t state, uint16_t mode, uint16_t sensor,
                           uint16_t faults, float udc, float temp);
void  focp_dev_send_event(focp_dev_t *d, uint16_t ev, uint16_t code, uint32_t t_ms);

/* Parameter access for the application (no range check, use for RO values) */
static inline float focp_dev_get(const focp_dev_t *d, focp_param_idx_t i) { return d->param[i]; }
static inline void  focp_dev_set(focp_dev_t *d, focp_param_idx_t i, float v) { d->param[i] = v; }

/* Validate + store a value as if written by the host. Returns focp_param_status_t. */
uint16_t focp_dev_write_param(focp_dev_t *d, uint16_t idx, float value, int from_host);

#ifdef __cplusplus
}
#endif
#endif
