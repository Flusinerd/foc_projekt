// gs_usb USB access via WinUSB (Windows). Other platforms: stubs (Linux uses SocketCAN).
#include "fochost/GsUsb.hpp"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <setupapi.h>
#include <winusb.h>

#include <array>
#include <cstring>
#endif

namespace fochost {
namespace gsusb {

#ifdef _WIN32
namespace {

// DeviceInterfaceGUID announced by the candleLight firmware (MS OS descriptor).
const GUID kGsUsbGuid = {0xc15b4308, 0x04d3, 0x11e6, {0xb3, 0xea, 0x60, 0x57, 0x18, 0x9e, 0x64, 0x43}};

std::string narrow(const wchar_t* w)
{
    const int n = ::WideCharToMultiByte(CP_UTF8, 0, w, -1, nullptr, 0, nullptr, nullptr);
    std::string s(n > 0 ? size_t(n - 1) : 0, '\0');
    if (n > 1) ::WideCharToMultiByte(CP_UTF8, 0, w, -1, &s[0], n, nullptr, nullptr);
    return s;
}

std::wstring widen(const std::string& s)
{
    const int n = ::MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    std::wstring w(n > 0 ? size_t(n - 1) : 0, L'\0');
    if (n > 1) ::MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &w[0], n);
    return w;
}

std::string lastError(const char* what)
{
    return std::string(what) + " (Windows-Fehler " + std::to_string(::GetLastError()) + ")";
}

class WinUsbIo final : public UsbIo {
public:
    explicit WinUsbIo(std::string path) : path_(std::move(path)) {}
    ~WinUsbIo() override { close(); }

    bool open(std::string& error) override
    {
        if (path_.empty()) {
            const auto devs = listDevices();
            if (devs.empty()) { error = "Kein CANable/candleLight-Adapter gefunden (USB eingesteckt?)"; return false; }
            path_ = devs.front().path;
        }
        file_ = ::CreateFileW(widen(path_).c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                              nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, nullptr);
        if (file_ == INVALID_HANDLE_VALUE) {
            error = lastError("Adapter laesst sich nicht oeffnen (von einem anderen Programm belegt?)");
            return false;
        }
        if (!::WinUsb_Initialize(file_, &usb_)) { error = lastError("WinUSB-Initialisierung fehlgeschlagen"); close(); return false; }

        USB_INTERFACE_DESCRIPTOR ifd{};
        if (!::WinUsb_QueryInterfaceSettings(usb_, 0, &ifd)) { error = lastError("USB-Schnittstelle unlesbar"); close(); return false; }
        for (UCHAR i = 0; i < ifd.bNumEndpoints; ++i) {
            WINUSB_PIPE_INFORMATION p{};
            if (!::WinUsb_QueryPipe(usb_, 0, i, &p) || p.PipeType != UsbdPipeTypeBulk) continue;
            if (USB_ENDPOINT_DIRECTION_IN(p.PipeId)) { pipeIn_ = p.PipeId; maxPacketIn_ = p.MaximumPacketSize; }
            else pipeOut_ = p.PipeId;
        }
        if (!pipeIn_ || !pipeOut_) { error = "gs_usb: Bulk-Endpunkte fehlen"; close(); return false; }

        ULONG timeout = 200;   // ms, a stuck bus must not block the sender forever
        ::WinUsb_SetPipePolicy(usb_, pipeOut_, PIPE_TRANSFER_TIMEOUT, sizeof(timeout), &timeout);
        ::WinUsb_FlushPipe(usb_, pipeIn_);

        for (auto& r : reads_) {
            r.ov = OVERLAPPED{};
            r.ov.hEvent = ::CreateEventW(nullptr, TRUE, FALSE, nullptr);
            if (!r.ov.hEvent || !post(r)) { error = lastError("USB-Lesepuffer nicht anlegbar"); close(); return false; }
        }
        next_ = 0;
        return true;
    }

    void close() override
    {
        if (usb_) {
            if (pipeIn_) ::WinUsb_AbortPipe(usb_, pipeIn_);
            if (pipeOut_) ::WinUsb_AbortPipe(usb_, pipeOut_);
            for (auto& r : reads_) {
                if (r.posted) {
                    ULONG n = 0;
                    ::WinUsb_GetOverlappedResult(usb_, &r.ov, &n, TRUE);
                    r.posted = false;
                }
            }
            ::WinUsb_Free(usb_);
            usb_ = nullptr;
        }
        for (auto& r : reads_) {
            if (r.ov.hEvent) ::CloseHandle(r.ov.hEvent);
            r.ov.hEvent = nullptr;
        }
        if (file_ != INVALID_HANDLE_VALUE) ::CloseHandle(file_);
        file_ = INVALID_HANDLE_VALUE;
        pipeIn_ = pipeOut_ = 0;
    }

    bool controlOut(uint8_t request, uint16_t value, const uint8_t* data, uint16_t len) override
    {
        return control(0x41, request, value, const_cast<uint8_t*>(data), len);
    }

    bool controlIn(uint8_t request, uint16_t value, uint8_t* data, uint16_t len) override
    {
        return control(0xC1, request, value, data, len);
    }

    bool bulkWrite(const uint8_t* data, size_t len) override
    {
        ULONG n = 0;
        return ::WinUsb_WritePipe(usb_, pipeOut_, const_cast<PUCHAR>(data), ULONG(len), &n, nullptr) && n == len;
    }

    int bulkRead(uint8_t* data, size_t len, int timeoutMs) override
    {
        // Several reads stay posted so the adapter can deliver frames while we process.
        Read& r = reads_[next_];
        if (!r.posted) return -1;
        if (::WaitForSingleObject(r.ov.hEvent, DWORD(timeoutMs < 0 ? 0 : timeoutMs)) != WAIT_OBJECT_0) return 0;
        ULONG n = 0;
        const bool ok = ::WinUsb_GetOverlappedResult(usb_, &r.ov, &n, FALSE) != FALSE;
        r.posted = false;
        if (!ok) return -1;
        const size_t c = n < len ? n : len;
        std::memcpy(data, r.buf.data(), c);
        if (!post(r)) return -1;
        next_ = (next_ + 1) % reads_.size();
        return int(c);
    }

private:
    struct Read {
        OVERLAPPED ov{};
        std::array<UCHAR, 64> buf{};
        bool posted = false;
    };

    bool post(Read& r)
    {
        ::ResetEvent(r.ov.hEvent);
        const ULONG size = maxPacketIn_ && maxPacketIn_ <= r.buf.size() ? maxPacketIn_ : ULONG(r.buf.size());
        if (!::WinUsb_ReadPipe(usb_, pipeIn_, r.buf.data(), size, nullptr, &r.ov) && ::GetLastError() != ERROR_IO_PENDING)
            return false;
        r.posted = true;
        return true;
    }

    bool control(UCHAR type, uint8_t request, uint16_t value, uint8_t* data, uint16_t len)
    {
        WINUSB_SETUP_PACKET s{};
        s.RequestType = type;
        s.Request = request;
        s.Value = value;
        s.Index = 0;   // interface 0
        s.Length = len;
        ULONG n = 0;
        return ::WinUsb_ControlTransfer(usb_, s, data, len, &n, nullptr) && n == len;
    }

    std::string path_;
    HANDLE file_ = INVALID_HANDLE_VALUE;
    WINUSB_INTERFACE_HANDLE usb_ = nullptr;
    UCHAR pipeIn_ = 0, pipeOut_ = 0;
    ULONG maxPacketIn_ = 0;
    std::array<Read, 8> reads_{};
    size_t next_ = 0;
};

} // namespace

std::vector<DeviceInfo> listDevices()
{
    std::vector<DeviceInfo> out;
    HDEVINFO set = ::SetupDiGetClassDevsW(&kGsUsbGuid, nullptr, nullptr, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (set == INVALID_HANDLE_VALUE) return out;
    SP_DEVICE_INTERFACE_DATA ifd{};
    ifd.cbSize = sizeof(ifd);
    for (DWORD i = 0; ::SetupDiEnumDeviceInterfaces(set, nullptr, &kGsUsbGuid, i, &ifd); ++i) {
        DWORD need = 0;
        ::SetupDiGetDeviceInterfaceDetailW(set, &ifd, nullptr, 0, &need, nullptr);
        if (!need) continue;
        std::vector<BYTE> buf(need);
        auto* det = reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W*>(buf.data());
        det->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);
        SP_DEVINFO_DATA dev{};
        dev.cbSize = sizeof(dev);
        if (!::SetupDiGetDeviceInterfaceDetailW(set, &ifd, det, need, nullptr, &dev)) continue;
        DeviceInfo info;
        info.path = narrow(det->DevicePath);
        wchar_t desc[256] = {};
        if (::SetupDiGetDeviceRegistryPropertyW(set, &dev, SPDRP_FRIENDLYNAME, nullptr, reinterpret_cast<PBYTE>(desc),
                                                sizeof(desc) - sizeof(wchar_t), nullptr) ||
            ::SetupDiGetDeviceRegistryPropertyW(set, &dev, SPDRP_DEVICEDESC, nullptr, reinterpret_cast<PBYTE>(desc),
                                                sizeof(desc) - sizeof(wchar_t), nullptr))
            info.description = narrow(desc);
        else
            info.description = "gs_usb";
        out.push_back(std::move(info));
    }
    ::SetupDiDestroyDeviceInfoList(set);
    return out;
}

std::unique_ptr<UsbIo> makeWinUsbIo(const std::string& path) { return std::make_unique<WinUsbIo>(path); }

#else

std::vector<DeviceInfo> listDevices() { return {}; }
std::unique_ptr<UsbIo> makeWinUsbIo(const std::string&) { return nullptr; }

#endif

} // namespace gsusb
} // namespace fochost
