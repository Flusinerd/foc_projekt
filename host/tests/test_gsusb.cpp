// gs_usb (CANable/candleLight): bit timing, frame layout, open sequence and an end-to-end
// run Device <-> GsUsbTransport <-> simulated gs_usb adapter <-> virtual drive.
#include "check.hpp"

#include <fochost/Device.hpp>
#include <fochost/GsUsb.hpp>
#include <fochost/SimTransport.hpp>

#include <chrono>
#include <deque>
#include <mutex>
#include <thread>
#include <vector>

using namespace fochost;
using namespace fochost::gsusb;
using namespace std::chrono_literals;

namespace {

// STM32F042 (CANable, original candleLight): bxCAN at 48 MHz.
BtConst bxcan48()
{
    BtConst c;
    c.fclk = 48000000;
    c.tseg1Min = 1; c.tseg1Max = 16;
    c.tseg2Min = 1; c.tseg2Max = 8;
    c.sjwMax = 4;
    c.brpMin = 1; c.brpMax = 1024; c.brpInc = 1;
    return c;
}

// STM32G431 (CANable 2.x): FDCAN nominal timing at 170 MHz.
BtConst fdcan170()
{
    BtConst c;
    c.fclk = 170000000;
    c.tseg1Min = 1; c.tseg1Max = 256;
    c.tseg2Min = 1; c.tseg2Max = 128;
    c.sjwMax = 128;
    c.brpMin = 1; c.brpMax = 512; c.brpInc = 1;
    return c;
}

template <class F> bool waitFor(F cond, std::chrono::milliseconds timeout)
{
    const auto end = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < end) {
        if (cond()) return true;
        std::this_thread::sleep_for(5ms);
    }
    return cond();
}

struct ControlCall {
    bool in;
    uint8_t request;
    uint16_t value;
    std::vector<uint8_t> data;
};

/// Simulated gs_usb adapter. With a drive attached, TX frames go to the drive and the
/// drive's answers come back as RX frames; every TX frame is echoed first.
class FakeAdapter final : public UsbIo {
public:
    explicit FakeAdapter(BtConst c, SimTransport* drive = nullptr) : bt_(c), drive_(drive) {}

    bool open(std::string& error) override
    {
        if (drive_ && !drive_->open(error)) return false;
        opened = true;
        return true;
    }
    void close() override
    {
        if (drive_) drive_->close();
        closed = true;
    }

    bool controlOut(uint8_t request, uint16_t value, const uint8_t* data, uint16_t len) override
    {
        std::lock_guard<std::mutex> lk(mx_);
        calls.push_back({false, request, value, std::vector<uint8_t>(data, data + len)});
        return true;
    }

    bool controlIn(uint8_t request, uint16_t value, uint8_t* data, uint16_t len) override
    {
        std::lock_guard<std::mutex> lk(mx_);
        calls.push_back({true, request, value, {}});
        if (request != kBtConst || len != kBtConstSize) return false;
        const uint32_t v[] = {bt_.feature, bt_.fclk, bt_.tseg1Min, bt_.tseg1Max, bt_.tseg2Min,
                              bt_.tseg2Max, bt_.sjwMax, bt_.brpMin, bt_.brpMax, bt_.brpInc};
        for (size_t i = 0; i < 10; ++i)
            for (int b = 0; b < 4; ++b) data[4 * i + b] = uint8_t(v[i] >> (8 * b));
        return true;
    }

    bool bulkWrite(const uint8_t* data, size_t len) override
    {
        HostFrame f;
        if (!unpackFrame(data, len, f)) return false;
        {
            std::lock_guard<std::mutex> lk(mx_);
            tx.push_back(f);
            rx_.push_back(f);   // echo after "transmission"
        }
        focp_msg_t m;
        if (drive_ && canToMsg(f.canId, f.data, f.dlc, m)) drive_->send(m);
        return true;
    }

    int bulkRead(uint8_t* data, size_t len, int timeoutMs) override
    {
        HostFrame f;
        {
            std::lock_guard<std::mutex> lk(mx_);
            if (!rx_.empty()) {
                f = rx_.front();
                rx_.pop_front();
                packFrame(f, data);
                return int(kFrameSize);
            }
        }
        if (len < kFrameSize || !drive_) return 0;
        // Interleave traffic the transport has to ignore.
        switch (++reads_ % 7) {
        case 1: f.canId = 0x101; f.dlc = 2; break;                        // old CarPediem ID
        case 2: f.canId = kFlagEff | FOCP_CAN_ID(FOCP_FN_STATUS, 1); break; // 29-bit ID
        case 3: f.canId = kFlagErr | 0x04; f.dlc = 8; break;              // error frame
        default: {
            focp_msg_t m;
            if (!drive_->receive(m, timeoutMs)) return 0;
            uint32_t id = 0;
            msgToCan(m, id, f.data, f.dlc);
            f.canId = id;
        }
        }
        f.echoId = kEchoRx;
        packFrame(f, data);
        return int(kFrameSize);
    }

    std::vector<ControlCall> calls;
    std::vector<HostFrame> tx;
    bool opened = false, closed = false;

private:
    BtConst bt_;
    SimTransport* drive_;
    std::mutex mx_;
    std::deque<HostFrame> rx_;
    unsigned reads_ = 0;
};

void checkTiming(const BtConst& c, int bitrate)
{
    BitTiming t;
    const bool ok = calcBitTiming(c, bitrate, t);
    CHECK(ok);
    if (!ok) return;
    const double actual = double(c.fclk) / (double(t.brp) * t.tq());
    const double sp = double(1 + t.propSeg + t.phaseSeg1) / t.tq();
    std::printf("  %3u MHz %7d bit/s: brp=%u tq=%u sp=%.1f %%\n", c.fclk / 1000000, bitrate, t.brp, t.tq(), sp * 100);
    CHECK(actual == double(bitrate));
    CHECK(sp >= 0.80 && sp <= 0.90);
    CHECK(t.tq() >= 8 && t.tq() <= 25);
    CHECK(t.propSeg + t.phaseSeg1 >= c.tseg1Min && t.propSeg + t.phaseSeg1 <= c.tseg1Max);
    CHECK(t.phaseSeg2 >= c.tseg2Min && t.phaseSeg2 <= c.tseg2Max);
    CHECK(t.sjw >= 1 && t.sjw <= c.sjwMax);
    CHECK(t.brp >= c.brpMin && t.brp <= c.brpMax);
}

} // namespace

int main()
{
    std::puts("bit timing");
    for (int br : {125000, 250000, 500000, 1000000}) {
        checkTiming(bxcan48(), br);
        checkTiming(fdcan170(), br);
    }
    {
        BtConst c = bxcan48();
        c.fclk = 1000003;   // no exact divider
        BitTiming t;
        CHECK(!calcBitTiming(c, 500000, t));
        CHECK(!calcBitTiming(bxcan48(), 0, t));
    }

    std::puts("frame layout (little endian)");
    {
        HostFrame f;
        f.echoId = 0x01020304;
        f.canId = 0x234;
        f.dlc = 3;
        f.data[0] = 0xAA; f.data[1] = 0xBB; f.data[2] = 0xCC;
        uint8_t b[kFrameSize];
        packFrame(f, b);
        CHECK(b[0] == 0x04 && b[3] == 0x01);
        CHECK(b[4] == 0x34 && b[5] == 0x02 && b[6] == 0 && b[7] == 0);
        CHECK(b[8] == 3 && b[12] == 0xAA && b[14] == 0xCC);
        HostFrame g;
        CHECK(unpackFrame(b, sizeof(b), g));
        CHECK(g.echoId == f.echoId && g.canId == f.canId && g.dlc == 3 && g.data[1] == 0xBB);
        CHECK(!unpackFrame(b, kFrameSize - 1, g));
    }

    std::puts("open/close sequence");
    {
        auto io = std::make_unique<FakeAdapter>(bxcan48());
        FakeAdapter* fake = io.get();
        GsUsbTransport t(std::move(io), 500000);
        std::string err;
        CHECK(t.open(err));
        CHECK(t.isOpen());
        CHECK(fake->calls.size() == 5);
        if (fake->calls.size() == 5) {
            const auto& c = fake->calls;
            CHECK(!c[0].in && c[0].request == kHostFormat && c[0].value == 1);
            CHECK(c[0].data == std::vector<uint8_t>({0xEF, 0xBE, 0x00, 0x00}));
            CHECK(c[1].in && c[1].request == kBtConst);
            CHECK(!c[2].in && c[2].request == kMode && c[2].data[0] == kModeReset);
            CHECK(!c[3].in && c[3].request == kBitTiming && c[3].data.size() == kBitTimingSize);
            uint8_t expect[kBitTimingSize];
            packBitTiming(t.bitTiming(), expect);
            CHECK(c[3].data == std::vector<uint8_t>(expect, expect + kBitTimingSize));
            CHECK(!c[4].in && c[4].request == kMode && c[4].data[0] == kModeStart);
        }
        CHECK(t.bitTiming().brp * t.bitTiming().tq() == 96u);   // 48 MHz / 500 kbit/s

        focp_msg_t m{};
        m.fn = FOCP_FN_CMD;
        m.node = 3;
        m.len = 2;
        m.data[0] = FOCP_CMD_HEARTBEAT;
        CHECK(t.send(m));
        CHECK(fake->tx.size() == 1 && fake->tx[0].canId == FOCP_CAN_ID(FOCP_FN_CMD, 3) && fake->tx[0].dlc == 2);
        CHECK(fake->tx[0].echoId != kEchoRx);
        focp_msg_t r;
        CHECK(!t.receive(r, 10));   // only the echo is pending -> discarded, then timeout

        t.close();
        CHECK(!t.isOpen() && fake->closed);
        CHECK(fake->calls.back().request == kMode && fake->calls.back().data[0] == kModeReset);
    }

    std::puts("unsupported bitrate is reported");
    {
        BtConst c = bxcan48();
        c.fclk = 1000003;
        auto io = std::make_unique<FakeAdapter>(c);
        FakeAdapter* fake = io.get();
        GsUsbTransport t(std::move(io), 500000);
        std::string err;
        CHECK(!t.open(err));
        CHECK(!err.empty());
        CHECK(fake->closed);
        std::printf("  -> %s\n", err.c_str());
    }

    std::puts("end to end: Device over gs_usb to the virtual drive");
    {
        SimTransport drive(1);
        auto io = std::make_unique<FakeAdapter>(bxcan48(), &drive);
        FakeAdapter* fake = io.get();
        Device dev;
        std::string err;
        CHECK(dev.connect(std::make_unique<GsUsbTransport>(std::move(io), 500000), 1, err));
        CHECK(waitFor([&] { for (auto& p : dev.params()) if (!p.valid) return false; return true; }, 3000ms));
        CHECK(waitFor([&] { return dev.info().valid; }, 1000ms));
        CHECK(dev.info().numParams == FOCP_NUM_PARAMS);
        CHECK(waitFor([&] { return dev.status().valid && dev.status().state == FOCP_ST_READY; }, 2000ms));
        dev.writeParam(FOCP_P_HALL_OFFSET, 21.5f);
        CHECK(waitFor([&] { auto p = dev.params()[FOCP_P_HALL_OFFSET]; return !p.pending && p.value == 21.5f; }, 1000ms));
        CHECK(fake->opened);
        dev.disconnect();
        CHECK(fake->closed);
    }

    return TEST_RESULT();
}
