#pragma once
#include "fochost/Transport.hpp"

#include <atomic>
#include <chrono>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace fochost {

struct ParamState {
    float value = 0.0f;
    bool valid = false;      ///< value was read from / confirmed by the drive
    bool pending = false;    ///< request sent, no answer yet
    uint16_t status = FOCP_PS_OK;
};

struct DriveStatus {
    bool valid = false;
    uint16_t state = 0, mode = 0, sensor = 0, faults = 0;
    float udc = 0.0f, temp = 0.0f;
    std::chrono::steady_clock::time_point stamp{};
};

struct DriveInfo {
    bool valid = false;
    int protocol = 0, fwMajor = 0, fwMinor = 0, board = 0, numParams = 0, numSignals = 0;
};

/// What a log entry is about, so a GUI can phrase (and translate) it itself.
enum class LogKind {
    Text,              ///< free text only
    Connected,         ///< code = node, detail = transport name
    Disconnected,
    CalibStarted,      ///< code = focp_calib_t
    CalibDone,         ///< code = focp_calib_t (0 if unknown)
    CalibFailed,       ///< code = focp_calib_t (0 if unknown)
    State,             ///< code = new focp_state_t
    FaultSet,          ///< code = one fault bit
    FaultCleared,      ///< code = one fault bit
    ParamsSaved,
    ParamRejected,     ///< code = focp_param_status_t, detail = parameter name
    UnknownParam,      ///< code = parameter id
    VersionMismatch,
};

struct LogEntry {
    double t;          ///< seconds since connect
    int level;         ///< 0 info, 1 warning, 2 error
    std::string text;  ///< German text (developer tool)
    LogKind kind = LogKind::Text;
    int code = 0;
    std::string detail;
    uint64_t seq = 0;  ///< increases with every entry, survives the 2000-entry limit
};

/// Ring buffer for streamed telemetry. Layout fits ImPlot's (data, count, offset) API.
struct ScopeBuffer {
    size_t capacity = 0;
    size_t count = 0;
    size_t head = 0;                       ///< index of the oldest sample once full
    std::vector<int> signals;              ///< signal index per slot, -1 = empty
    std::vector<double> t;
    std::vector<std::vector<double>> y;    ///< [slot][sample]

    void reset(const std::vector<int>& sig, size_t cap);
    void push(double time, const float* values);
    size_t offset() const { return count < capacity ? 0 : head; }
    double tLast() const;
    /// Copy the samples with t >= tFrom into contiguous arrays (oldest first).
    /// Keeps at most maxPoints by striding. Returns number of points.
    size_t copyWindow(double tFrom, size_t maxPoints, std::vector<double>& tOut,
                      std::vector<std::vector<double>>& yOut) const;
};

/// Host side of one drive: protocol handling, parameter cache, telemetry assembly.
/// All public methods are thread-safe; a background thread does the receiving.
class Device {
public:
    Device();
    ~Device();

    bool connect(const TransportConfig& cfg, uint16_t node, std::string& error);
    bool connect(std::unique_ptr<Transport> t, uint16_t node, std::string& error);
    void disconnect();
    bool connected() const { return running_; }
    std::string transportName() const;

    // commands
    void enable();
    void disable();
    void ackFault();
    void setReference(float value);
    void calibrate(focp_calib_t what);
    void saveParams();
    void loadDefaults();
    void requestInfo();

    // parameters (index = focp_param_idx_t)
    void readParam(int idx);
    void readAllParams();
    void writeParam(int idx, float value);
    std::vector<ParamState> params() const;
    float paramValue(int idx) const;

    // telemetry
    void configureScope(const std::vector<int>& signalIdx, uint16_t divider, size_t capacity = 200000);
    void runScope(bool run);
    bool scopeRunning() const { return scopeRun_; }
    void clearScope();
    double scopeSampleTime() const;
    /// Telemetry base rate (divider 1): comm.telem_f if the drive reports it, else inv.f_pwm.
    double telemetryBaseHz() const;
    uint64_t lostSamples() const { return lost_; }
    template <class F> void withScope(F&& f)
    {
        std::lock_guard<std::mutex> lk(scopeMx_);
        f(static_cast<const ScopeBuffer&>(scope_));
    }
    bool exportCsv(const std::string& path, std::string& error);

    DriveStatus status() const;
    DriveInfo info() const;
    std::vector<LogEntry> log() const;
    /// Only entries with LogEntry::seq > seq (cheap polling).
    std::vector<LogEntry> logSince(uint64_t seq) const;
    void clearLog();
    double connectedTime() const;
    uint64_t rxMessages() const { return rxCount_; }

    std::atomic<bool> heartbeat{true};

private:
    void rxLoop();
    void handle(const focp_msg_t& m);
    void handleTelem(const focp_msg_t& m);
    void send(const focp_msg_t& m);
    void addLog(int level, std::string text, LogKind kind = LogKind::Text, int code = 0, std::string detail = {});
    void retryParams();

    std::unique_ptr<Transport> tr_;
    uint16_t node_ = 1;
    std::atomic<bool> running_{false};
    std::thread rx_;
    std::chrono::steady_clock::time_point t0_{};

    mutable std::mutex mx_;                 // params, status, info, log
    std::vector<ParamState> params_;
    DriveStatus status_;
    DriveInfo info_;
    std::deque<LogEntry> log_;
    uint64_t logSeq_ = 0;
    // CAN frames can get lost (adapter/drive buffers). Unanswered parameter requests are
    // repeated a few times; see retryParams().
    struct ParamRequest { std::chrono::steady_clock::time_point sent{}; int tries = 0; };
    std::vector<ParamRequest> paramReq_;

    mutable std::mutex scopeMx_;
    ScopeBuffer scope_;
    uint16_t scopeDiv_ = 20;
    std::atomic<bool> scopeRun_{false};
    int framesPerSample_ = 0;
    int curSeq_ = -1, lastSeq_ = -1, framesGot_ = 0;
    uint64_t sampleCounter_ = 0;
    std::atomic<uint64_t> lost_{0};
    std::vector<float> pending_;

    std::atomic<uint64_t> rxCount_{0};
};

} // namespace fochost
