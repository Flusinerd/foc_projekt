// gs_usb protocol and transport (platform independent part).
#include "fochost/GsUsb.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>

namespace fochost {
namespace {
void put32(uint8_t* p, uint32_t v)
{
    for (int i = 0; i < 4; ++i) p[i] = uint8_t(v >> (8 * i));
}
uint32_t get32(const uint8_t* p)
{
    return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
}
} // namespace

namespace gsusb {

bool calcBitTiming(const BtConst& c, int bitrate, BitTiming& out)
{
    if (bitrate <= 0 || c.fclk == 0) return false;
    const uint32_t inc = c.brpInc ? c.brpInc : 1;
    const uint32_t brpMin = c.brpMin ? c.brpMin : 1;
    // Ranking: 8..25 time quanta per bit (CiA recommendation, keeps SJW meaningful),
    // then sample point error, then more time quanta.
    bool found = false;
    bool bestInRange = false;
    double bestErr = 1.0;
    uint32_t bestTq = 0;
    for (uint32_t brp = brpMin; brp <= c.brpMax; brp += inc) {
        const uint64_t div = uint64_t(brp) * uint64_t(bitrate);
        if (c.fclk % div) continue;                      // only exact bitrates
        const uint32_t tq = uint32_t(c.fclk / div);
        if (tq < 1 + c.tseg1Min + c.tseg2Min || tq > 1 + c.tseg1Max + c.tseg2Max) continue;
        uint32_t tseg2 = tq - uint32_t(std::lround(tq * 0.875));
        if (tseg2 < c.tseg2Min) tseg2 = c.tseg2Min;
        if (tseg2 > c.tseg2Max) tseg2 = c.tseg2Max;
        uint32_t tseg1 = tq - 1 - tseg2;
        if (tseg1 > c.tseg1Max) { tseg1 = c.tseg1Max; tseg2 = tq - 1 - tseg1; }
        if (tseg1 < c.tseg1Min || tseg1 < 2 || tseg2 < c.tseg2Min || tseg2 > c.tseg2Max) continue;
        const double err = std::fabs(double(1 + tseg1) / tq - 0.875);
        const bool inRange = tq >= 8 && tq <= 25;
        const bool better = !found || (inRange != bestInRange ? inRange
                                       : std::fabs(err - bestErr) > 1e-9 ? err < bestErr
                                       : tq > bestTq);
        if (better) {
            found = true;
            bestInRange = inRange;
            bestErr = err;
            bestTq = tq;
            out.brp = brp;
            out.propSeg = 1;
            out.phaseSeg1 = tseg1 - 1;
            out.phaseSeg2 = tseg2;
            out.sjw = std::min(c.sjwMax ? c.sjwMax : 1u, tseg2);
        }
    }
    return found;
}

void packFrame(const HostFrame& f, uint8_t* buf)
{
    put32(buf, f.echoId);
    put32(buf + 4, f.canId);
    buf[8] = f.dlc;
    buf[9] = f.channel;
    buf[10] = f.flags;
    buf[11] = 0;
    for (int i = 0; i < 8; ++i) buf[12 + i] = f.data[i];
}

bool unpackFrame(const uint8_t* buf, size_t len, HostFrame& f)
{
    if (len < kFrameSize) return false;
    f.echoId = get32(buf);
    f.canId = get32(buf + 4);
    f.dlc = buf[8];
    f.channel = buf[9];
    f.flags = buf[10];
    for (int i = 0; i < 8; ++i) f.data[i] = buf[12 + i];
    return true;
}

void packBitTiming(const BitTiming& t, uint8_t* buf)
{
    put32(buf, t.propSeg);
    put32(buf + 4, t.phaseSeg1);
    put32(buf + 8, t.phaseSeg2);
    put32(buf + 12, t.sjw);
    put32(buf + 16, t.brp);
}

BtConst unpackBtConst(const uint8_t* buf)
{
    BtConst c;
    uint32_t* f[] = {&c.feature, &c.fclk, &c.tseg1Min, &c.tseg1Max, &c.tseg2Min,
                     &c.tseg2Max, &c.sjwMax, &c.brpMin, &c.brpMax, &c.brpInc};
    for (size_t i = 0; i < 10; ++i) *f[i] = get32(buf + 4 * i);
    return c;
}

} // namespace gsusb

// ------------------------------------------------------------------------------------------
using namespace gsusb;

GsUsbTransport::GsUsbTransport(std::unique_ptr<UsbIo> io, int bitrate) : io_(std::move(io)), bitrate_(bitrate) {}

GsUsbTransport::~GsUsbTransport() { close(); }

std::string GsUsbTransport::name() const
{
    return "CANable/candleLight " + std::to_string(bitrate_ / 1000) + " kbit/s";
}

bool GsUsbTransport::open(std::string& error)
{
    if (!io_) { error = "gs_usb wird auf dieser Plattform nicht direkt unterstuetzt (Linux: SocketCAN verwenden)"; return false; }
    if (!io_->open(error)) return false;
    auto fail = [&](const char* what) {
        error = std::string("gs_usb: ") + what;
        io_->close();
        return false;
    };

    uint8_t buf[kBtConstSize] = {};
    put32(buf, 0x0000BEEFu);   // host format: little endian
    if (!io_->controlOut(kHostFormat, 1, buf, 4)) return fail("HOST_FORMAT abgelehnt");
    if (!io_->controlIn(kBtConst, 0, buf, uint16_t(kBtConstSize))) return fail("BT_CONST nicht lesbar");
    const BtConst c = unpackBtConst(buf);
    fclk_ = c.fclk;
    if (!calcBitTiming(c, bitrate_, timing_)) {
        error = "gs_usb: " + std::to_string(bitrate_) + " bit/s mit " + std::to_string(c.fclk) +
                " Hz CAN-Takt nicht exakt einstellbar";
        io_->close();
        return false;
    }

    uint8_t mode[8] = {};
    if (!io_->controlOut(kMode, 0, mode, 8)) return fail("MODE reset abgelehnt");
    uint8_t bt[kBitTimingSize];
    packBitTiming(timing_, bt);
    if (!io_->controlOut(kBitTiming, 0, bt, uint16_t(kBitTimingSize))) return fail("BITTIMING abgelehnt");
    mode[0] = uint8_t(kModeStart);
    if (!io_->controlOut(kMode, 0, mode, 8)) return fail("MODE start abgelehnt");
    open_ = true;
    return true;
}

void GsUsbTransport::close()
{
    if (!open_) return;
    open_ = false;
    uint8_t mode[8] = {};
    io_->controlOut(kMode, 0, mode, 8);   // bus off; ignore errors (adapter may be unplugged)
    io_->close();
}

bool GsUsbTransport::send(const focp_msg_t& m)
{
    if (!open_) return false;
    HostFrame f;
    uint32_t id = 0;
    msgToCan(m, id, f.data, f.dlc);
    f.canId = id;
    f.echoId = nextEcho_++ & 0x3F;   // echoes are only discarded, any id < kEchoRx works
    uint8_t buf[kFrameSize];
    packFrame(f, buf);
    return io_->bulkWrite(buf, sizeof(buf));
}

bool GsUsbTransport::receive(focp_msg_t& m, int timeoutMs)
{
    if (!open_) return false;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    for (;;) {
        const auto left = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now());
        uint8_t buf[64];
        const int n = io_->bulkRead(buf, sizeof(buf), left.count() > 0 ? int(left.count()) : 0);
        if (n <= 0) return false;
        HostFrame f;
        if (unpackFrame(buf, size_t(n), f) && f.echoId == kEchoRx &&
            !(f.canId & (kFlagEff | kFlagRtr | kFlagErr)) && canToMsg(f.canId, f.data, f.dlc, m))
            return true;
        // TX echo, error frame or foreign traffic: keep reading until the deadline
        if (left.count() <= 0) return false;
    }
}

} // namespace fochost
