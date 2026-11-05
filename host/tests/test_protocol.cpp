#include "check.hpp"

#include <focp/focp_dev.h>
#include <focp/focp_frame.h>
#include <fochost/Transport.hpp>
#include <focp_target.h>
#include <focp_c28x.h>

#include <cmath>
#include <random>
#include <set>
#include <vector>

static void testTables()
{
    std::puts("tables");
    std::set<uint16_t> ids;
    for (int i = 0; i < FOCP_NUM_PARAMS; ++i) {
        const auto& p = focp_param_table[i];
        CHECK(ids.insert(p.id).second);                     // unique IDs
        CHECK(p.def >= p.min && p.def <= p.max);            // default inside range
        CHECK(focp_param_find(p.id) == i);
    }
    std::set<uint16_t> sids;
    for (int i = 0; i < FOCP_NUM_SIGNALS; ++i) {
        CHECK(focp_signal_table[i].id != 0);
        CHECK(focp_signal_table[i].id < 256);
        CHECK(sids.insert(focp_signal_table[i].id).second);
        CHECK(focp_signal_table[i].scale > 0.0f);
    }
    CHECK(focp_param_find(0xBEEF) == -1);
}

static void testBytes()
{
    std::puts("byte helpers");
    focp_byte_t b[4];
    focp_put_u16(b, 0xA55A);
    CHECK(b[0] == 0x5A && b[1] == 0xA5);
    CHECK(focp_get_u16(b) == 0xA55A);
    focp_put_i16(b, -1234);
    CHECK(focp_get_i16(b) == -1234);
    focp_put_f32(b, 3.14159f);
    CHECK(focp_get_f32(b) == 3.14159f);
    focp_put_f32(b, -2.5e-7f);
    CHECK(focp_get_f32(b) == -2.5e-7f);
    CHECK(focp_scale_to_i16(1e9f, 0.001f) == 32767);
    CHECK(focp_scale_to_i16(-1e9f, 0.001f) == -32767);
    CHECK(focp_scale_to_i16(1.2345f, 0.001f) == 1235);
    CHECK(focp_scale_to_i16(-1.2345f, 0.001f) == -1235);
}

static void testCan()
{
    std::puts("can mapping");
    CHECK(FOCP_CAN_ID(FOCP_FN_CMD, 1) == 0x201);
    CHECK(FOCP_CAN_ID(FOCP_FN_INFO, 15) == 0x26F);
    focp_msg_t m, r;
    focp_build_param_req(&m, 3, FOCP_OP_WRITE, 0x0310, 0.5f);
    uint32_t id; uint8_t d[8], dlc;
    fochost::msgToCan(m, id, d, dlc);
    CHECK(id == 0x213 && dlc == 7);
    CHECK(fochost::canToMsg(id, d, dlc, r));
    CHECK(r.fn == FOCP_FN_PARAM_REQ && r.node == 3 && r.len == 7);
    CHECK(focp_get_f32(&r.data[3]) == 0.5f);
    const uint8_t legacy[8] = {1};
    CHECK(!fochost::canToMsg(0x101, legacy, 2, r));            // CarpeDiem Freemode frame ignored
}

static void testFrame()
{
    std::puts("stream framing (COBS + CRC)");
    std::mt19937 rng(42);
    std::uniform_int_distribution<int> byte(0, 255), len(0, 8);
    focp_frame_decoder_t dec;
    focp_frame_decoder_init(&dec);

    int ok = 0;
    for (int n = 0; n < 2000; ++n) {
        focp_msg_t m{};
        m.fn = uint16_t(0x20 + n % 7);
        m.node = uint16_t(n % 16);
        m.len = uint16_t(len(rng));
        for (int i = 0; i < m.len; ++i) m.data[i] = focp_byte_t(n % 3 == 0 ? 0 : byte(rng));   // many zeros
        focp_byte_t buf[FOCP_FRAME_MAX];
        const uint16_t fl = focp_frame_encode(&m, buf);
        CHECK(fl <= FOCP_FRAME_MAX);
        for (int i = 0; i + 1 < fl; ++i) CHECK(buf[i] != 0);      // delimiter only at the end
        CHECK(buf[fl - 1] == 0);

        // some garbage between frames must not break decoding
        if (n % 10 == 0) { focp_msg_t junk; for (int g = 0; g < 5; ++g) focp_frame_decoder_feed(&dec, focp_byte_t(byte(rng) | 1), &junk); focp_frame_decoder_feed(&dec, 0, &junk); }

        focp_msg_t r{};
        int got = 0;
        for (int i = 0; i < fl; ++i) got += focp_frame_decoder_feed(&dec, buf[i], &r);
        CHECK(got == 1);
        if (got == 1) {
            bool same = r.fn == m.fn && r.node == m.node && r.len == m.len;
            for (int i = 0; i < m.len; ++i) same = same && r.data[i] == m.data[i];
            CHECK(same);
            ok += same;
        }
    }
    CHECK(ok == 2000);

    // corrupted frame -> CRC error, no message
    focp_msg_t m{}, r{};
    focp_build_status(&m, 1, FOCP_ST_RUN, 2, 0, 0, 24.0f, 30.0f);
    focp_byte_t buf[FOCP_FRAME_MAX];
    uint16_t fl = focp_frame_encode(&m, buf);
    buf[3] ^= 0x10;
    if (buf[3] == 0) buf[3] = 1;
    const uint32_t before = dec.crc_errors + dec.frame_errors;
    int got = 0;
    for (int i = 0; i < fl; ++i) got += focp_frame_decoder_feed(&dec, buf[i], &r);
    CHECK(got == 0);
    CHECK(dec.crc_errors + dec.frame_errors == before + 1);
}

// --- device side --------------------------------------------------------------------------
struct DevFixture {
    std::vector<focp_msg_t> tx;
    int locked = 0;
    std::vector<uint16_t> cmds;
    focp_dev_t dev{};
    DevFixture()
    {
        focp_dev_cfg_t c{};
        c.node = 2;
        c.fw_major = 1; c.fw_minor = 3;
        c.ctx = this;
        c.send = [](void* ctx, const focp_msg_t* m) { static_cast<DevFixture*>(ctx)->tx.push_back(*m); return 0; };
        c.is_locked = [](void* ctx) { return static_cast<DevFixture*>(ctx)->locked; };
        c.on_command = [](void* ctx, uint16_t cmd, const focp_byte_t*, uint16_t) { static_cast<DevFixture*>(ctx)->cmds.push_back(cmd); };
        focp_dev_init(&dev, &c);
    }
    const focp_msg_t& last() const { return tx.back(); }
};

static void testDev()
{
    std::puts("device side");
    DevFixture f;
    focp_msg_t m;

    // read
    focp_build_param_req(&m, 2, FOCP_OP_READ, 0x0101, 0);
    focp_dev_rx(&f.dev, &m);
    CHECK(f.tx.size() == 1 && f.last().fn == FOCP_FN_PARAM_RSP && f.last().data[0] == FOCP_PS_OK);
    CHECK(focp_get_f32(&f.last().data[3]) == 0.0225f);

    // other node ignored
    focp_build_param_req(&m, 3, FOCP_OP_READ, 0x0101, 0);
    focp_dev_rx(&f.dev, &m);
    CHECK(f.tx.size() == 1);

    // write ok / out of range / read only / unknown
    focp_build_param_req(&m, 2, FOCP_OP_WRITE, 0x0310, 1.25f);
    focp_dev_rx(&f.dev, &m);
    CHECK(f.last().data[0] == FOCP_PS_OK && focp_dev_get(&f.dev, FOCP_P_CUR_KP) == 1.25f);
    focp_build_param_req(&m, 2, FOCP_OP_WRITE, 0x0310, 1000.0f);
    focp_dev_rx(&f.dev, &m);
    CHECK(f.last().data[0] == FOCP_PS_OUT_OF_RANGE && focp_dev_get(&f.dev, FOCP_P_CUR_KP) == 1.25f);
    focp_build_param_req(&m, 2, FOCP_OP_WRITE, 0x0F00, 5.0f);
    focp_dev_rx(&f.dev, &m);
    CHECK(f.last().data[0] == FOCP_PS_READ_ONLY);
    focp_build_param_req(&m, 2, FOCP_OP_WRITE, 0x7777, 5.0f);
    focp_dev_rx(&f.dev, &m);
    CHECK(f.last().data[0] == FOCP_PS_UNKNOWN_ID);
    CHECK(focp_dev_get(&f.dev, FOCP_P_SYS_FW_VERSION) == float(1 * 256 + 3));

    // lock: motor params cannot change while running, gains can
    f.locked = 1;
    focp_build_param_req(&m, 2, FOCP_OP_WRITE, 0x0101, 0.05f);
    focp_dev_rx(&f.dev, &m);
    CHECK(f.last().data[0] == FOCP_PS_LOCKED);
    focp_build_param_req(&m, 2, FOCP_OP_WRITE, 0x0311, 100.0f);
    focp_dev_rx(&f.dev, &m);
    CHECK(f.last().data[0] == FOCP_PS_OK);
    f.locked = 0;

    // integer rounding
    focp_build_param_req(&m, 2, FOCP_OP_WRITE, 0x0100, 6.4f);
    focp_dev_rx(&f.dev, &m);
    CHECK(focp_dev_get(&f.dev, FOCP_P_MOTOR_POLE_PAIRS) == 6.0f);

    // application commands forwarded, internal ones not
    focp_build_cmd(&m, 2, FOCP_CMD_ENABLE);  focp_dev_rx(&f.dev, &m);
    focp_build_cmd(&m, 2, FOCP_CMD_HEARTBEAT); focp_dev_rx(&f.dev, &m);
    CHECK(f.cmds.size() == 1 && f.cmds[0] == FOCP_CMD_ENABLE);

    // info
    size_t n0 = f.tx.size();
    focp_build_cmd(&m, 2, FOCP_CMD_GET_INFO); focp_dev_rx(&f.dev, &m);
    CHECK(f.tx.size() == n0 + 1 && f.last().fn == FOCP_FN_INFO && focp_get_u16(&f.last().data[4]) == FOCP_NUM_PARAMS);

    // telemetry: 4 slots -> 2 frames per sample, divider 2
    f.tx.clear();
    focp_build_cmd_u8u8(&m, 2, FOCP_CMD_TELEM_SLOT, 0, 11); focp_dev_rx(&f.dev, &m);   // speed
    focp_build_cmd_u8u8(&m, 2, FOCP_CMD_TELEM_SLOT, 1, 5);  focp_dev_rx(&f.dev, &m);   // iq
    focp_build_cmd_u8u8(&m, 2, FOCP_CMD_TELEM_SLOT, 3, 15); focp_dev_rx(&f.dev, &m);   // udc (slot 2 empty)
    focp_build_cmd_u16(&m, 2, FOCP_CMD_TELEM_RATE, 2);       focp_dev_rx(&f.dev, &m);
    focp_build_cmd_u8(&m, 2, FOCP_CMD_TELEM_RUN, 1);         focp_dev_rx(&f.dev, &m);
    float sig[FOCP_NUM_SIGNALS] = {};
    sig[FOCP_S_SPEED] = 1500.0f; sig[FOCP_S_IQ] = -2.5f; sig[FOCP_S_UDC] = 24.1f;
    for (int i = 0; i < 6; ++i) focp_dev_sample(&f.dev, sig);
    CHECK(f.tx.size() == 6);                                  // 3 samples x 2 frames
    CHECK(f.tx[0].data[0] == 0 && f.tx[0].data[1] == 0 && f.tx[1].data[1] == 3);
    CHECK(f.tx[2].data[0] == 1);                              // seq increments per sample
    CHECK_NEAR(focp_get_i16(&f.tx[0].data[2]) * 0.25, 1500.0, 0.01);
    CHECK_NEAR(focp_get_i16(&f.tx[0].data[4]) * 0.001, -2.5, 0.001);
    CHECK(focp_get_i16(&f.tx[0].data[6]) == 0);
    CHECK_NEAR(focp_get_i16(&f.tx[1].data[2]) * 0.01, 24.1, 0.01);

    // heartbeat timeout
    focp_dev_set(&f.dev, FOCP_P_COMM_TIMEOUT, 100.0f);
    focp_build_cmd(&m, 2, FOCP_CMD_HEARTBEAT); focp_dev_rx(&f.dev, &m);
    int to = 0;
    for (int i = 0; i < 100; ++i) to |= focp_dev_tick_ms(&f.dev, 1);
    CHECK(to == 0);
    CHECK(focp_dev_tick_ms(&f.dev, 1) == 1);
}

static void testTargetBinding()
{
    std::puts("firmware binding (focp_target)");
    focp_target_init(1);
    // host asks for cur.Kp via CAN id 0x211
    const uint16_t req[8] = {FOCP_OP_READ, 0x10, 0x03, 0, 0, 0, 0, 0};
    focp_target_rx_can(0x211, req, 3);
    uint32_t id = 0; uint16_t d[8], len = 0;
    CHECK(focp_target_tx_pop(&id, d, &len) == 1);
    CHECK(id == 0x221 && len == 7 && d[0] == FOCP_PS_OK);
    focp_byte_t b[4] = {focp_byte_t(d[3]), focp_byte_t(d[4]), focp_byte_t(d[5]), focp_byte_t(d[6])};
    CHECK(focp_get_f32(b) == focp_target_param(FOCP_P_CUR_KP));
    CHECK(focp_target_tx_pop(&id, d, &len) == 0);
    // commands are queued for the application, SET_REF is latched
    focp_msg_t m;
    focp_build_cmd_f32(&m, 1, FOCP_CMD_SET_REF, 1234.5f);
    uint16_t raw[8]; for (int i = 0; i < 8; ++i) raw[i] = m.data[i];
    focp_target_rx_can(0x201, raw, m.len);
    focp_build_cmd(&m, 1, FOCP_CMD_ENABLE);
    raw[0] = m.data[0];
    focp_target_rx_can(0x201, raw, 1);
    CHECK(focp_target_reference() == 1234.5f);
    CHECK(focp_target_cmd_pending() == 1);
    float arg;
    CHECK(focp_target_cmd_take(&arg) == FOCP_CMD_ENABLE);
    // locked parameter
    focp_target_set_locked(1);
    const uint16_t wr[8] = {FOCP_OP_WRITE, 0x01, 0x01, 0, 0, 0x80, 0x3F, 0};   // motor.Rs = 1.0
    focp_target_rx_can(0x211, wr, 7);
    CHECK(focp_target_tx_pop(&id, d, &len) == 1 && d[0] == FOCP_PS_LOCKED);
}

static void testC28xGlue()
{
    std::puts("app_speed glue (focp_c28x)");
    uint16_t active = 9, fm = 9; float ref = -1.0f;
    const float z[2] = {0.0f, 0.0f};
    focp_c28x_reset();
    // no FOCP traffic: potentiometer mode
    focp_c28x_step(0, 0, 0, 1, 1, 0.0f, 4000.0f, 24.0f, z, z, 56.0f, 3, &active, &fm, &ref);
    CHECK(active == 0 && fm == 0);
    // SET_REF 666 rpm (speedMode 1: span floor(4000/3) = 1333 rpm) -> FOCP mode, Freemode until ENABLE
    focp_msg_t m; uint16_t raw[8];
    focp_build_cmd_f32(&m, 1, FOCP_CMD_SET_REF, 666.5f);
    for (int i = 0; i < 8; ++i) raw[i] = m.data[i];
    focp_target_rx_can(0x201, raw, m.len);
    focp_c28x_step(0, 0, 0, 1, 1, 0.0f, 4000.0f, 24.0f, z, z, 56.0f, 3, &active, &fm, &ref);
    CHECK(active == 1 && fm == 1 && std::fabs(ref - 0.5f) < 1e-6f);
    focp_build_cmd(&m, 1, FOCP_CMD_ENABLE); raw[0] = m.data[0];
    focp_target_rx_can(0x201, raw, 1);
    focp_c28x_step(0, 0, 0, 1, 1, 0.0f, 4000.0f, 24.0f, z, z, 56.0f, 3, &active, &fm, &ref);
    CHECK(active == 1 && fm == 0);
    // no heartbeat for > comm.timeout_ms (500 ms = 1000 steps) -> Freemode, ENABLE refused until ACK_FAULT
    for (int i = 0; i < 1100; ++i) focp_c28x_step(0, 0, 0, 1, 1, 0.0f, 4000.0f, 24.0f, z, z, 56.0f, 3, &active, &fm, &ref);
    CHECK(fm == 1);
    focp_target_rx_can(0x201, raw, 1);
    focp_c28x_step(0, 0, 0, 1, 1, 0.0f, 4000.0f, 24.0f, z, z, 56.0f, 3, &active, &fm, &ref);
    CHECK(fm == 1);
    focp_build_cmd(&m, 1, FOCP_CMD_ACK_FAULT); raw[0] = m.data[0];
    focp_target_rx_can(0x201, raw, 1);
    focp_build_cmd(&m, 1, FOCP_CMD_ENABLE); raw[0] = m.data[0];
    focp_target_rx_can(0x201, raw, 1);
    focp_c28x_step(0, 0, 0, 1, 1, 0.0f, 4000.0f, 24.0f, z, z, 56.0f, 3, &active, &fm, &ref);
    CHECK(fm == 0);
    // STATUS frames were queued (0x231)
    uint32_t id = 0; uint16_t d[8], len = 0; bool status = false;
    while (focp_target_tx_pop(&id, d, &len))
        status = status || (id == 0x231 && d[4] == 0x60 && d[5] == 0x09);   // Udc 24 V = 2400
    CHECK(status);
}

int main()
{
    testTargetBinding();
    testC28xGlue();
    testTables();
    testBytes();
    testCan();
    testFrame();
    testDev();
    return TEST_RESULT();
}
