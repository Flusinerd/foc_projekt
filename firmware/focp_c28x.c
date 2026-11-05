#include "focp_c28x.h"
#include "focp_target.h"

#if defined(__TMS320C28XX__)
#include "driverlib.h"

#define CAN_BASE      CANA_BASE
#define OBJ_RX_FIRST  17u
#define OBJ_RX_LAST   31u   /* 15 deep: a PARAM_REQ burst at full bus load brings up to ~7 frames per 0.5 ms */
#define OBJ_TX_FIRST  4u    /* 4..9 (2, 3, 10, 16 belong to the blockset) */
#define OBJ_TX_LAST   9u

uint16_t focp_target_irq_save(void) { return __disable_interrupts(); }
void focp_target_irq_restore(uint16_t s) { __restore_interrupts(s); }

static uint16_t s_tx_next = OBJ_TX_FIRST;

static void hal_init(void)
{
    uint32_t obj;
    /* FIFO over 17..31: ID 0x2?1 with bit 4 free -> 0x201 (CMD) and 0x211 (PARAM_REQ), node 1 */
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

#define STEPS_PER_MS     2u       /* Ts_speed = 0.5 ms */
#define STATUS_PERIOD_MS 100u
#define TELEM_BASE_HZ    20000.0f /* focp_c28x_isr runs in the current ISR (PWM frequency) */
#define N_TUNE           6u

static uint16_t s_init, s_active, s_enabled, s_comm_fault, s_div_ms, s_ms_status;
static uint32_t s_rx_seen;
static float s_i_base = 1.0f, s_v_base = 1.0f, s_n_base = 1.0f;

/* Tunable controller parameters in PU. On the target they are ExportedGlobal parameters of the
 * model (matlab/tools/foc_focp_tunable.m); on the host they live here for the tests. */
#if !defined(__TMS320C28XX__)
float focp_tune_cur_Kp, focp_tune_cur_Ki, focp_tune_spd_Kp, focp_tune_spd_Ki, focp_tune_iq_max, focp_tune_iq_min;
#endif

/* Values compiled into the firmware. LOAD_DEFAULTS restores these and not the table defaults,
 * whose spd.Iq_min = -5 A would brake into a lab supply. */
static float s_def[N_TUNE];
static const uint16_t s_tune_param[N_TUNE] = {FOCP_P_CUR_KP, FOCP_P_CUR_KI, FOCP_P_SPD_KP,
                                              FOCP_P_SPD_KI, FOCP_P_SPD_IQ_MAX, FOCP_P_SPD_IQ_MIN};

static float *tune_var(uint16_t k)
{
    switch (k) {
    case 0: return &focp_tune_cur_Kp;
    case 1: return &focp_tune_cur_Ki;
    case 2: return &focp_tune_spd_Kp;
    case 3: return &focp_tune_spd_Ki;
    case 4: return &focp_tune_iq_max;
    default: return &focp_tune_iq_min;
    }
}

/* SI value = PU value * factor: current loop V/A, speed loop A/rpm, limits A */
static float tune_factor(uint16_t k)
{
    if (k < 2u) return s_v_base / s_i_base;
    if (k < 4u) return s_i_base / s_n_base;
    return s_i_base;
}

static void publish_tunables(void)
{
    uint16_t k;
    for (k = 0; k < N_TUNE; ++k) focp_target_param_set(s_tune_param[k], *tune_var(k) * tune_factor(k));
}

static void on_param(uint16_t idx)
{
    uint16_t k;
    if (idx == 0xFFFFu) {                       /* LOAD_DEFAULTS */
        for (k = 0; k < N_TUNE; ++k) *tune_var(k) = s_def[k];
        publish_tunables();
        focp_target_param_set(FOCP_P_COMM_TELEM_F, TELEM_BASE_HZ);
        return;
    }
    for (k = 0; k < N_TUNE; ++k)
        if (s_tune_param[k] == idx) *tune_var(k) = focp_target_param(idx) / tune_factor(k);
}

void focp_c28x_reset(void) { s_init = 0; }

void focp_c28x_step(uint16_t undervoltage, uint16_t freemode_can, uint16_t hot_temp,
                    uint16_t safe_state, uint16_t speed_mode, float speed_pu, float n_base,
                    float udc, const float *idq_ref_pu, float i_base, float v_base,
                    float temp, uint16_t hall_state, uint16_t *active, uint16_t *freemode, float *speed_ref)
{
    float arg, span, ref;
    uint16_t cmd, faults, fm, k;

    s_i_base = i_base; s_v_base = v_base; s_n_base = n_base;
    if (!s_init) {
        focp_target_init(FOCP_C28X_NODE);
        focp_target_param_set(FOCP_P_COMM_TELEM_F, TELEM_BASE_HZ);
        for (k = 0; k < N_TUNE; ++k) s_def[k] = *tune_var(k);
        publish_tunables();                     /* FOC Studio shows what the firmware really uses */
        focp_target_set_param_callback(on_param);
        hal_init();
        s_active = s_enabled = s_comm_fault = 0;
        s_div_ms = s_ms_status = 0;
        s_rx_seen = 0;
        s_init = 1;                             /* last: enables focp_c28x_isr */
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
                               FOCP_MODE_SPEED, FOCP_SENS_HALL, faults, udc, temp);
        }
    }

    /* Speed calculator in the model: N_ref = Speed_Ref * floor(N_base/3) * speedMode */
    span = (float)((uint32_t)(n_base / 3.0f)) * (float)(speed_mode ? speed_mode : 1u);
    ref = span > 0.0f ? focp_target_reference() / span : 0.0f;
    if (ref < 0.0f) ref = 0.0f;
    if (ref > 1.0f) ref = 1.0f;

    /* slow signals; focp_c28x_isr adds the current-loop signals and samples at 20 kHz */
    focp_target_set_locked(s_enabled);
    focp_target_set_signal(FOCP_S_SPEED, speed_pu * n_base);
    focp_target_set_signal(FOCP_S_SPEED_REF, ref * span);
    focp_target_set_signal(FOCP_S_UDC, udc);
    focp_target_set_signal(FOCP_S_TEMP, temp);
    focp_target_set_signal(FOCP_S_ID_REF, idq_ref_pu[0] * i_base);
    focp_target_set_signal(FOCP_S_IQ_REF, idq_ref_pu[1] * i_base);
    focp_target_set_signal(FOCP_S_HALL_STATE, (float)hall_state);
    hal_tx();

    *active = s_active;
    *freemode = (uint16_t)(s_active && !s_enabled);
    *speed_ref = ref;
}

void focp_c28x_isr(const float *iab_pu, float pos_pu, const float *idq_pu, const float *vdq_pu)
{
    if (!s_init) return;                        /* bases and protocol are set up by the speed task */
    focp_target_set_signal(FOCP_S_IA, iab_pu[0] * s_i_base);
    focp_target_set_signal(FOCP_S_IB, iab_pu[1] * s_i_base);
    focp_target_set_signal(FOCP_S_IC, -(iab_pu[0] + iab_pu[1]) * s_i_base);
    focp_target_set_signal(FOCP_S_ID, idq_pu[0] * s_i_base);
    focp_target_set_signal(FOCP_S_IQ, idq_pu[1] * s_i_base);
    focp_target_set_signal(FOCP_S_UD, vdq_pu[0] * s_v_base);
    focp_target_set_signal(FOCP_S_UQ, vdq_pu[1] * s_v_base);
    focp_target_set_signal(FOCP_S_THETA_EL, pos_pu * 360.0f);
    focp_target_sample();
}
