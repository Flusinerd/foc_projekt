#include "focp/focp.h"

/* ------------------------------------------------------------------------------------- */
/* Tables                                                                                */
/* ------------------------------------------------------------------------------------- */
const focp_param_info_t focp_param_table[FOCP_NUM_PARAMS] = {
#define X(IDX, ID, NAME, TYPE, UNIT, MIN, MAX, DEF, FLAGS, DESC) \
    { (uint16_t)(ID), NAME, (uint16_t)(TYPE), UNIT, (float)(MIN), (float)(MAX), (float)(DEF), (uint16_t)(FLAGS), DESC },
#include "focp/focp_params.def"
#undef X
};

const focp_signal_info_t focp_signal_table[FOCP_NUM_SIGNALS] = {
#define X(IDX, ID, NAME, UNIT, SCALE, DESC) { (uint16_t)(ID), NAME, UNIT, (float)(SCALE), DESC },
#include "focp/focp_signals.def"
#undef X
};

int focp_param_find(uint16_t id)
{
    int i;
    for (i = 0; i < (int)FOCP_NUM_PARAMS; ++i) {
        if (focp_param_table[i].id == id) return i;
    }
    return -1;
}

int focp_signal_find(uint16_t id)
{
    int i;
    if (id == 0u) return -1;
    for (i = 0; i < (int)FOCP_NUM_SIGNALS; ++i) {
        if (focp_signal_table[i].id == id) return i;
    }
    return -1;
}

const char *focp_state_name(uint16_t state)
{
    switch (state) {
    case FOCP_ST_INIT:       return "INIT";
    case FOCP_ST_OFFSET_CAL: return "OFFSET_CAL";
    case FOCP_ST_READY:      return "READY";
    case FOCP_ST_RUN:        return "RUN";
    case FOCP_ST_FAULT:      return "FAULT";
    case FOCP_ST_CALIB:      return "CALIB";
    default:                 return "?";
    }
}

const char *focp_mode_name(uint16_t mode)
{
    switch (mode) {
    case FOCP_MODE_OFF:      return "Aus";
    case FOCP_MODE_TORQUE:   return "Moment";
    case FOCP_MODE_SPEED:    return "Drehzahl";
    case FOCP_MODE_POSITION: return "Lage";
    case FOCP_MODE_OPENLOOP: return "Open-Loop I/f";
    default:                 return "?";
    }
}

const char *focp_fault_name(uint16_t fault_bit)
{
    switch (fault_bit) {
    case FOCP_FAULT_OC_HW:        return "Ueberstrom (HW)";
    case FOCP_FAULT_OC_SW:        return "Ueberstrom (SW)";
    case FOCP_FAULT_UNDERVOLTAGE: return "Unterspannung";
    case FOCP_FAULT_OVERVOLTAGE:  return "Ueberspannung";
    case FOCP_FAULT_OVERTEMP:     return "Uebertemperatur";
    case FOCP_FAULT_HALL:         return "Hall-Fehler";
    case FOCP_FAULT_OBSERVER:     return "Beobachter verloren";
    case FOCP_FAULT_COMM:         return "Kommunikations-Timeout";
    case FOCP_FAULT_SAFE_INPUT:   return "Sicherheitseingang";
    case FOCP_FAULT_PARAM:        return "Parameterfehler";
    case FOCP_FAULT_OVERSPEED:    return "Ueberdrehzahl";
    default:                      return "?";
    }
}

/* ------------------------------------------------------------------------------------- */
/* Byte helpers                                                                          */
/* ------------------------------------------------------------------------------------- */
void focp_put_u16(focp_byte_t *p, uint16_t v)
{
    p[0] = (focp_byte_t)(v & 0xFFu);
    p[1] = (focp_byte_t)((v >> 8) & 0xFFu);
}

void focp_put_i16(focp_byte_t *p, int16_t v) { focp_put_u16(p, (uint16_t)v); }

void focp_put_u32(focp_byte_t *p, uint32_t v)
{
    p[0] = (focp_byte_t)(v & 0xFFu);
    p[1] = (focp_byte_t)((v >> 8) & 0xFFu);
    p[2] = (focp_byte_t)((v >> 16) & 0xFFu);
    p[3] = (focp_byte_t)((v >> 24) & 0xFFu);
}

void focp_put_f32(focp_byte_t *p, float v)
{
    union { float f; uint32_t u; } c;   /* float is IEEE754 binary32 on C28x and hosts */
    c.f = v;
    focp_put_u32(p, c.u);
}

uint16_t focp_get_u16(const focp_byte_t *p)
{
    return (uint16_t)(((uint16_t)p[0] & 0xFFu) | (((uint16_t)p[1] & 0xFFu) << 8));
}

int16_t focp_get_i16(const focp_byte_t *p)
{
    uint16_t u = focp_get_u16(p);
    return (int16_t)u;
}

uint32_t focp_get_u32(const focp_byte_t *p)
{
    return ((uint32_t)p[0] & 0xFFu) | (((uint32_t)p[1] & 0xFFu) << 8) |
           (((uint32_t)p[2] & 0xFFu) << 16) | (((uint32_t)p[3] & 0xFFu) << 24);
}

float focp_get_f32(const focp_byte_t *p)
{
    union { float f; uint32_t u; } c;
    c.u = focp_get_u32(p);
    return c.f;
}

int16_t focp_scale_to_i16(float value, float scale)
{
    float r = value / scale;
    if (r >= 32767.0f) return 32767;
    if (r <= -32767.0f) return -32767;
    return (int16_t)(r >= 0.0f ? r + 0.5f : r - 0.5f);
}

/* ------------------------------------------------------------------------------------- */
/* Builders                                                                              */
/* ------------------------------------------------------------------------------------- */
static void msg_init(focp_msg_t *m, uint16_t fn, uint16_t node, uint16_t len)
{
    uint16_t i;
    m->fn = fn;
    m->node = (uint16_t)(node & 0x0Fu);
    m->len = len;
    for (i = 0; i < FOCP_MAX_PAYLOAD; ++i) m->data[i] = 0;
}

void focp_build_cmd(focp_msg_t *m, uint16_t node, uint16_t cmd)
{
    msg_init(m, FOCP_FN_CMD, node, 1);
    m->data[0] = (focp_byte_t)(cmd & 0xFFu);
}

void focp_build_cmd_u8(focp_msg_t *m, uint16_t node, uint16_t cmd, uint16_t a)
{
    msg_init(m, FOCP_FN_CMD, node, 2);
    m->data[0] = (focp_byte_t)(cmd & 0xFFu);
    m->data[1] = (focp_byte_t)(a & 0xFFu);
}

void focp_build_cmd_u8u8(focp_msg_t *m, uint16_t node, uint16_t cmd, uint16_t a, uint16_t b)
{
    msg_init(m, FOCP_FN_CMD, node, 3);
    m->data[0] = (focp_byte_t)(cmd & 0xFFu);
    m->data[1] = (focp_byte_t)(a & 0xFFu);
    m->data[2] = (focp_byte_t)(b & 0xFFu);
}

void focp_build_cmd_u16(focp_msg_t *m, uint16_t node, uint16_t cmd, uint16_t a)
{
    msg_init(m, FOCP_FN_CMD, node, 3);
    m->data[0] = (focp_byte_t)(cmd & 0xFFu);
    focp_put_u16(&m->data[1], a);
}

void focp_build_cmd_f32(focp_msg_t *m, uint16_t node, uint16_t cmd, float a)
{
    msg_init(m, FOCP_FN_CMD, node, 5);
    m->data[0] = (focp_byte_t)(cmd & 0xFFu);
    focp_put_f32(&m->data[1], a);
}

void focp_build_param_req(focp_msg_t *m, uint16_t node, uint16_t op, uint16_t id, float value)
{
    msg_init(m, FOCP_FN_PARAM_REQ, node, (op == FOCP_OP_WRITE) ? 7u : 3u);
    m->data[0] = (focp_byte_t)(op & 0xFFu);
    focp_put_u16(&m->data[1], id);
    if (op == FOCP_OP_WRITE) focp_put_f32(&m->data[3], value);
}

void focp_build_param_rsp(focp_msg_t *m, uint16_t node, uint16_t status, uint16_t id, float value)
{
    msg_init(m, FOCP_FN_PARAM_RSP, node, 7);
    m->data[0] = (focp_byte_t)(status & 0xFFu);
    focp_put_u16(&m->data[1], id);
    focp_put_f32(&m->data[3], value);
}

void focp_build_status(focp_msg_t *m, uint16_t node, uint16_t state, uint16_t mode, uint16_t sensor,
                       uint16_t faults, float udc, float temp)
{
    float u = udc * 100.0f;
    msg_init(m, FOCP_FN_STATUS, node, 8);
    m->data[0] = (focp_byte_t)(state & 0xFFu);
    m->data[1] = (focp_byte_t)((mode & 0x0Fu) | ((sensor & 0x0Fu) << 4));
    focp_put_u16(&m->data[2], faults);
    if (u < 0.0f) u = 0.0f;
    if (u > 65535.0f) u = 65535.0f;
    focp_put_u16(&m->data[4], (uint16_t)(u + 0.5f));
    focp_put_i16(&m->data[6], focp_scale_to_i16(temp, 0.01f));
}

void focp_build_event(focp_msg_t *m, uint16_t node, uint16_t ev, uint16_t code, uint32_t t_ms)
{
    msg_init(m, FOCP_FN_EVENT, node, 7);
    m->data[0] = (focp_byte_t)(ev & 0xFFu);
    focp_put_u16(&m->data[1], code);
    focp_put_u32(&m->data[3], t_ms);
}
