#include "fochost/Device.hpp"

#include <cstdio>
#include <fstream>

namespace fochost {

// ------------------------------------------------------------------------------------------
void ScopeBuffer::reset(const std::vector<int>& sig, size_t cap)
{
    signals = sig;
    capacity = cap;
    count = 0;
    head = 0;
    t.assign(cap, 0.0);
    y.assign(sig.size(), std::vector<double>(cap, 0.0));
}

void ScopeBuffer::push(double time, const float* values)
{
    if (capacity == 0) return;
    size_t idx;
    if (count < capacity) { idx = count++; }
    else { idx = head; head = (head + 1) % capacity; }
    t[idx] = time;
    for (size_t s = 0; s < y.size(); ++s) y[s][idx] = values[s];
}

size_t ScopeBuffer::copyWindow(double tFrom, size_t maxPoints, std::vector<double>& tOut,
                               std::vector<std::vector<double>>& yOut) const
{
    tOut.clear();
    yOut.assign(y.size(), {});
    if (count == 0 || maxPoints == 0) return 0;
    const size_t off = offset();
    auto at = [&](size_t i) { return (off + i) % capacity; };
    // binary search for the first sample >= tFrom (time is monotonic in logical order)
    size_t lo = 0, hi = count;
    while (lo < hi) {
        const size_t mid = (lo + hi) / 2;
        if (t[at(mid)] < tFrom) lo = mid + 1; else hi = mid;
    }
    const size_t n = count - lo;
    const size_t stride = (n + maxPoints - 1) / maxPoints;
    const size_t m = (n + stride - 1) / stride;
    tOut.reserve(m);
    for (auto& v : yOut) v.reserve(m);
    for (size_t i = lo; i < count; i += stride) {
        const size_t k = at(i);
        tOut.push_back(t[k]);
        for (size_t s = 0; s < y.size(); ++s) yOut[s].push_back(y[s][k]);
    }
    return tOut.size();
}

double ScopeBuffer::tLast() const
{
    if (count == 0) return 0.0;
    const size_t last = (count < capacity) ? count - 1 : (head + capacity - 1) % capacity;
    return t[last];
}

// ------------------------------------------------------------------------------------------
Device::Device() : params_(FOCP_NUM_PARAMS)
{
    for (int i = 0; i < FOCP_NUM_PARAMS; ++i) params_[i].value = focp_param_table[i].def;
}

Device::~Device() { disconnect(); }

bool Device::connect(const TransportConfig& cfg, uint16_t node, std::string& error)
{
    auto t = makeTransport(cfg);
    if (!t) { error = "Transport auf dieser Plattform nicht verfuegbar"; return false; }
    return connect(std::move(t), node, error);
}

bool Device::connect(std::unique_ptr<Transport> t, uint16_t node, std::string& error)
{
    disconnect();
    if (!t->open(error)) return false;
    tr_ = std::move(t);
    node_ = node;
    t0_ = std::chrono::steady_clock::now();
    {
        std::lock_guard<std::mutex> lk(mx_);
        status_ = DriveStatus{};
        info_ = DriveInfo{};
        for (auto& p : params_) { p.valid = false; p.pending = false; }
    }
    running_ = true;
    rx_ = std::thread([this] { rxLoop(); });
    addLog(0, "Verbunden ueber " + tr_->name() + ", Knoten " + std::to_string(node));
    requestInfo();
    readAllParams();
    return true;
}

void Device::disconnect()
{
    if (!running_) return;
    if (scopeRun_) runScope(false);
    running_ = false;
    if (rx_.joinable()) rx_.join();
    tr_->close();
    addLog(0, "Verbindung getrennt");
    tr_.reset();
}

std::string Device::transportName() const { return tr_ ? tr_->name() : std::string("-"); }

double Device::connectedTime() const
{
    return std::chrono::duration<double>(std::chrono::steady_clock::now() - t0_).count();
}

void Device::send(const focp_msg_t& m)
{
    if (running_ && tr_) tr_->send(m);
}

// --- commands ----------------------------------------------------------------------------
void Device::enable()         { focp_msg_t m; focp_build_cmd(&m, node_, FOCP_CMD_ENABLE); send(m); }
void Device::disable()        { focp_msg_t m; focp_build_cmd(&m, node_, FOCP_CMD_DISABLE); send(m); }
void Device::ackFault()       { focp_msg_t m; focp_build_cmd(&m, node_, FOCP_CMD_ACK_FAULT); send(m); }
void Device::saveParams()     { focp_msg_t m; focp_build_cmd(&m, node_, FOCP_CMD_SAVE_PARAMS); send(m); }
void Device::requestInfo()    { focp_msg_t m; focp_build_cmd(&m, node_, FOCP_CMD_GET_INFO); send(m); }
void Device::setReference(float v) { focp_msg_t m; focp_build_cmd_f32(&m, node_, FOCP_CMD_SET_REF, v); send(m); }

void Device::calibrate(focp_calib_t what)
{
    focp_msg_t m;
    focp_build_cmd_u8(&m, node_, FOCP_CMD_CALIBRATE, static_cast<uint16_t>(what));
    send(m);
    addLog(0, "Kalibrierung gestartet");
}

void Device::loadDefaults()
{
    focp_msg_t m;
    focp_build_cmd(&m, node_, FOCP_CMD_LOAD_DEFAULTS);
    send(m);
    readAllParams();
}

// --- parameters --------------------------------------------------------------------------
void Device::readParam(int idx)
{
    if (idx < 0 || idx >= FOCP_NUM_PARAMS) return;
    { std::lock_guard<std::mutex> lk(mx_); params_[idx].pending = true; }
    focp_msg_t m;
    focp_build_param_req(&m, node_, FOCP_OP_READ, focp_param_table[idx].id, 0.0f);
    send(m);
}

void Device::readAllParams()
{
    for (int i = 0; i < FOCP_NUM_PARAMS; ++i) readParam(i);
}

void Device::writeParam(int idx, float value)
{
    if (idx < 0 || idx >= FOCP_NUM_PARAMS) return;
    { std::lock_guard<std::mutex> lk(mx_); params_[idx].pending = true; }
    focp_msg_t m;
    focp_build_param_req(&m, node_, FOCP_OP_WRITE, focp_param_table[idx].id, value);
    send(m);
}

std::vector<ParamState> Device::params() const
{
    std::lock_guard<std::mutex> lk(mx_);
    return params_;
}

float Device::paramValue(int idx) const
{
    std::lock_guard<std::mutex> lk(mx_);
    return params_[idx].value;
}

DriveStatus Device::status() const { std::lock_guard<std::mutex> lk(mx_); return status_; }
DriveInfo Device::info() const     { std::lock_guard<std::mutex> lk(mx_); return info_; }

std::vector<LogEntry> Device::log() const
{
    std::lock_guard<std::mutex> lk(mx_);
    return {log_.begin(), log_.end()};
}

void Device::clearLog() { std::lock_guard<std::mutex> lk(mx_); log_.clear(); }

void Device::addLog(int level, std::string text)
{
    std::lock_guard<std::mutex> lk(mx_);
    log_.push_back({connectedTime(), level, std::move(text)});
    while (log_.size() > 2000) log_.pop_front();
}

// --- telemetry ---------------------------------------------------------------------------
void Device::configureScope(const std::vector<int>& signalIdx, uint16_t divider, size_t capacity)
{
    const bool wasRunning = scopeRun_;
    if (wasRunning) runScope(false);

    std::vector<int> slots(signalIdx.begin(), signalIdx.end());
    if (slots.size() > FOCP_TELEM_SLOTS) slots.resize(FOCP_TELEM_SLOTS);
    for (uint16_t s = 0; s < FOCP_TELEM_SLOTS; ++s) {
        const int sig = s < slots.size() ? slots[s] : -1;
        focp_msg_t m;
        focp_build_cmd_u8u8(&m, node_, FOCP_CMD_TELEM_SLOT, s,
                            sig >= 0 ? focp_signal_table[sig].id : 0);
        send(m);
    }
    focp_msg_t m;
    focp_build_cmd_u16(&m, node_, FOCP_CMD_TELEM_RATE, divider ? divider : 1);
    send(m);

    {
        std::lock_guard<std::mutex> lk(scopeMx_);
        scopeDiv_ = divider ? divider : 1;
        scope_.reset(slots, capacity);
        int last = 0;
        for (size_t i = 0; i < slots.size(); ++i) if (slots[i] >= 0) last = int(i) + 1;
        framesPerSample_ = (last + FOCP_TELEM_PER_FRAME - 1) / FOCP_TELEM_PER_FRAME;
        pending_.assign(framesPerSample_ * FOCP_TELEM_PER_FRAME, 0.0f);
        curSeq_ = lastSeq_ = -1;
        framesGot_ = 0;
        sampleCounter_ = 0;
        lost_ = 0;
    }
    if (wasRunning) runScope(true);
}

void Device::runScope(bool run)
{
    focp_msg_t m;
    focp_build_cmd_u8(&m, node_, FOCP_CMD_TELEM_RUN, run ? 1 : 0);
    send(m);
    scopeRun_ = run;
}

void Device::clearScope()
{
    std::lock_guard<std::mutex> lk(scopeMx_);
    scope_.reset(scope_.signals, scope_.capacity);
    lastSeq_ = -1;
    sampleCounter_ = 0;
}

double Device::scopeSampleTime() const
{
    const float f = paramValue(FOCP_P_INV_F_PWM);
    return (f > 0 ? scopeDiv_ / f : 1e-3);
}

void Device::handleTelem(const focp_msg_t& m)
{
    if (m.len < 8) return;
    const double dt = scopeSampleTime();
    std::lock_guard<std::mutex> lk(scopeMx_);
    if (framesPerSample_ == 0) return;
    const int seq = m.data[0] & 0xFF;
    const int first = m.data[1] & 0xFF;
    if (first + int(FOCP_TELEM_PER_FRAME) > int(pending_.size())) return;

    if (seq != curSeq_) {               // new sample begins; drop an incomplete previous one
        if (curSeq_ >= 0 && framesGot_ != 0 && framesGot_ < framesPerSample_) ++lost_;
        curSeq_ = seq;
        framesGot_ = 0;
    }
    for (int k = 0; k < int(FOCP_TELEM_PER_FRAME); ++k) {
        const int slot = first + k;
        const int sig = slot < int(scope_.signals.size()) ? scope_.signals[slot] : -1;
        const float scale = sig >= 0 ? focp_signal_table[sig].scale : 1.0f;
        pending_[slot] = focp_get_i16(&m.data[2 + 2 * k]) * scale;
    }
    if (++framesGot_ < framesPerSample_) return;

    if (lastSeq_ >= 0) {
        const int delta = (seq - lastSeq_) & 0xFF;
        if (delta > 1) lost_ += delta - 1;
        sampleCounter_ += delta ? delta : 256;
    }
    lastSeq_ = seq;
    framesGot_ = 0;
    scope_.push(sampleCounter_ * dt, pending_.data());
}

// --- receive -----------------------------------------------------------------------------
void Device::rxLoop()
{
    auto lastHb = std::chrono::steady_clock::now();
    while (running_) {
        focp_msg_t m;
        if (tr_->receive(m, 20) && (m.node & 0x0F) == node_) {
            ++rxCount_;
            handle(m);
        }
        const auto now = std::chrono::steady_clock::now();
        if (heartbeat && now - lastHb > std::chrono::milliseconds(100)) {
            focp_msg_t hb;
            focp_build_cmd(&hb, node_, FOCP_CMD_HEARTBEAT);
            send(hb);
            lastHb = now;
        }
    }
}

void Device::handle(const focp_msg_t& m)
{
    switch (m.fn) {
    case FOCP_FN_TELEM:
        handleTelem(m);
        break;

    case FOCP_FN_STATUS: {
        if (m.len < 8) break;
        std::lock_guard<std::mutex> lk(mx_);
        status_.valid = true;
        status_.state = m.data[0] & 0xFF;
        status_.mode = m.data[1] & 0x0F;
        status_.sensor = (m.data[1] >> 4) & 0x0F;
        status_.faults = focp_get_u16(&m.data[2]);
        status_.udc = focp_get_u16(&m.data[4]) * 0.01f;
        status_.temp = focp_get_i16(&m.data[6]) * 0.01f;
        status_.stamp = std::chrono::steady_clock::now();
        break;
    }

    case FOCP_FN_PARAM_RSP: {
        if (m.len < 7) break;
        const uint16_t st = m.data[0] & 0xFF;
        const uint16_t id = focp_get_u16(&m.data[1]);
        const int idx = focp_param_find(id);
        if (idx < 0) { addLog(1, "Antwort fuer unbekannte Parameter-ID 0x" + std::to_string(id)); break; }
        {
            std::lock_guard<std::mutex> lk(mx_);
            auto& p = params_[idx];
            p.pending = false;
            p.status = st;
            if (st == FOCP_PS_OK) { p.value = focp_get_f32(&m.data[3]); p.valid = true; }
            else if (st != FOCP_PS_UNKNOWN_ID) { p.value = focp_get_f32(&m.data[3]); }
        }
        if (st != FOCP_PS_OK) {
            static const char* why[] = {"ok", "unbekannt", "nur lesbar", "ausserhalb Bereich", "gesperrt (Antrieb laeuft)", "ungueltige Anfrage"};
            addLog(1, std::string("Parameter ") + focp_param_table[idx].name + ": " + (st < 6 ? why[st] : "?"));
        }
        break;
    }

    case FOCP_FN_EVENT: {
        if (m.len < 7) break;
        const int ev = m.data[0] & 0xFF;
        const uint16_t code = focp_get_u16(&m.data[1]);
        if (ev == FOCP_EV_STATE) addLog(0, std::string("Zustand -> ") + focp_state_name(code));
        else if (ev == FOCP_EV_FAULT_SET || ev == FOCP_EV_FAULT_CLEAR) {
            for (uint16_t b = 1; b; b <<= 1)
                if (code & b)
                    addLog(ev == FOCP_EV_FAULT_SET ? 2 : 0,
                           std::string(ev == FOCP_EV_FAULT_SET ? "Fehler: " : "Quittiert: ") + focp_fault_name(b));
        }
        else if (ev == FOCP_EV_CALIB_DONE) { addLog(0, "Kalibrierung abgeschlossen"); readAllParams(); }
        else if (ev == FOCP_EV_CALIB_FAILED) addLog(2, "Kalibrierung fehlgeschlagen / nicht verfuegbar");
        else if (ev == FOCP_EV_PARAMS_SAVED) addLog(0, "Parameter gespeichert");
        break;
    }

    case FOCP_FN_INFO: {
        std::lock_guard<std::mutex> lk(mx_);
        info_.valid = true;
        info_.protocol = m.data[0] & 0xFF;
        info_.fwMajor = m.data[1] & 0xFF;
        info_.fwMinor = m.data[2] & 0xFF;
        info_.board = m.data[3] & 0xFF;
        info_.numParams = focp_get_u16(&m.data[4]);
        info_.numSignals = m.data[6] & 0xFF;
        break;
    }
    default:
        break;
    }
    if (m.fn == FOCP_FN_INFO) {
        const auto inf = info();
        if (inf.protocol != int(FOCP_PROTOCOL_VERSION) || inf.numParams != FOCP_NUM_PARAMS || inf.numSignals != FOCP_NUM_SIGNALS)
            addLog(1, "Protokoll-/Tabellenversion von Antrieb und Tool unterscheiden sich");
    }
}

bool Device::exportCsv(const std::string& path, std::string& error)
{
    std::ofstream f(path);
    if (!f) { error = "Datei kann nicht geschrieben werden: " + path; return false; }
    std::lock_guard<std::mutex> lk(scopeMx_);
    f << "t_s";
    for (int s : scope_.signals)
        if (s >= 0) f << ';' << focp_signal_table[s].name << '_' << focp_signal_table[s].unit;
    f << '\n';
    const size_t off = scope_.offset();
    for (size_t i = 0; i < scope_.count; ++i) {
        const size_t k = (off + i) % scope_.capacity;
        f << scope_.t[k];
        for (size_t s = 0; s < scope_.signals.size(); ++s)
            if (scope_.signals[s] >= 0) f << ';' << scope_.y[s][k];
        f << '\n';
    }
    return true;
}

} // namespace fochost
