#include "focp/focp_frame.h"

uint16_t focp_crc16(const focp_byte_t *data, uint16_t len)
{
    uint16_t crc = 0xFFFFu;
    uint16_t i, b;
    for (i = 0; i < len; ++i) {
        crc ^= (uint16_t)(((uint16_t)data[i] & 0xFFu) << 8);
        for (b = 0; b < 8u; ++b) {
            if (crc & 0x8000u) crc = (uint16_t)((crc << 1) ^ 0x1021u);
            else               crc = (uint16_t)(crc << 1);
        }
    }
    return crc;
}

static uint16_t cobs_encode(const focp_byte_t *in, uint16_t len, focp_byte_t *out)
{
    uint16_t read = 0, write = 1, code_idx = 0;
    focp_byte_t code = 1;
    while (read < len) {
        if ((in[read] & 0xFFu) == 0u) {
            out[code_idx] = code;
            code = 1;
            code_idx = write++;
            ++read;
        } else {
            out[write++] = (focp_byte_t)(in[read++] & 0xFFu);
            ++code;
            if (code == 0xFFu) {           /* cannot happen for FOCP sizes, kept for safety */
                out[code_idx] = code;
                code = 1;
                code_idx = write++;
            }
        }
    }
    out[code_idx] = code;
    return write;
}

/* Returns decoded length, or 0xFFFF on error. */
static uint16_t cobs_decode(const focp_byte_t *in, uint16_t len, focp_byte_t *out)
{
    uint16_t read = 0, write = 0, i;
    while (read < len) {
        uint16_t code = (uint16_t)(in[read] & 0xFFu);
        if (code == 0u || (uint16_t)(read + code) > len) return 0xFFFFu;
        ++read;
        for (i = 1; i < code; ++i) {
            if (read >= len) return 0xFFFFu;
            out[write++] = (focp_byte_t)(in[read++] & 0xFFu);
        }
        if (code < 0xFFu && read < len) out[write++] = 0;
    }
    return write;
}

uint16_t focp_frame_encode(const focp_msg_t *m, focp_byte_t *out)
{
    focp_byte_t raw[FOCP_RAW_MAX];
    uint16_t n = 0, i, crc, len;
    len = (m->len > FOCP_MAX_PAYLOAD) ? FOCP_MAX_PAYLOAD : m->len;
    raw[n++] = (focp_byte_t)(m->fn & 0xFFu);
    raw[n++] = (focp_byte_t)(m->node & 0xFFu);
    raw[n++] = (focp_byte_t)(len & 0xFFu);
    for (i = 0; i < len; ++i) raw[n++] = (focp_byte_t)(m->data[i] & 0xFFu);
    crc = focp_crc16(raw, n);
    raw[n++] = (focp_byte_t)(crc & 0xFFu);
    raw[n++] = (focp_byte_t)((crc >> 8) & 0xFFu);
    n = cobs_encode(raw, n, out);
    out[n++] = 0;
    return n;
}

void focp_frame_decoder_init(focp_frame_decoder_t *d)
{
    d->n = 0;
    d->overflow = 0;
    d->crc_errors = 0;
    d->frame_errors = 0;
}

int focp_frame_decoder_feed(focp_frame_decoder_t *d, focp_byte_t byte, focp_msg_t *m)
{
    focp_byte_t raw[FOCP_FRAME_MAX];
    uint16_t n, i, len;
    byte = (focp_byte_t)(byte & 0xFFu);

    if (byte != 0u) {
        if (d->n < FOCP_FRAME_MAX) d->buf[d->n++] = byte;
        else d->overflow = 1;
        return 0;
    }

    /* delimiter: try to decode collected frame */
    if (d->n == 0u) return 0;
    if (d->overflow) { d->n = 0; d->overflow = 0; ++d->frame_errors; return 0; }

    n = cobs_decode(d->buf, d->n, raw);
    d->n = 0;
    if (n == 0xFFFFu || n < 5u) { ++d->frame_errors; return 0; }

    len = (uint16_t)(raw[2] & 0xFFu);
    if (len > FOCP_MAX_PAYLOAD || n != (uint16_t)(len + 5u)) { ++d->frame_errors; return 0; }
    if (focp_crc16(raw, (uint16_t)(n - 2u)) != focp_get_u16(&raw[n - 2u])) { ++d->crc_errors; return 0; }

    m->fn = (uint16_t)(raw[0] & 0xFFu);
    m->node = (uint16_t)(raw[1] & 0xFFu);
    m->len = len;
    for (i = 0; i < FOCP_MAX_PAYLOAD; ++i) m->data[i] = (i < len) ? raw[3u + i] : 0;
    return 1;
}
