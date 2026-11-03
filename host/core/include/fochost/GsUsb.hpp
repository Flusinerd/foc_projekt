#pragma once
// gs_usb (candleLight firmware, e.g. CANable): protocol helpers and transport.
//
// Windows: WinUSB (driver bound automatically via MS OS descriptors, no install).
// Linux:   the kernel driver gs_usb exposes the adapter as SocketCAN (can0) -> use SocketCAN.
// Implemented from the gs_usb USB interface description (vendor control requests +
// 20-byte bulk frames, little endian); no third-party code.
#include "fochost/Transport.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace fochost {
namespace gsusb {

enum Request : uint8_t {
    kHostFormat = 0,
    kBitTiming = 1,
    kMode = 2,
    kBtConst = 4,
    kDeviceConfig = 5,
};
enum Mode : uint32_t { kModeReset = 0, kModeStart = 1 };

constexpr uint32_t kEchoRx = 0xFFFFFFFFu;     ///< echo_id of frames received from the bus
constexpr uint32_t kFlagEff = 0x80000000u;    ///< 29-bit identifier
constexpr uint32_t kFlagRtr = 0x40000000u;
constexpr uint32_t kFlagErr = 0x20000000u;    ///< error frame
constexpr size_t kFrameSize = 20;             ///< classic CAN frame without timestamp

struct BtConst {
    uint32_t feature = 0, fclk = 0;
    uint32_t tseg1Min = 0, tseg1Max = 0, tseg2Min = 0, tseg2Max = 0;
    uint32_t sjwMax = 0, brpMin = 0, brpMax = 0, brpInc = 1;
};
constexpr size_t kBtConstSize = 40;

struct BitTiming {
    uint32_t propSeg = 0, phaseSeg1 = 0, phaseSeg2 = 0, sjw = 0, brp = 0;
    uint32_t tq() const { return 1 + propSeg + phaseSeg1 + phaseSeg2; }
};
constexpr size_t kBitTimingSize = 20;

struct HostFrame {
    uint32_t echoId = kEchoRx;
    uint32_t canId = 0;           ///< incl. kFlag* bits
    uint8_t dlc = 0, channel = 0, flags = 0;
    uint8_t data[8] = {};
};

/// Exact bit timing for `bitrate` with sample point as close as possible to 87.5 %.
bool calcBitTiming(const BtConst& c, int bitrate, BitTiming& out);

void packFrame(const HostFrame& f, uint8_t* buf);                    ///< writes kFrameSize bytes
bool unpackFrame(const uint8_t* buf, size_t len, HostFrame& f);
void packBitTiming(const BitTiming& t, uint8_t* buf);                ///< writes kBitTimingSize bytes
BtConst unpackBtConst(const uint8_t* buf);                           ///< reads kBtConstSize bytes

/// Raw USB access used by GsUsbTransport (WinUSB on Windows, simulated device in tests).
/// Control requests are vendor requests to interface 0. bulkRead/bulkWrite may be called
/// concurrently from one reader and one writer thread.
class UsbIo {
public:
    virtual ~UsbIo() = default;
    virtual bool open(std::string& error) = 0;
    virtual void close() = 0;
    virtual bool controlOut(uint8_t request, uint16_t value, const uint8_t* data, uint16_t len) = 0;
    virtual bool controlIn(uint8_t request, uint16_t value, uint8_t* data, uint16_t len) = 0;
    virtual bool bulkWrite(const uint8_t* data, size_t len) = 0;
    /// Bytes read (one USB transfer), 0 on timeout, -1 on error.
    virtual int bulkRead(uint8_t* data, size_t len, int timeoutMs) = 0;
};

struct DeviceInfo {
    std::string path;          ///< pass as TransportConfig::channel
    std::string description;   ///< e.g. "canable gs_usb"
};

/// Connected gs_usb adapters (Windows only; empty elsewhere).
std::vector<DeviceInfo> listDevices();
/// WinUSB access to `path` (empty = first adapter). nullptr if not supported on this platform.
std::unique_ptr<UsbIo> makeWinUsbIo(const std::string& path);

} // namespace gsusb

class GsUsbTransport final : public Transport {
public:
    GsUsbTransport(std::unique_ptr<gsusb::UsbIo> io, int bitrate);
    ~GsUsbTransport() override;

    std::string name() const override;
    bool open(std::string& error) override;
    void close() override;
    bool isOpen() const override { return open_; }
    bool send(const focp_msg_t& m) override;
    bool receive(focp_msg_t& m, int timeoutMs) override;

    const gsusb::BitTiming& bitTiming() const { return timing_; }
    uint32_t canClock() const { return fclk_; }

private:
    std::unique_ptr<gsusb::UsbIo> io_;
    int bitrate_;
    bool open_ = false;
    gsusb::BitTiming timing_;
    uint32_t fclk_ = 0;
    uint32_t nextEcho_ = 0;
};

} // namespace fochost
