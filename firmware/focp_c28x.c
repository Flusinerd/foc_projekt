#include "focp_c28x.h"
#include "focp_target.h"

#if defined(__TMS320C28XX__)
#include "driverlib.h"

#define CAN_BASE      CANA_BASE
#define OBJ_RX_FIRST  20u
#define OBJ_RX_LAST   23u
#define OBJ_TX_FIRST  24u
#define OBJ_TX_LAST   31u

uint16_t focp_target_irq_save(void) { return __disable_interrupts(); }
void focp_target_irq_restore(uint16_t s) { __restore_interrupts(s); }

static uint16_t s_tx_next = OBJ_TX_FIRST;

static void hal_init(void)
{
    uint32_t obj;
    /* FIFO over 20..23: ID 0x2?1 with bit 4 free -> 0x201 (CMD) and 0x211 (PARAM_REQ), node 1 */
    for (obj = OBJ_RX_FIRST; obj <= OBJ_RX_LAST; ++obj)
        CAN_setupMessageObject(CAN_BASE, obj, 0x201u, CAN_MSG_FRAME_STD, CAN_MSG_OBJ_TYPE_RX, 0x7EFu,
                               CAN_MSG_OBJ_USE_ID_FILTER | (obj < OBJ_RX_LAST ? CAN_MSG_OBJ_FIFO : 0u), 8u);
}

static void hal_rx(void)
{
    uint32_t obj, id;
    uint16_t data[8], len;
    CAN_MsgFrameType type;
    for (obj = OBJ_RX_FIRST; obj <= OBJ_RX_LAST; ++obj) {
        if (!CAN_readMessageWithID(CAN_BASE, obj, &type, &id, data)) continue;
        len = HWREGH(CAN_BASE + CAN_O_IF2MCTL) & CAN_IF2MCTL_DLC_M;   /* IF2 still holds the object */
        focp_target_rx_can(id, data, len);
    }
}

static void hal_tx(void)
{
    uint32_t id, busy = CAN_getTxRequests(CAN_BASE);
    uint16_t data[8], len, n;
    for (n = 0; n <= OBJ_TX_LAST - OBJ_TX_FIRST; ++n) {
        const uint16_t obj = s_tx_next;
        if (busy & (1UL << (obj - 1u))) break;                      /* oldest object still pending */
        if (!focp_target_tx_pop(&id, data, &len)) break;
        CAN_setupMessageObject(CAN_BASE, obj, id, CAN_MSG_FRAME_STD, CAN_MSG_OBJ_TYPE_TX, 0u,
                               CAN_MSG_OBJ_NO_FLAGS, len);
        CAN_sendMessage(CAN_BASE, obj, len, data);
        s_tx_next = (obj == OBJ_TX_LAST) ? OBJ_TX_FIRST : (uint16_t)(obj + 1u);
    }
}
#else
/* Host build (tests): no CAN hardware, frames go through focp_target_rx_can/tx_pop directly. */
static void hal_init(void) {}
static void hal_rx(void) {}
static void hal_tx(void) {}
#endif

#define STEPS_PER_MS     2u     /* Ts_speed = 0.5 ms */
#define STATUS_PERIOD_MS 100u

static uint16_t s_init, s_active, s_enabled, s_comm_fault, s_div_ms, s_ms_status;
static uint32_t s_rx_seen;

void focp_c28x_reset(void) { s_init = 0; }

void focp_c28x_step(uint16_t undervoltage, uint16_t freemode_can, uint16_t hot_temp,
                    uint16_t safe_state, uint16_t speed_mode, float speed_pu, float n_base,
                    uint16_t *active, uint16_t *freemode, float *speed_ref)
{
    float arg, span, ref;
    uint16_t cmd, faults, fm;

    if (!s_init) {
        focp_target_init(FOCP_C28X_NODE);
        hal_init();
        s_init = 1;
        s_active = s_enabled = s_comm_fault = 0;
        s_div_ms = s_ms_status = 0;
        s_rx_seen = 0;
    }

    hal_rx();
    if (focp_target_rx_count() != s_rx_seen) {      /* first FOCP frame switches to FOCP */
        s_rx_seen = focp_target_rx_count();
        s_active = 1;
    }
    while ((cmd = focp_target_cmd_take(&arg)) != 0u) {
        if (cmd == FOCP_CMD_ENABLE && !s_comm_fault) s_enabled = 1;
        else if (cmd == FOCP_CMD_DISABLE) s_enabled = 0;
        else if (cmd == FOCP_CMD_ACK_FAULT) s_comm_fault = 0;
    }

    if (++s_div_ms >= STEPS_PER_MS) {
        s_div_ms = 0;
        if (focp_target_tick_1ms() && s_active && s_enabled) { s_comm_fault = 1; s_enabled = 0; }
        if (++s_ms_status >= STATUS_PERIOD_MS) {
            s_ms_status = 0;
            faults = 0;
            if (undervoltage) faults |= FOCP_FAULT_UNDERVOLTAGE;
            if (hot_temp) faults |= FOCP_FAULT_OVERTEMP;
            if (!safe_state) faults |= FOCP_FAULT_SAFE_INPUT;
            if (s_comm_fault) faults |= FOCP_FAULT_COMM;
            fm = (uint16_t)(undervoltage || freemode_can || hot_temp || !safe_state || (s_active && !s_enabled));
            focp_target_status(s_comm_fault ? FOCP_ST_FAULT : (fm ? FOCP_ST_READY : FOCP_ST_RUN),
                               FOCP_MODE_SPEED, FOCP_SENS_HALL, faults, 0.0f, 0.0f);
        }
    }

    /* Speed calculator in the model: N_ref = Speed_Ref * floor(N_base/3) * speedMode */
    span = (float)((uint32_t)(n_base / 3.0f)) * (float)(speed_mode ? speed_mode : 1u);
    ref = span > 0.0f ? focp_target_reference() / span : 0.0f;
    if (ref < 0.0f) ref = 0.0f;
    if (ref > 1.0f) ref = 1.0f;

    focp_target_set_locked(s_enabled);
    focp_target_set_signal(FOCP_S_SPEED, speed_pu * n_base);
    focp_target_set_signal(FOCP_S_SPEED_REF, ref * span);
    focp_target_sample();
    hal_tx();

    *active = s_active;
    *freemode = (uint16_t)(s_active && !s_enabled);
    *speed_ref = ref;
}
