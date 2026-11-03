#include "focp/focp_dev.h"

static int send_msg(focp_dev_t *d, const focp_msg_t *m)
{
    if (d->cfg.send == 0) return -1;
    if (d->cfg.send(d->cfg.ctx, m) != 0) { ++d->tx_dropped; return -1; }
    return 0;
}

void focp_dev_load_defaults(focp_dev_t *d)
{
    uint16_t i;
    for (i = 0; i < FOCP_NUM_PARAMS; ++i) d->param[i] = focp_param_table[i].def;
}

void focp_dev_init(focp_dev_t *d, const focp_dev_cfg_t *cfg)
{
    uint16_t i;
    d->cfg = *cfg;
    focp_dev_load_defaults(d);
    for (i = 0; i < FOCP_TELEM_SLOTS; ++i) d->telem_slot[i] = 0;
    d->telem_div = 20;
    d->telem_cnt = 0;
    d->telem_run = 0;
    d->telem_seq = 0;
    d->hb_age_ms = 0;
    d->tx_dropped = 0;
    d->rx_count = 0;
    d->param[FOCP_P_SYS_FW_VERSION] = (float)((cfg->fw_major << 8) | (cfg->fw_minor & 0xFFu));
}

static int is_locked(const focp_dev_t *d)
{
    return (d->cfg.is_locked != 0) ? d->cfg.is_locked(d->cfg.ctx) : 0;
}

uint16_t focp_dev_write_param(focp_dev_t *d, uint16_t idx, float value, int from_host)
{
    const focp_param_info_t *p;
    if (idx >= FOCP_NUM_PARAMS) return FOCP_PS_UNKNOWN_ID;
    p = &focp_param_table[idx];
    if (from_host && (p->flags & FOCP_F_RO)) return FOCP_PS_READ_ONLY;
    if (from_host && (p->flags & FOCP_F_LOCK) && is_locked(d)) return FOCP_PS_LOCKED;
    if (!(value == value)) return FOCP_PS_OUT_OF_RANGE;               /* NaN */
    if (p->type != FOCP_T_F32) {                                      /* integer types */
        value = (value >= 0.0f) ? (float)(int32_t)(value + 0.5f) : (float)(int32_t)(value - 0.5f);
    }
    if (p->type == FOCP_T_BOOL && value != 0.0f) value = 1.0f;
    if (value < p->min || value > p->max) return FOCP_PS_OUT_OF_RANGE;
    d->param[idx] = value;
    if (from_host && d->cfg.on_param_changed) d->cfg.on_param_changed(d->cfg.ctx, idx);
    return FOCP_PS_OK;
}

static void handle_param(focp_dev_t *d, const focp_msg_t *m)
{
    focp_msg_t r;
    uint16_t op, id, st;
    int idx;
    if (m->len < 3u) return;
    op = (uint16_t)(m->data[0] & 0xFFu);
    id = focp_get_u16(&m->data[1]);

    if (op == FOCP_OP_READ_INDEX) {                 /* id field = table index (enumeration) */
        if (id >= FOCP_NUM_PARAMS) { focp_build_param_rsp(&r, d->cfg.node, FOCP_PS_UNKNOWN_ID, 0xFFFFu, 0.0f); }
        else { focp_build_param_rsp(&r, d->cfg.node, FOCP_PS_OK, focp_param_table[id].id, d->param[id]); }
        send_msg(d, &r);
        return;
    }

    idx = focp_param_find(id);
    if (idx < 0) {
        focp_build_param_rsp(&r, d->cfg.node, FOCP_PS_UNKNOWN_ID, id, 0.0f);
    } else if (op == FOCP_OP_READ) {
        focp_build_param_rsp(&r, d->cfg.node, FOCP_PS_OK, id, d->param[idx]);
    } else if (op == FOCP_OP_WRITE && m->len >= 7u) {
        st = focp_dev_write_param(d, (uint16_t)idx, focp_get_f32(&m->data[3]), 1);
        focp_build_param_rsp(&r, d->cfg.node, st, id, d->param[idx]);
    } else {
        focp_build_param_rsp(&r, d->cfg.node, FOCP_PS_BAD_REQUEST, id, 0.0f);
    }
    send_msg(d, &r);
}

static void send_info(focp_dev_t *d)
{
    focp_msg_t r;
    uint16_t i;
    r.fn = FOCP_FN_INFO;
    r.node = d->cfg.node;
    r.len = 7;
    for (i = 0; i < FOCP_MAX_PAYLOAD; ++i) r.data[i] = 0;
    r.data[0] = (focp_byte_t)FOCP_PROTOCOL_VERSION;
    r.data[1] = (focp_byte_t)(d->cfg.fw_major & 0xFFu);
    r.data[2] = (focp_byte_t)(d->cfg.fw_minor & 0xFFu);
    r.data[3] = (focp_byte_t)(d->cfg.board_id & 0xFFu);
    focp_put_u16(&r.data[4], (uint16_t)FOCP_NUM_PARAMS);
    r.data[6] = (focp_byte_t)FOCP_NUM_SIGNALS;
    send_msg(d, &r);
}

static void handle_cmd(focp_dev_t *d, const focp_msg_t *m)
{
    uint16_t cmd, slot, sig;
    int idx;
    if (m->len < 1u) return;
    cmd = (uint16_t)(m->data[0] & 0xFFu);
    switch (cmd) {
    case FOCP_CMD_TELEM_SLOT:
        if (m->len < 3u) return;
        slot = (uint16_t)(m->data[1] & 0xFFu);
        sig  = (uint16_t)(m->data[2] & 0xFFu);
        if (slot >= FOCP_TELEM_SLOTS) return;
        idx = focp_signal_find(sig);
        d->telem_slot[slot] = (idx < 0) ? 0u : (uint16_t)(idx + 1);
        break;
    case FOCP_CMD_TELEM_RATE:
        if (m->len < 3u) return;
        d->telem_div = focp_get_u16(&m->data[1]);
        if (d->telem_div == 0u) d->telem_div = 1;
        d->telem_cnt = 0;
        break;
    case FOCP_CMD_TELEM_RUN:
        if (m->len < 2u) return;
        d->telem_run = (uint16_t)((m->data[1] & 0xFFu) != 0u);
        d->telem_cnt = 0;
        break;
    case FOCP_CMD_GET_INFO:
        send_info(d);
        break;
    case FOCP_CMD_HEARTBEAT:
        break;                                  /* any rx resets the timer */
    case FOCP_CMD_LOAD_DEFAULTS:
        if (!is_locked(d)) {
            uint16_t keep = (uint16_t)d->param[FOCP_P_SYS_FW_VERSION];
            focp_dev_load_defaults(d);
            d->param[FOCP_P_SYS_FW_VERSION] = (float)keep;
            if (d->cfg.on_param_changed) d->cfg.on_param_changed(d->cfg.ctx, 0xFFFFu);
        }
        break;
    default:
        if (d->cfg.on_command) d->cfg.on_command(d->cfg.ctx, cmd, &m->data[1], (uint16_t)(m->len - 1u));
        break;
    }
}

void focp_dev_rx(focp_dev_t *d, const focp_msg_t *m)
{
    if ((m->node & 0x0Fu) != (d->cfg.node & 0x0Fu)) return;
    ++d->rx_count;
    if (m->fn == FOCP_FN_CMD) { d->hb_age_ms = 0; handle_cmd(d, m); }
    else if (m->fn == FOCP_FN_PARAM_REQ) { d->hb_age_ms = 0; handle_param(d, m); }
}

void focp_dev_sample(focp_dev_t *d, const float *signals)
{
    focp_msg_t m;
    uint16_t s, first, k, i, nslots = 0;
    uint16_t sig_idx[FOCP_TELEM_SLOTS];

    if (!d->telem_run) return;
    if (++d->telem_cnt < d->telem_div) return;
    d->telem_cnt = 0;

    /* last used slot defines the frame count; empty slots in between are sent as 0 */
    for (s = 0; s < FOCP_TELEM_SLOTS; ++s) {
        sig_idx[s] = d->telem_slot[s];
        if (sig_idx[s] != 0u) nslots = (uint16_t)(s + 1u);
    }
    if (nslots == 0u) return;

    for (first = 0; first < nslots; first = (uint16_t)(first + FOCP_TELEM_PER_FRAME)) {
        m.fn = FOCP_FN_TELEM;
        m.node = d->cfg.node;
        m.len = 8;
        m.data[0] = (focp_byte_t)(d->telem_seq & 0xFFu);
        m.data[1] = (focp_byte_t)first;
        for (k = 0; k < FOCP_TELEM_PER_FRAME; ++k) {
            i = (uint16_t)(first + k);
            if (i < nslots && sig_idx[i] != 0u) {
                const focp_signal_info_t *si = &focp_signal_table[sig_idx[i] - 1u];
                focp_put_i16(&m.data[2u + 2u * k], focp_scale_to_i16(signals[sig_idx[i] - 1u], si->scale));
            } else {
                focp_put_i16(&m.data[2u + 2u * k], 0);
            }
        }
        send_msg(d, &m);
    }
    d->telem_seq = (uint16_t)((d->telem_seq + 1u) & 0xFFu);
}

int focp_dev_tick_ms(focp_dev_t *d, uint16_t ms)
{
    uint32_t to = (uint32_t)d->param[FOCP_P_COMM_TIMEOUT];
    if (d->hb_age_ms < 0xFFFF0000UL) d->hb_age_ms += ms;
    return (to != 0u && d->hb_age_ms > to) ? 1 : 0;
}

void focp_dev_send_status(focp_dev_t *d, uint16_t state, uint16_t mode, uint16_t sensor,
                          uint16_t faults, float udc, float temp)
{
    focp_msg_t m;
    focp_build_status(&m, d->cfg.node, state, mode, sensor, faults, udc, temp);
    send_msg(d, &m);
}

void focp_dev_send_event(focp_dev_t *d, uint16_t ev, uint16_t code, uint32_t t_ms)
{
    focp_msg_t m;
    focp_build_event(&m, d->cfg.node, ev, code, t_ms);
    send_msg(d, &m);
}
