// CAN transports: SocketCAN (Linux) and PEAK PCAN-Basic (Windows/Linux, loaded at runtime).
// gs_usb (CANable/candleLight) lives in GsUsbTransport.cpp / GsUsbWinUsb.cpp.
#include "fochost/GsUsb.hpp"
#include "fochost/SimTransport.hpp"
#include "fochost/Transport.hpp"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <thread>

#ifdef __linux__
#include <dlfcn.h>
#include <linux/can.h>
#include <linux/can/raw.h>
#include <net/if.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace fochost {

// ------------------------------------------------------------------------------------------
bool canToMsg(uint32_t canId, const uint8_t* data, uint8_t dlc, focp_msg_t& m)
{
    if (canId > 0x7FF) return false;                        // FOCP uses 11-bit IDs only
    const uint16_t fn = FOCP_CAN_FN(canId);
    if (fn < FOCP_FN_CMD || fn > FOCP_FN_INFO) return false; // foreign traffic (e.g. CarpeDiem 0x1xx)
    m.fn = fn;
    m.node = FOCP_CAN_NODE(canId);
    m.len = dlc > 8 ? 8 : dlc;
    for (int i = 0; i < 8; ++i) m.data[i] = i < m.len ? data[i] : 0;
    return true;
}

void msgToCan(const focp_msg_t& m, uint32_t& canId, uint8_t* data, uint8_t& dlc)
{
    canId = FOCP_CAN_ID(m.fn, m.node);
    dlc = static_cast<uint8_t>(m.len > 8 ? 8 : m.len);
    for (int i = 0; i < 8; ++i) data[i] = static_cast<uint8_t>(m.data[i] & 0xFF);
}

// ------------------------------------------------------------------------------------------
#ifdef __linux__
class SocketCanTransport final : public Transport {
public:
    explicit SocketCanTransport(std::string ifname) : if_(std::move(ifname)) {}
    ~SocketCanTransport() override { close(); }
    std::string name() const override { return "SocketCAN " + if_; }
    bool isOpen() const override { return fd_ >= 0; }

    bool open(std::string& error) override
    {
        fd_ = ::socket(PF_CAN, SOCK_RAW, CAN_RAW);
        if (fd_ < 0) { error = "socket(PF_CAN) fehlgeschlagen"; return false; }
        ifreq ifr{};
        std::strncpy(ifr.ifr_name, if_.c_str(), IFNAMSIZ - 1);
        if (::ioctl(fd_, SIOCGIFINDEX, &ifr) < 0) { error = "Interface " + if_ + " nicht gefunden"; close(); return false; }
        sockaddr_can addr{};
        addr.can_family = AF_CAN;
        addr.can_ifindex = ifr.ifr_ifindex;
        if (::bind(fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) { error = "bind fehlgeschlagen"; close(); return false; }
        return true;
    }

    void close() override
    {
        if (fd_ >= 0) ::close(fd_);
        fd_ = -1;
    }

    bool send(const focp_msg_t& m) override
    {
        can_frame f{};
        uint8_t dlc = 0;
        uint32_t id = 0;
        msgToCan(m, id, f.data, dlc);
        f.can_id = id;
        f.can_dlc = dlc;
        std::lock_guard<std::mutex> lk(txMx_);
        return ::write(fd_, &f, sizeof(f)) == static_cast<ssize_t>(sizeof(f));
    }

    bool receive(focp_msg_t& m, int timeoutMs) override
    {
        pollfd p{fd_, POLLIN, 0};
        while (::poll(&p, 1, timeoutMs) > 0) {
            can_frame f{};
            if (::read(fd_, &f, sizeof(f)) != static_cast<ssize_t>(sizeof(f))) return false;
            if (f.can_id & (CAN_EFF_FLAG | CAN_RTR_FLAG | CAN_ERR_FLAG)) continue;
            if (canToMsg(f.can_id & CAN_SFF_MASK, f.data, f.can_dlc, m)) return true;
            timeoutMs = 0;
        }
        return false;
    }

private:
    std::string if_;
    int fd_ = -1;
    std::mutex txMx_;
};
#endif

// ------------------------------------------------------------------------------------------
// PCAN-Basic, loaded dynamically so the tool builds and runs without the PEAK SDK.
// Types follow PCANBasic.h. NOTE: not yet tested against real hardware.
class PcanTransport final : public Transport {
#ifdef _WIN32
#define FOCP_PCAN_CALL __stdcall
#else
#define FOCP_PCAN_CALL
#endif
    struct TPCANMsg { uint32_t ID; uint8_t MSGTYPE; uint8_t LEN; uint8_t DATA[8]; };
    struct TPCANTimestamp { uint32_t millis; uint16_t millis_overflow; uint16_t micros; };
    using FnInit = uint32_t(FOCP_PCAN_CALL*)(uint16_t ch, uint16_t btr, uint8_t hwType, uint32_t ioPort, uint16_t irq);
    using FnUninit = uint32_t(FOCP_PCAN_CALL*)(uint16_t ch);
    using FnRead = uint32_t(FOCP_PCAN_CALL*)(uint16_t ch, TPCANMsg* msg, TPCANTimestamp* ts);
    using FnWrite = uint32_t(FOCP_PCAN_CALL*)(uint16_t ch, TPCANMsg* msg);

    static constexpr uint32_t kOk = 0x00000;
    static constexpr uint32_t kRxEmpty = 0x00020;
    static constexpr uint8_t kMsgStandard = 0x00;

public:
    PcanTransport(std::string channel, int bitrate) : chName_(std::move(channel)), bitrate_(bitrate) {}
    ~PcanTransport() override { close(); }
    std::string name() const override { return "PCAN " + chName_; }
    bool isOpen() const override { return open_; }

    bool open(std::string& error) override
    {
        if (!loadLib(error)) return false;
        int usb = 1;
        if (chName_.rfind("USB", 0) == 0) usb = std::atoi(chName_.c_str() + 3);
        if (usb < 1 || usb > 16) { error = "Kanal muss USB1..USB16 sein"; return false; }
        ch_ = static_cast<uint16_t>(0x50 + usb);          // PCAN_USBBUS1 = 0x51
        const uint32_t st = init_(ch_, btrCode(bitrate_), 0, 0, 0);
        if (st != kOk) { error = "CAN_Initialize Fehler 0x" + hex(st); return false; }
        open_ = true;
        return true;
    }

    void close() override
    {
        if (open_ && uninit_) uninit_(ch_);
        open_ = false;
    }

    bool send(const focp_msg_t& m) override
    {
        TPCANMsg f{};
        uint32_t id = 0;
        msgToCan(m, id, f.DATA, f.LEN);
        f.ID = id;
        f.MSGTYPE = kMsgStandard;
        std::lock_guard<std::mutex> lk(mx_);
        return write_(ch_, &f) == kOk;
    }

    bool receive(focp_msg_t& m, int timeoutMs) override
    {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
        do {
            TPCANMsg f{};
            TPCANTimestamp ts{};
            uint32_t st;
            {
                std::lock_guard<std::mutex> lk(mx_);
                st = read_(ch_, &f, &ts);
            }
            if (st == kOk) {
                if (f.MSGTYPE == kMsgStandard && canToMsg(f.ID, f.DATA, f.LEN, m)) return true;
                continue;
            }
            if (st != kRxEmpty) return false;
            std::this_thread::sleep_for(std::chrono::microseconds(500));
        } while (std::chrono::steady_clock::now() < deadline);
        return false;
    }

private:
    static uint16_t btrCode(int bitrate)
    {
        switch (bitrate) {
        case 1000000: return 0x0014;
        case 800000:  return 0x0016;
        case 250000:  return 0x011C;
        case 125000:  return 0x031C;
        default:      return 0x001C;   // 500 kbit/s
        }
    }

    static std::string hex(uint32_t v)
    {
        char b[16];
        std::snprintf(b, sizeof(b), "%X", v);
        return b;
    }

    bool loadLib(std::string& error)
    {
        if (init_) return true;
#ifdef _WIN32
        HMODULE h = ::LoadLibraryA("PCANBasic.dll");
        if (!h) { error = "PCANBasic.dll nicht gefunden (PEAK-Treiber installieren)"; return false; }
        auto sym = [h](const char* n) { return reinterpret_cast<void*>(::GetProcAddress(h, n)); };
#elif defined(__linux__)
        void* h = ::dlopen("libpcanbasic.so", RTLD_NOW);
        if (!h) { error = "libpcanbasic.so nicht gefunden (PEAK PCAN-Basic fuer Linux installieren oder SocketCAN nutzen)"; return false; }
        auto sym = [h](const char* n) { return ::dlsym(h, n); };
#else
        error = "PCAN auf dieser Plattform nicht unterstuetzt";
        return false;
        auto sym = [](const char*) -> void* { return nullptr; };
#endif
        init_ = reinterpret_cast<FnInit>(sym("CAN_Initialize"));
        uninit_ = reinterpret_cast<FnUninit>(sym("CAN_Uninitialize"));
        read_ = reinterpret_cast<FnRead>(sym("CAN_Read"));
        write_ = reinterpret_cast<FnWrite>(sym("CAN_Write"));
        if (!init_ || !uninit_ || !read_ || !write_) { init_ = nullptr; error = "PCAN-Basic: Funktionen fehlen"; return false; }
        return true;
    }

    std::string chName_;
    int bitrate_;
    uint16_t ch_ = 0x51;
    bool open_ = false;
    std::mutex mx_;
    FnInit init_ = nullptr;
    FnUninit uninit_ = nullptr;
    FnRead read_ = nullptr;
    FnWrite write_ = nullptr;
};

// ------------------------------------------------------------------------------------------
std::unique_ptr<Transport> makeTransport(const TransportConfig& cfg)
{
    switch (cfg.kind) {
    case TransportKind::Simulation: return std::make_unique<SimTransport>();
#ifdef __linux__
    case TransportKind::SocketCAN:  return std::make_unique<SocketCanTransport>(cfg.channel);
#endif
    case TransportKind::PCAN:       return std::make_unique<PcanTransport>(cfg.channel, cfg.bitrate);
#ifdef _WIN32
    case TransportKind::GsUsb:      return std::make_unique<GsUsbTransport>(gsusb::makeWinUsbIo(cfg.channel), cfg.bitrate);
#endif
    default:                        return nullptr;
    }
}

std::vector<TransportKind> availableTransports()
{
    std::vector<TransportKind> v{TransportKind::Simulation};
#ifdef _WIN32
    v.push_back(TransportKind::GsUsb);   // Linux: kernel driver gs_usb -> SocketCAN
#endif
#ifdef __linux__
    v.push_back(TransportKind::SocketCAN);
#endif
    v.push_back(TransportKind::PCAN);
    return v;
}

const char* transportName(TransportKind k)
{
    switch (k) {
    case TransportKind::Simulation: return "Simulation (virtueller Antrieb)";
    case TransportKind::SocketCAN:  return "SocketCAN";
    case TransportKind::PCAN:       return "PEAK PCAN-USB";
    case TransportKind::GsUsb:      return "CANable / candleLight (gs_usb)";
    }
    return "?";
}

} // namespace fochost
