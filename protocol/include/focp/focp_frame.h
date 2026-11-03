/*
 * FOCP framing for byte-stream transports (UART, TCP).
 *
 * raw   = fn | node | len | data[len] | crc16_lo | crc16_hi
 * frame = COBS(raw) | 0x00
 *
 * CRC-16/CCITT-FALSE (poly 0x1021, init 0xFFFF) over fn..data.
 * COBS guarantees 0x00 only appears as delimiter -> trivial resynchronisation.
 */
#ifndef FOCP_FRAME_H
#define FOCP_FRAME_H

#include "focp.h"

#ifdef __cplusplus
extern "C" {
#endif

#define FOCP_RAW_MAX   (3u + FOCP_MAX_PAYLOAD + 2u)
#define FOCP_FRAME_MAX (FOCP_RAW_MAX + 2u)   /* COBS overhead (1) + delimiter (1) */

uint16_t focp_crc16(const focp_byte_t *data, uint16_t len);

/* Encode message into out (>= FOCP_FRAME_MAX). Returns frame length incl. delimiter. */
uint16_t focp_frame_encode(const focp_msg_t *m, focp_byte_t *out);

typedef struct {
    focp_byte_t buf[FOCP_FRAME_MAX];
    uint16_t    n;
    uint16_t    overflow;
    uint32_t    crc_errors;
    uint32_t    frame_errors;
} focp_frame_decoder_t;

void focp_frame_decoder_init(focp_frame_decoder_t *d);

/* Feed one received byte. Returns 1 and fills *m when a valid message is complete. */
int  focp_frame_decoder_feed(focp_frame_decoder_t *d, focp_byte_t byte, focp_msg_t *m);

#ifdef __cplusplus
}
#endif
#endif
