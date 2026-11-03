/*
 * FOCP - FOC drive protocol
 *
 * Shared between firmware (TI C28x, Embedded Coder) and host tools (C++).
 * Pure C99, no dynamic memory, no dependencies.
 *
 * Portability note (C28x): the smallest addressable unit on C28x is 16 bit
 * (CHAR_BIT == 16, no uint8_t). Every "byte" in this library is therefore stored
 * in a focp_byte_t (uint_least8_t) and only the lower 8 bits are used. Never
 * memcpy structs onto the wire - always use the pack/unpack helpers.
 *
 * Every message carries at most 8 payload bytes so it maps 1:1 onto a classic
 * CAN frame. Stream transports (UART, TCP) wrap the same message with COBS+CRC
 * (see focp_frame.h).
 */
#ifndef FOCP_H
#define FOCP_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FOCP_PROTOCOL_VERSION 1u
#define FOCP_MAX_PAYLOAD      8u
#define FOCP_TELEM_SLOTS      8u   /* max. simultaneously streamed signals */
#define FOCP_TELEM_PER_FRAME  3u   /* int16 values per TELEM frame */

typedef uint_least8_t focp_byte_t;

/* ---------------------------------------------------------------------------------------
 * Message / function codes. On CAN: 11-bit ID = (fn << 4) | node  (node 0..15).
 * Base 0x20 keeps clear of the legacy CarPediem IDs 0x101/0x102/0x110/0x111.
 * Lower fn = higher CAN priority.
 * ------------------------------------------------------------------------------------ */
typedef enum {
    FOCP_FN_CMD       = 0x20, /* host -> drive */
    FOCP_FN_PARAM_REQ = 0x21, /* host -> drive */
    FOCP_FN_PARAM_RSP = 0x22, /* drive -> host */
    FOCP_FN_STATUS    = 0x23, /* drive -> host, cyclic */
    FOCP_FN_TELEM     = 0x24, /* drive -> host, streamed */
    FOCP_FN_EVENT     = 0x25, /* drive -> host, on change */
    FOCP_FN_INFO      = 0x26  /* drive -> host, on request */
} focp_fn_t;

typedef struct {
    uint16_t    fn;     /* focp_fn_t */
    uint16_t    node;   /* 0..15 */
    uint16_t    len;    /* 0..8 */
    focp_byte_t data[FOCP_MAX_PAYLOAD];
} focp_msg_t;

/* CAN identifier helpers */
#define FOCP_CAN_ID(fn, node)  ((uint16_t)((((uint16_t)(fn)) << 4) | ((uint16_t)(node) & 0x0Fu)))
#define FOCP_CAN_FN(id)        ((uint16_t)(((id) >> 4) & 0x7Fu))
#define FOCP_CAN_NODE(id)      ((uint16_t)((id) & 0x0Fu))

/* ---------------------------------------------------------------------------------------
 * Commands (FOCP_FN_CMD, data[0] = command)
 * ------------------------------------------------------------------------------------ */
typedef enum {
    FOCP_CMD_ENABLE        = 0x01, /* -                                 */
    FOCP_CMD_DISABLE       = 0x02, /* -                                 */
    FOCP_CMD_ACK_FAULT     = 0x03, /* -                                 */
    FOCP_CMD_SET_REF       = 0x05, /* f32 ref (unit depends on ctrl.mode: A, rpm, deg, rpm) */
    FOCP_CMD_CALIBRATE     = 0x06, /* u8 focp_calib_t                   */
    FOCP_CMD_TELEM_SLOT    = 0x07, /* u8 slot, u8 signal id (0 = empty) */
    FOCP_CMD_TELEM_RATE    = 0x08, /* u16 divider (1 = every control cycle) */
    FOCP_CMD_TELEM_RUN     = 0x09, /* u8 0/1                            */
    FOCP_CMD_SAVE_PARAMS   = 0x0A, /* -  store parameters in non-volatile memory */
    FOCP_CMD_GET_INFO      = 0x0B, /* -  drive answers with FOCP_FN_INFO */
    FOCP_CMD_HEARTBEAT     = 0x0C, /* -  host alive (see comm.timeout_ms) */
    FOCP_CMD_LOAD_DEFAULTS = 0x0D  /* -  reset all parameters to defaults (not running) */
} focp_cmd_t;

typedef enum {
    FOCP_CALIB_CURRENT_OFFSET = 1,
    FOCP_CALIB_HALL           = 2,
    FOCP_CALIB_MOTOR_ID       = 3
} focp_calib_t;

/* ---------------------------------------------------------------------------------------
 * Parameter service
 *   PARAM_REQ: [0]=op [1..2]=id [3..6]=f32 value (write only)
 *   PARAM_RSP: [0]=status [1..2]=id [3..6]=f32 value (current value)
 * All values travel as IEEE754 float32 regardless of the storage type.
 * ------------------------------------------------------------------------------------ */
typedef enum { FOCP_OP_READ = 1, FOCP_OP_WRITE = 2, FOCP_OP_READ_INDEX = 3 } focp_param_op_t;

typedef enum {
    FOCP_PS_OK          = 0,
    FOCP_PS_UNKNOWN_ID  = 1,
    FOCP_PS_READ_ONLY   = 2,
    FOCP_PS_OUT_OF_RANGE= 3,
    FOCP_PS_LOCKED      = 4,  /* FOCP_F_LOCK parameter while running */
    FOCP_PS_BAD_REQUEST = 5
} focp_param_status_t;

typedef enum { FOCP_T_F32 = 0, FOCP_T_U16 = 1, FOCP_T_BOOL = 2, FOCP_T_ENUM = 3 } focp_type_t;

#define FOCP_F_RO   0x01u
#define FOCP_F_LOCK 0x02u
#define FOCP_F_SIM  0x04u

/* Parameter index enum: FOCP_P_<IDX> */
typedef enum {
#define X(IDX, ID, NAME, TYPE, UNIT, MIN, MAX, DEF, FLAGS, DESC) FOCP_P_##IDX,
#include "focp_params.def"
#undef X
    FOCP_NUM_PARAMS
} focp_param_idx_t;

typedef struct {
    uint16_t    id;
    const char *name;
    uint16_t    type;   /* focp_type_t */
    const char *unit;
    float       min;
    float       max;
    float       def;
    uint16_t    flags;
    const char *desc;
} focp_param_info_t;

extern const focp_param_info_t focp_param_table[FOCP_NUM_PARAMS];
int  focp_param_find(uint16_t id);   /* returns index or -1 */

/* ---------------------------------------------------------------------------------------
 * Telemetry signals
 *   TELEM: [0]=sample seq (u8) [1]=first slot [2..7]=3 x int16 values
 *   One sample of n configured slots is sent as ceil(n/3) frames with the same seq.
 * ------------------------------------------------------------------------------------ */
typedef enum {
#define X(IDX, ID, NAME, UNIT, SCALE, DESC) FOCP_S_##IDX,
#include "focp_signals.def"
#undef X
    FOCP_NUM_SIGNALS
} focp_signal_idx_t;

typedef struct {
    uint16_t    id;
    const char *name;
    const char *unit;
    float       scale;
    const char *desc;
} focp_signal_info_t;

extern const focp_signal_info_t focp_signal_table[FOCP_NUM_SIGNALS];
int focp_signal_find(uint16_t id);  /* returns index or -1 */

/* ---------------------------------------------------------------------------------------
 * Drive state machine and faults (STATUS / EVENT)
 *   STATUS: [0]=state [1]=mode|sensor<<4 [2..3]=faults u16 [4..5]=Udc*100 u16 [6..7]=T*100 i16
 *   EVENT : [0]=event [1..2]=code u16 [3..6]=timestamp ms u32
 *   INFO  : [0]=protocol ver [1]=fw major [2]=fw minor [3]=board [4..5]=num params [6]=num signals
 * ------------------------------------------------------------------------------------ */
typedef enum {
    FOCP_ST_INIT       = 0,
    FOCP_ST_OFFSET_CAL = 1,
    FOCP_ST_READY      = 2,
    FOCP_ST_RUN        = 3,
    FOCP_ST_FAULT      = 4,
    FOCP_ST_CALIB      = 5
} focp_state_t;

typedef enum {
    FOCP_MODE_OFF      = 0,
    FOCP_MODE_TORQUE   = 1,
    FOCP_MODE_SPEED    = 2,
    FOCP_MODE_POSITION = 3,
    FOCP_MODE_OPENLOOP = 4
} focp_mode_t;

typedef enum { FOCP_SENS_HALL = 0, FOCP_SENS_SENSORLESS = 1, FOCP_SENS_HYBRID = 2 } focp_sensor_t;

#define FOCP_FAULT_OC_HW        0x0001u /* hardware trip (CMPSS/TZ)        */
#define FOCP_FAULT_OC_SW        0x0002u /* |i| > inv.I_max                  */
#define FOCP_FAULT_UNDERVOLTAGE 0x0004u
#define FOCP_FAULT_OVERVOLTAGE  0x0008u
#define FOCP_FAULT_OVERTEMP     0x0010u
#define FOCP_FAULT_HALL         0x0020u /* invalid hall state / sequence    */
#define FOCP_FAULT_OBSERVER     0x0040u /* observer lost lock               */
#define FOCP_FAULT_COMM         0x0080u /* heartbeat timeout                */
#define FOCP_FAULT_SAFE_INPUT   0x0100u /* safe-state input (button)        */
#define FOCP_FAULT_PARAM        0x0200u /* inconsistent parameters          */
#define FOCP_FAULT_OVERSPEED    0x0400u

typedef enum {
    FOCP_EV_FAULT_SET    = 1, /* code = fault bit(s)   */
    FOCP_EV_FAULT_CLEAR  = 2, /* code = fault bit(s)   */
    FOCP_EV_STATE        = 3, /* code = new state      */
    FOCP_EV_CALIB_DONE   = 4, /* code = focp_calib_t   */
    FOCP_EV_CALIB_FAILED = 5, /* code = focp_calib_t   */
    FOCP_EV_PARAMS_SAVED = 6
} focp_event_t;

const char *focp_state_name(uint16_t state);
const char *focp_mode_name(uint16_t mode);
const char *focp_fault_name(uint16_t fault_bit);

/* ---------------------------------------------------------------------------------------
 * Byte helpers (little endian, C28x safe)
 * ------------------------------------------------------------------------------------ */
void     focp_put_u16(focp_byte_t *p, uint16_t v);
void     focp_put_i16(focp_byte_t *p, int16_t v);
void     focp_put_u32(focp_byte_t *p, uint32_t v);
void     focp_put_f32(focp_byte_t *p, float v);
uint16_t focp_get_u16(const focp_byte_t *p);
int16_t  focp_get_i16(const focp_byte_t *p);
uint32_t focp_get_u32(const focp_byte_t *p);
float    focp_get_f32(const focp_byte_t *p);

/* Scale a physical value to the int16 wire representation (saturating). */
int16_t  focp_scale_to_i16(float value, float scale);

/* ---------------------------------------------------------------------------------------
 * Message builders. All return the message ready to send.
 * ------------------------------------------------------------------------------------ */
void focp_build_cmd(focp_msg_t *m, uint16_t node, uint16_t cmd);
void focp_build_cmd_u8(focp_msg_t *m, uint16_t node, uint16_t cmd, uint16_t a);
void focp_build_cmd_u8u8(focp_msg_t *m, uint16_t node, uint16_t cmd, uint16_t a, uint16_t b);
void focp_build_cmd_u16(focp_msg_t *m, uint16_t node, uint16_t cmd, uint16_t a);
void focp_build_cmd_f32(focp_msg_t *m, uint16_t node, uint16_t cmd, float a);
void focp_build_param_req(focp_msg_t *m, uint16_t node, uint16_t op, uint16_t id, float value);
void focp_build_param_rsp(focp_msg_t *m, uint16_t node, uint16_t status, uint16_t id, float value);
void focp_build_status(focp_msg_t *m, uint16_t node, uint16_t state, uint16_t mode, uint16_t sensor,
                       uint16_t faults, float udc, float temp);
void focp_build_event(focp_msg_t *m, uint16_t node, uint16_t ev, uint16_t code, uint32_t t_ms);

#ifdef __cplusplus
}
#endif
#endif /* FOCP_H */
