#include "focp_target.h"

/*
 * Concurrency model (single core, C28x):
 *  - The TX queue is written from the current ISR (telemetry) and from the background
 *    task (parameter responses, status). Writers protect the head update with a short
 *    interrupt lock; the reader (background) only moves the tail.
 *  - Provide FOCP_TARGET_IRQ_SAVE/RESTORE for your platform. Defaults are for C28x.
 */
#if defined(__TMS320C28XX__)
#include <stdint.h>
extern uint16_t focp_target_irq_save(void);
extern void focp_target_irq_restore(uint16_t s);
/* Example implementation (driverlib): return __disable_interrupts(); / __restore_interrupts(s); */
#define FOCP_TARGET_IRQ_SAVE()      focp_target_irq_save()
#define FOCP_TARGET_IRQ_RESTORE(s)  focp_target_irq_restore(s)
#else
#define FOCP_TARGET_IRQ_SAVE()      0u
#define FOCP_TARGET_IRQ_RESTORE(s)  ((void)(s))
#endif

#define FOCP_TARGET_CMDQ 8u

static focp_dev_t s_dev;
static float s_sig[FOCP_NUM_SIGNALS];
static volatile uint16_t s_locked;

static focp_msg_t s_txq[FOCP_TARGET_TXQ];
static volatile uint16_t s_tx_head, s_tx_tail;

static uint16_t s_cmd[FOCP_TARGET_CMDQ];
static float s_cmd_arg[FOCP_TARGET_CMDQ];
static volatile uint16_t s_cmd_head, s_cmd_tail;
static volatile float s_ref;

static int tx_push(void *ctx, const focp_msg_t *m)
{
    uint16_t s, next;
    (void)ctx;
    s = FOCP_TARGET_IRQ_SAVE();
    next = (uint16_t)((s_tx_head + 1u) & (FOCP_TARGET_TXQ - 1u));
    if (next == s_tx_tail) { FOCP_TARGET_IRQ_RESTORE(s); return -1; }   /* full: drop */
    s_txq[s_tx_head] = *m;
    s_tx_head = next;
    FOCP_TARGET_IRQ_RESTORE(s);
    return 0;
}

static int is_locked(void *ctx) { (void)ctx; return s_locked ? 1 : 0; }

static void on_command(void *ctx, uint16_t cmd, const focp_byte_t *args, uint16_t nargs)
{
    uint16_t next;
    float arg = 0.0f;
    (void)ctx;
    if (cmd == FOCP_CMD_SET_REF && nargs >= 4u) { s_ref = focp_get_f32(args); return; }
    if (nargs >= 4u) arg = focp_get_f32(args);
    else if (nargs >= 1u) arg = (float)(args[0] & 0xFFu);
    next = (uint16_t)((s_cmd_head + 1u) % FOCP_TARGET_CMDQ);
    if (next == s_cmd_tail) return;                      /* queue full: ignore */
    s_cmd[s_cmd_head] = cmd;
    s_cmd_arg[s_cmd_head] = arg;
    s_cmd_head = next;
}

void focp_target_init(uint16_t node)
{
    focp_dev_cfg_t cfg;
    uint16_t i;
    cfg.node = node;
    cfg.fw_major = 0;
    cfg.fw_minor = 1;
    cfg.board_id = 0x01;            /* CarPediem F28388D */
    cfg.ctx = 0;
    cfg.send = tx_push;
    cfg.on_command = on_command;
    cfg.is_locked = is_locked;
    cfg.on_param_changed = 0;
    s_tx_head = s_tx_tail = 0;
    s_cmd_head = s_cmd_tail = 0;
    s_locked = 0;
    s_ref = 0.0f;
    for (i = 0; i < FOCP_NUM_SIGNALS; ++i) s_sig[i] = 0.0f;
    focp_dev_init(&s_dev, &cfg);
}

void focp_target_rx_can(uint32_t can_id, const uint16_t *data, uint16_t len)
{
    focp_msg_t m;
    uint16_t i, fn;
    if (can_id > 0x7FFu) return;
    fn = FOCP_CAN_FN(can_id);
    if (fn != FOCP_FN_CMD && fn != FOCP_FN_PARAM_REQ) return;
    m.fn = fn;
    m.node = FOCP_CAN_NODE(can_id);
    m.len = (len > FOCP_MAX_PAYLOAD) ? FOCP_MAX_PAYLOAD : len;
    for (i = 0; i < FOCP_MAX_PAYLOAD; ++i) m.data[i] = (focp_byte_t)((i < m.len) ? (data[i] & 0xFFu) : 0u);
    focp_dev_rx(&s_dev, &m);
}

uint16_t focp_target_tx_pop(uint32_t *can_id, uint16_t *data, uint16_t *len)
{
    const focp_msg_t *m;
    uint16_t i;
    if (s_tx_tail == s_tx_head) return 0;
    m = &s_txq[s_tx_tail];
    *can_id = FOCP_CAN_ID(m->fn, m->node);
    *len = m->len;
    for (i = 0; i < FOCP_MAX_PAYLOAD; ++i) data[i] = (uint16_t)(m->data[i] & 0xFFu);
    s_tx_tail = (uint16_t)((s_tx_tail + 1u) & (FOCP_TARGET_TXQ - 1u));
    return 1;
}

void focp_target_set_signal(uint16_t sig_idx, float value)
{
    if (sig_idx < FOCP_NUM_SIGNALS) s_sig[sig_idx] = value;
}

void focp_target_sample(void) { focp_dev_sample(&s_dev, s_sig); }

uint16_t focp_target_tick_1ms(void) { return (uint16_t)focp_dev_tick_ms(&s_dev, 1); }

void focp_target_status(uint16_t state, uint16_t mode, uint16_t sensor, uint16_t faults, float udc, float temp)
{
    focp_dev_send_status(&s_dev, state, mode, sensor, faults, udc, temp);
}

void focp_target_event(uint16_t ev, uint16_t code, uint32_t t_ms) { focp_dev_send_event(&s_dev, ev, code, t_ms); }

void focp_target_set_locked(uint16_t locked) { s_locked = locked; }

float focp_target_param(uint16_t param_idx)
{
    return (param_idx < FOCP_NUM_PARAMS) ? s_dev.param[param_idx] : 0.0f;
}

void focp_target_param_set(uint16_t param_idx, float value)
{
    if (param_idx < FOCP_NUM_PARAMS) s_dev.param[param_idx] = value;
}

uint16_t focp_target_cmd_pending(void)
{
    return (uint16_t)((s_cmd_head + FOCP_TARGET_CMDQ - s_cmd_tail) % FOCP_TARGET_CMDQ);
}

uint16_t focp_target_cmd_take(float *arg)
{
    uint16_t c;
    if (s_cmd_tail == s_cmd_head) return 0;
    c = s_cmd[s_cmd_tail];
    if (arg) *arg = s_cmd_arg[s_cmd_tail];
    s_cmd_tail = (uint16_t)((s_cmd_tail + 1u) % FOCP_TARGET_CMDQ);
    return c;
}

float focp_target_reference(void) { return s_ref; }
