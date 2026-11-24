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
    /* Bus-off recovery (2026-11-20): when the board is powered from USB before PVDD, the CAN
     * transceiver is not yet supplied, every frame fails (LEC = bit-0 error, TEC -> 255) and the
     * module stays bus-off with Init = 1, because the blockset does not enable Auto-Bus-On.
     * Auto-Bus-On restarts the module 10 ms after each bus-off; a bus-off that happened before
     * this point is ended here once. */
    CAN_setAutoBusOnTime(CAN_BASE, 2000000UL);        /* 10 ms at 200 MHz */
    CAN_enableAutoBusOn(CAN_BASE);
    if (CAN_getStatus(CAN_BASE) & CAN_STATUS_BUS_OFF) CAN_startModule(CAN_BASE);
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

/* Overcurrent trip on the three ePWMs of the power stage (6, 7, 8). The one-shot trip is set by
 * CMPSS3 (I_B, via ePWM X-BAR TRIP4 -> DCAEVT1, configured in the model) or by
 * focp_c28x_isr (software, I_A/I_B/I_C). Trip action A low / B high (DRV8300DI: INL inverted). */
static const uint32_t s_epwm[3] = {EPWM6_BASE, EPWM7_BASE, EPWM8_BASE};

static void hal_trip_force(void)
{
    uint16_t k;
    for (k = 0; k < 3u; ++k) EPWM_forceTripZoneEvent(s_epwm[k], EPWM_TZ_FORCE_EVENT_OST);
}

static uint16_t hal_trip_active(void)
{
    return (uint16_t)((EPWM_getTripZoneFlagStatus(EPWM6_BASE) & EPWM_TZ_FLAG_OST) != 0u);
}

static uint16_t hal_cmpss_active(void)
{
    return (uint16_t)((CMPSS_getStatus(CMPSS3_BASE) & (CMPSS_STS_HI_FILTOUT | CMPSS_STS_LO_FILTOUT)) != 0u);
}

static void hal_trip_clear(void)
{
    uint16_t k;
    for (k = 0; k < 3u; ++k) {
        EPWM_clearOneShotTripZoneFlag(s_epwm[k], EPWM_TZ_OST_FLAG_DCAEVT1);
        EPWM_clearTripZoneFlag(s_epwm[k], EPWM_TZ_INTERRUPT | EPWM_TZ_FLAG_OST | EPWM_TZ_FLAG_DCAEVT1);
    }
}
#else
/* Host build (tests): no CAN hardware, frames go through focp_target_rx_can/tx_pop directly.
 * The trip is simulated by a flag; focp_c28x_host_cmpss() emulates the comparator. */
static uint16_t s_host_trip, s_host_cmpss;
static void hal_init(void) {}
static void hal_rx(void) {}
static void hal_tx(void) {}
static void hal_trip_force(void) { s_host_trip = 1; }
static uint16_t hal_trip_active(void) { return (uint16_t)(s_host_trip || s_host_cmpss); }
static uint16_t hal_cmpss_active(void) { return s_host_cmpss; }
static void hal_trip_clear(void) { s_host_trip = 0; }
void focp_c28x_host_cmpss(uint16_t on) { s_host_cmpss = on; if (on) s_host_trip = 1; }
#endif

#define STEPS_PER_MS     2u       /* Ts_speed = 0.5 ms */
#define STATUS_PERIOD_MS 100u
#define TELEM_BASE_HZ    20000.0f /* focp_c28x_isr runs in the current ISR (PWM frequency) */
#define N_TUNE           8u
#define TRIP_ARM_MS      500u     /* software overcurrent armed after the ADC offset calibration */

static uint16_t s_init, s_active, s_enabled, s_comm_fault, s_div_ms, s_ms_status;
static uint32_t s_rx_seen;
static float s_i_base = 1.0f, s_v_base = 1.0f, s_n_base = 1.0f;
static uint16_t s_arm_ms;                       /* ms since start, up to TRIP_ARM_MS */
enum { TRIP_HW = 0, TRIP_SW_OC = 1, TRIP_SUM = 2 };
#define SUM_SAMPLES      20u      /* consecutive ISR samples (1 ms) with |ia+ib+ic| > i_sum_max */
#define IC_CAL_MAX_A     0.5f     /* I_C offset is averaged only while |ia|, |ib| are below this */
static volatile uint16_t s_trip_reason;         /* cause of the active trip, TRIP_HW unless set by the ISR */
static volatile float s_i_peak;                 /* largest |phase current| of the last ISR sample (A) */
static volatile float s_i_sum;                  /* |ia + ib + ic| of the last ISR sample (A) */
static float s_i_sum_max;                       /* plausibility threshold (A), 0 = off */
static uint32_t s_ic_acc, s_ic_n;               /* I_C offset averaging (ADC counts) */
static float s_ic_offset;
static uint16_t s_ic_valid, s_sum_cnt;

/* Tunable controller parameters in PU. On the target they are ExportedGlobal parameters of the
 * model (matlab/tools/foc_focp_tunable.m); on the host they live here for the tests. */
#if !defined(__TMS320C28XX__)
float focp_tune_cur_Kp, focp_tune_cur_Ki, focp_tune_spd_Kp, focp_tune_spd_Ki, focp_tune_iq_max, focp_tune_iq_min;
float focp_tune_udc_max;
#endif
/* Software overcurrent threshold (A): owned by this file, default from the model (ctrl_limits.I_trip)
 * on the first step, changed by FOC Studio via inv.I_max. */
float focp_tune_i_trip;
/* Number of start-up trips cleared when the protection was armed (diagnosis via JTAG/host tests). */
volatile uint16_t focp_c28x_boot_trips;
/* Trip diagnosis (JTAG, 2026-11-20): [0] reason of the first ISR trip (1 SW_OC, 2 SUM), [1] its time in ms,
 * [2..4] ia, ib, ic in A, [5] I_C offset (counts), [6] FOCP active, [7] ms of the first latched trip seen
 * by the speed task, [8] ms of the start-up clear, [9] largest |i| before arming (A). */
volatile float focp_c28x_trip_diag[10];
static uint32_t s_ms;

/* Values compiled into the firmware. LOAD_DEFAULTS restores these and not the table defaults,
 * whose spd.Iq_min = -5 A would brake into a lab supply. */
static float s_def[N_TUNE];
static const uint16_t s_tune_param[N_TUNE] = {FOCP_P_CUR_KP, FOCP_P_CUR_KI, FOCP_P_SPD_KP,
                                              FOCP_P_SPD_KI, FOCP_P_SPD_IQ_MAX, FOCP_P_SPD_IQ_MIN,
                                              FOCP_P_INV_UDC_MAX, FOCP_P_INV_I_MAX};

static float *tune_var(uint16_t k)
{
    switch (k) {
    case 0: return &focp_tune_cur_Kp;
    case 1: return &focp_tune_cur_Ki;
    case 2: return &focp_tune_spd_Kp;
    case 3: return &focp_tune_spd_Ki;
    case 4: return &focp_tune_iq_max;
    case 5: return &focp_tune_iq_min;
    case 6: return &focp_tune_udc_max;
    default: return &focp_tune_i_trip;
    }
}

/* SI value = model value * factor: current loop V/A, speed loop A/rpm, current limits A,
 * Udc_max (V) and the software overcurrent threshold (A) are already in SI */
static float tune_factor(uint16_t k)
{
    if (k < 2u) return s_v_base / s_i_base;
    if (k < 4u) return s_i_base / s_n_base;
    if (k < 6u) return s_i_base;
    return 1.0f;
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

void focp_c28x_step(uint16_t undervoltage, uint16_t overvoltage, uint16_t freemode_can, uint16_t hot_temp,
                    uint16_t safe_state, uint16_t speed_mode, float speed_pu, float n_base,
                    float udc, const float *idq_ref_pu, float i_base, float v_base,
                    float temp, uint16_t hall_state, float i_trip, float i_sum_max, uint16_t *active,
                    uint16_t *freemode, float *speed_ref, uint16_t *mode_out)
{
    float arg, span, ref;
    uint16_t mode;
    uint16_t cmd, faults, fm, k, tripped;

    s_i_base = i_base; s_v_base = v_base; s_n_base = n_base;
    s_i_sum_max = i_sum_max;
    if (!s_init) {
        focp_target_init(FOCP_C28X_NODE);
        focp_tune_i_trip = i_trip;
        focp_target_param_set(FOCP_P_COMM_TELEM_F, TELEM_BASE_HZ);
        for (k = 0; k < N_TUNE; ++k) s_def[k] = *tune_var(k);
        publish_tunables();                     /* FOC Studio shows what the firmware really uses */
        focp_target_set_param_callback(on_param);
        hal_init();
        s_active = s_enabled = s_comm_fault = 0;
        s_div_ms = s_ms_status = 0;
        s_rx_seen = 0;
        s_arm_ms = 0;
        s_trip_reason = TRIP_HW;
        focp_c28x_boot_trips = 0u;
        s_ms = 0u;
        for (k = 0; k < 10u; ++k) focp_c28x_trip_diag[k] = 0.0f;
        s_ic_acc = s_ic_n = 0u;
        s_ic_valid = s_sum_cnt = 0u;
        s_init = 1;                             /* last: enables focp_c28x_isr */
    }

    hal_rx();
    if (focp_target_rx_count() != s_rx_seen) {      /* first FOCP frame switches to FOCP */
        s_rx_seen = focp_target_rx_count();
        s_active = 1;
    }
    /* Overcurrent trip (latched by the ePWM one-shot trip): no enable until acknowledged.
     * ACK_FAULT clears it only when the comparator and the measured currents are below threshold. */
    tripped = hal_trip_active();
    if (tripped) s_enabled = 0;
    while ((cmd = focp_target_cmd_take(&arg)) != 0u) {
        if (cmd == FOCP_CMD_ENABLE && !s_comm_fault && !tripped) s_enabled = 1;
        else if (cmd == FOCP_CMD_DISABLE) s_enabled = 0;
        else if (cmd == FOCP_CMD_ACK_FAULT) {
            s_comm_fault = 0;
            if (tripped && !hal_cmpss_active() && s_i_peak < focp_tune_i_trip && s_sum_cnt == 0u) {
                hal_trip_clear();
                s_trip_reason = TRIP_HW;
                tripped = 0;
            }
        }
    }

    if (++s_div_ms >= STEPS_PER_MS) {
        s_div_ms = 0;
        ++s_ms;
        if (tripped && focp_c28x_trip_diag[7] == 0.0f) focp_c28x_trip_diag[7] = (float)s_ms;
        if (s_arm_ms < TRIP_ARM_MS && ++s_arm_ms == TRIP_ARM_MS) {
            /* Start-up trip (seen after every power-up, 2026-11-20): CMPSS3 fires while ISENB
             * settles and the one-shot latch keeps the drive locked, also in potentiometer mode.
             * Clear it once when the protection is armed, under the same conditions as ACK_FAULT. */
            if (tripped && s_trip_reason == TRIP_HW && !hal_cmpss_active() && s_i_peak < focp_tune_i_trip) {
                hal_trip_clear();
                tripped = 0;
                ++focp_c28x_boot_trips;
                focp_c28x_trip_diag[8] = (float)s_ms;
            }
        }
        if (focp_target_tick_1ms() && s_active && s_enabled) { s_comm_fault = 1; s_enabled = 0; }
        if (++s_ms_status >= STATUS_PERIOD_MS) {
            s_ms_status = 0;
            faults = 0;
            if (undervoltage) faults |= FOCP_FAULT_UNDERVOLTAGE;
            if (overvoltage) faults |= FOCP_FAULT_OVERVOLTAGE;
            if (hot_temp) faults |= FOCP_FAULT_OVERTEMP;
            if (!safe_state) faults |= FOCP_FAULT_SAFE_INPUT;
            if (s_comm_fault) faults |= FOCP_FAULT_COMM;
            if (tripped)
                faults |= s_trip_reason == TRIP_SUM ? FOCP_FAULT_CURRENT_SUM
                        : (s_trip_reason == TRIP_SW_OC ? FOCP_FAULT_OC_SW : FOCP_FAULT_OC_HW);
            fm = (uint16_t)(undervoltage || overvoltage || freemode_can || hot_temp || !safe_state || (s_active && !s_enabled));
            focp_target_status((s_comm_fault || tripped) ? FOCP_ST_FAULT : (fm ? FOCP_ST_READY : FOCP_ST_RUN),
                               FOCP_MODE_SPEED, FOCP_SENS_HALL, faults, udc, temp);
        }
    }

    /* Speed calculator in the model: N_ref = Speed_Ref * floor(N_base/3) * speedMode.
     * Potentiometer mode keeps speedMode from CAN 0x102. In FOCP mode the firmware chooses the
     * smallest mode whose range covers the reference (2026-11-24: with speedMode 1 from 0x102 a
     * reference of 2000 rpm was silently limited to 1333 rpm); the model takes it from *mode_out. */
    span = (float)((uint32_t)(n_base / 3.0f));
    mode = speed_mode ? speed_mode : 1u;
    if (s_active) {
        arg = focp_target_reference();
        mode = 1u;
        while (mode < FOCP_C28X_SPEED_MODES && arg > span * (float)mode) ++mode;
    }
    span *= (float)mode;
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
    /* Freemode also while a trip is latched and until the protection is armed: Freemode resets the
     * speed and current controllers in the model. Without it they wind up while the bridge is tripped
     * (potentiometer mode) and the start-up clear produced a current step of 27 A (2026-11-20,
     * matlab/reports/09_Einschalt_Trip_2026-11-20.md). */
    *freemode = (uint16_t)((s_active && !s_enabled) || tripped || s_arm_ms < TRIP_ARM_MS);
    *speed_ref = ref;
    *mode_out = mode;
}

void focp_c28x_isr(const float *iab_pu, float pos_pu, const float *idq_pu, const float *vdq_pu,
                   uint16_t ic_raw)
{
    float ia, ib, ic, peak, sum;
    const uint16_t armed = (uint16_t)(s_arm_ms >= TRIP_ARM_MS);
    if (!s_init) return;                        /* bases and protocol are set up by the speed task */
    ia = iab_pu[0] * s_i_base;
    ib = iab_pu[1] * s_i_base;
    /* I_C (ADCINB3): same scaling as the model uses for I_A/I_B, (ADC - offset) / 2048 PU.
     * The offset is averaged while the protection is not yet armed and the currents are small. */
    if (!armed) {
        if ((ia < 0.0f ? -ia : ia) < IC_CAL_MAX_A && (ib < 0.0f ? -ib : ib) < IC_CAL_MAX_A) {
            s_ic_acc += ic_raw;
            ++s_ic_n;
        }
        ic = -(ia + ib);
    } else {
        if (s_ic_n != 0u) { s_ic_offset = (float)s_ic_acc / (float)s_ic_n; s_ic_n = 0u; s_ic_valid = 1u; }
        ic = s_ic_valid ? ((float)ic_raw - s_ic_offset) / 2048.0f * s_i_base : -(ia + ib);
    }
    peak = ia < 0.0f ? -ia : ia;
    if ((ib < 0.0f ? -ib : ib) > peak) peak = ib < 0.0f ? -ib : ib;
    if ((ic < 0.0f ? -ic : ic) > peak) peak = ic < 0.0f ? -ic : ic;
    s_i_peak = peak;
    if (!armed && peak > focp_c28x_trip_diag[9]) focp_c28x_trip_diag[9] = peak;
    /* plausibility ia + ib + ic = 0 (only with a measured ic) */
    sum = ia + ib + ic;
    if (sum < 0.0f) sum = -sum;
    if (s_ic_valid && s_i_sum_max > 0.0f && sum > s_i_sum_max) {
        if (s_sum_cnt < SUM_SAMPLES) ++s_sum_cnt;
    } else {
        s_sum_cnt = 0u;
    }
    s_i_sum = sum;
    /* software overcurrent (I_A and I_C have no comparator, HW16) and implausible current sum;
     * same latch as the CMPSS trip */
    if (armed && !hal_trip_active()) {
        if (peak > focp_tune_i_trip) { hal_trip_force(); s_trip_reason = TRIP_SW_OC; }
        else if (s_sum_cnt >= SUM_SAMPLES) { hal_trip_force(); s_trip_reason = TRIP_SUM; }
        if (hal_trip_active() && focp_c28x_trip_diag[0] == 0.0f) {
            focp_c28x_trip_diag[0] = (float)s_trip_reason;
            focp_c28x_trip_diag[1] = (float)s_ms;
            focp_c28x_trip_diag[2] = ia; focp_c28x_trip_diag[3] = ib; focp_c28x_trip_diag[4] = ic;
            focp_c28x_trip_diag[5] = s_ic_offset;
            focp_c28x_trip_diag[6] = (float)s_active;
        }
    }
    focp_target_set_signal(FOCP_S_IA, ia);
    focp_target_set_signal(FOCP_S_IB, ib);
    focp_target_set_signal(FOCP_S_IC, ic);
    focp_target_set_signal(FOCP_S_ID, idq_pu[0] * s_i_base);
    focp_target_set_signal(FOCP_S_IQ, idq_pu[1] * s_i_base);
    focp_target_set_signal(FOCP_S_UD, vdq_pu[0] * s_v_base);
    focp_target_set_signal(FOCP_S_UQ, vdq_pu[1] * s_v_base);
    focp_target_set_signal(FOCP_S_THETA_EL, pos_pu * 360.0f);
    focp_target_sample();
}
