// Closed-loop tests of the virtual drive (no threads, simulated time only).
#include "check.hpp"

#include <fochost/VirtualDrive.hpp>

#include <algorithm>
#include <vector>

using fochost::VirtualDrive;

struct Bench {
    std::vector<focp_msg_t> out;
    VirtualDrive drv{1, &Bench::sink, this};

    static int sink(void* ctx, const focp_msg_t* m)
    {
        static_cast<Bench*>(ctx)->out.push_back(*m);
        return 0;
    }
    void cmd(uint16_t c) { focp_msg_t m; focp_build_cmd(&m, 1, c); drv.rx(m); }
    void ref(float v) { focp_msg_t m; focp_build_cmd_f32(&m, 1, FOCP_CMD_SET_REF, v); drv.rx(m); }
    void calib(uint16_t w) { focp_msg_t m; focp_build_cmd_u8(&m, 1, FOCP_CMD_CALIBRATE, w); drv.rx(m); }
    uint16_t set(uint16_t id, float v)
    {
        focp_msg_t m;
        focp_build_param_req(&m, 1, FOCP_OP_WRITE, id, v);
        drv.rx(m);
        return out.back().data[0] & 0xFF;
    }
    void run(double seconds)
    {
        const int n = int(seconds / drv.controlPeriod());
        for (int i = 0; i < n; ++i) drv.step();
        out.clear();
    }
    Bench()
    {
        set(0x0C00, 0);             // no heartbeat timeout in these tests
        run(0.4);                   // INIT -> OFFSET_CAL -> READY
    }
};

static void testStartup()
{
    std::puts("startup sequence");
    Bench b;
    CHECK(b.drv.state() == FOCP_ST_READY);
    CHECK(b.drv.faults() == 0);
}

static void testSpeedHall()
{
    std::puts("speed control with hall (after hall calibration)");
    Bench b;
    b.calib(FOCP_CALIB_HALL);
    b.run(3.0);
    CHECK(b.drv.state() == FOCP_ST_READY);
    std::printf("  calibrated hall.offset = %.1f deg (true %.1f)\n", b.drv.param(FOCP_P_HALL_OFFSET), VirtualDrive::kTrueHallOffsetDeg);
    CHECK_NEAR(b.drv.param(FOCP_P_HALL_OFFSET), VirtualDrive::kTrueHallOffsetDeg, 3.0);

    b.ref(2000.0f);
    b.cmd(FOCP_CMD_ENABLE);
    b.run(1.5);
    CHECK(b.drv.state() == FOCP_ST_RUN);
    CHECK_NEAR(b.drv.speedRpm(), 2000.0, 60.0);

    b.set(0x0E00, 0.3f);           // load step 0.3 Nm
    b.run(1.0);
    CHECK_NEAR(b.drv.speedRpm(), 2000.0, 60.0);
    // iq must carry the load: T = 1.5 * p * psi * iq
    CHECK_NEAR(b.drv.iq(), 0.3 / (1.5 * 4 * 0.00587), 1.5);

    b.ref(-1000.0f);
    b.set(0x0E00, 0.0f);
    b.run(1.5);
    CHECK_NEAR(b.drv.speedRpm(), -1000.0, 50.0);
    b.cmd(FOCP_CMD_DISABLE);
    b.run(0.1);
    CHECK(b.drv.state() == FOCP_ST_READY);
}

static void testUncalibratedHallIsWorse()
{
    std::puts("hall offset error costs current");
    Bench a, b;
    b.set(0x0400, float(VirtualDrive::kTrueHallOffsetDeg));
    for (Bench* x : {&a, &b}) {
        x->set(0x0E00, 0.2f);
        x->ref(1500.0f);
        x->cmd(FOCP_CMD_ENABLE);
        x->run(2.0);
    }
    // same torque, but with 23 deg error the controller needs |i| = iq/cos(23 deg)
    CHECK(a.drv.state() == FOCP_ST_RUN && b.drv.state() == FOCP_ST_RUN);
    CHECK_NEAR(b.drv.speedRpm(), 1500.0, 50.0);
    const double ia = std::hypot(a.drv.id(), a.drv.iq()), ib = std::hypot(b.drv.id(), b.drv.iq());
    std::printf("  |i| uncalibrated %.2f A, calibrated %.2f A\n", ia, ib);
    CHECK(ia > 1.05 * ib);
}

static void testPosition()
{
    std::puts("position control with hall");
    Bench b;
    b.set(0x0400, float(VirtualDrive::kTrueHallOffsetDeg));
    CHECK(b.set(0x0300, FOCP_MODE_POSITION) == FOCP_PS_OK);
    // hall resolution: 360 / (6 * 4) = 15 deg mech per state -> expect settling within ~1 state
    for (float target : {720.0f, -90.0f}) {
        b.ref(target);
        b.cmd(FOCP_CMD_ENABLE);
        b.run(3.0);
        double lo = 1e9, hi = -1e9;
        for (int i = 0; i < 20; ++i) {                // observe 1 s: must be at rest near target
            b.run(0.05);
            lo = std::min(lo, b.drv.positionDeg());
            hi = std::max(hi, b.drv.positionDeg());
        }
        std::printf("  target %.0f deg: position %.1f .. %.1f deg\n", target, lo, hi);
        CHECK_NEAR(lo, target, 15.0);
        CHECK_NEAR(hi, target, 15.0);
        CHECK(hi - lo < 5.0);                          // no hunting
    }
}

static void testSensorless()
{
    std::puts("sensorless: I/f start-up, then observer");
    Bench b;
    CHECK(b.set(0x0301, FOCP_SENS_SENSORLESS) == FOCP_PS_OK);
    b.ref(3000.0f);
    b.cmd(FOCP_CMD_ENABLE);
    b.run(3.0);
    CHECK(b.drv.state() == FOCP_ST_RUN);
    CHECK_NEAR(b.drv.speedRpm(), 3000.0, 100.0);
}

static void testHybrid()
{
    std::puts("hybrid: hall at low speed, observer at high speed");
    Bench b;
    b.set(0x0400, float(VirtualDrive::kTrueHallOffsetDeg));
    CHECK(b.set(0x0301, FOCP_SENS_HYBRID) == FOCP_PS_OK);
    b.ref(200.0f);
    b.cmd(FOCP_CMD_ENABLE);
    b.run(1.0);
    CHECK_NEAR(b.drv.speedRpm(), 200.0, 25.0);
    b.ref(3000.0f);
    b.run(1.5);
    CHECK_NEAR(b.drv.speedRpm(), 3000.0, 100.0);
    b.ref(100.0f);
    b.run(1.5);
    CHECK_NEAR(b.drv.speedRpm(), 100.0, 25.0);
}

static void testFaults()
{
    std::puts("protection: undervoltage, comm timeout, locking");
    Bench b;
    b.ref(1000.0f);
    b.cmd(FOCP_CMD_ENABLE);
    b.run(0.2);
    CHECK(b.set(0x0101, 0.1f) == FOCP_PS_LOCKED);        // Rs locked while running
    b.set(0x0E01, 15.0f);                                 // supply drops below 17 V
    b.run(0.01);
    CHECK(b.drv.state() == FOCP_ST_FAULT);
    CHECK(b.drv.faults() & FOCP_FAULT_UNDERVOLTAGE);
    b.cmd(FOCP_CMD_ENABLE);                               // must not restart while faulted
    b.run(0.01);
    CHECK(b.drv.state() == FOCP_ST_FAULT);
    b.set(0x0E01, 24.0f);
    b.cmd(FOCP_CMD_ACK_FAULT);
    b.run(0.01);
    CHECK(b.drv.state() == FOCP_ST_READY && b.drv.faults() == 0);

    b.set(0x0C00, 200.0f);                                // heartbeat timeout 200 ms
    b.cmd(FOCP_CMD_ENABLE);
    b.run(0.15);
    CHECK(b.drv.state() == FOCP_ST_RUN);
    b.run(0.1);
    CHECK(b.drv.state() == FOCP_ST_FAULT && (b.drv.faults() & FOCP_FAULT_COMM));
}

static void testHallNoise()
{
    std::puts("noisy hall lines trigger the hall fault");
    Bench b;
    b.set(0x0400, float(VirtualDrive::kTrueHallOffsetDeg));
    b.set(0x0E02, 0.002f);
    b.ref(1000.0f);
    b.cmd(FOCP_CMD_ENABLE);
    b.run(1.0);
    CHECK(b.drv.state() == FOCP_ST_FAULT && (b.drv.faults() & FOCP_FAULT_HALL));
}

int main()
{
    testStartup();
    testSpeedHall();
    testUncalibratedHallIsWorse();
    testPosition();
    testSensorless();
    testHybrid();
    testFaults();
    testHallNoise();
    return TEST_RESULT();
}
