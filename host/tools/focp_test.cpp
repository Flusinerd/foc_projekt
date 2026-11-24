// focp_test - command line access to a FOCP drive for scripted hardware tests (no GUI).
//
//   focp_test [--sim] [--bitrate N] <command> ...
//
//   status                                  info, state, faults, Udc, temperature, main parameters
//   param <name> [value]                    read or write one parameter (e.g. inv.I_max 2)
//   ack                                     acknowledge faults
//   watch <s>                               print state/fault changes and Udc every 0.5 s
//   record <signals> <div> <s> <csv>        telemetry without enabling the drive
//   run <rpm> <s> <signals> <div> <csv> --freigabe
//                                           ENABLE, speed reference, record, then reference 0 and
//                                           DISABLE; stops at once on a fault or on Ctrl+C
//
//   <signals>: comma separated names from focp_signals.def (ia,ib,ic,speed,hall_state,...).
//   Transport: CANable/candleLight (gs_usb) at 1 Mbit/s, node 1; --sim uses the virtual drive.
//   Hardware rule (AGENTS.md rule 4): only with the human's approval in the same session; FOC Studio
//   must be closed (one program per adapter). The drive enters FOCP mode on the first frame and
//   reports a COMM fault after the tool exits (heartbeat timeout) - acknowledge with "ack".
#include <fochost/Device.hpp>
#include <fochost/SimTransport.hpp>

#include <atomic>
#include <chrono>
#include <csignal>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

using namespace std::chrono_literals;
using fochost::Device;

static std::atomic<bool> g_stop{false};
static void onSignal(int) { g_stop = true; }

template <class F> static bool waitFor(F cond, std::chrono::milliseconds timeout)
{
    const auto end = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < end) {
        if (cond()) return true;
        std::this_thread::sleep_for(5ms);
    }
    return cond();
}

static double nowS()
{
    static const auto t0 = std::chrono::steady_clock::now();
    return std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
}

static std::string faultText(uint16_t f)
{
    if (!f) return "-";
    std::string s;
    for (int b = 0; b < 16; ++b)
        if (f & (1u << b)) { if (!s.empty()) s += ", "; s += focp_fault_name(static_cast<uint16_t>(1u << b)); }
    return s;
}

static void printStatus(const Device& d)
{
    const auto st = d.status();
    if (!st.valid) { std::printf("%8.2f s  kein STATUS\n", nowS()); return; }
    std::printf("%8.2f s  %-10s Udc %6.2f V  T %5.1f degC  Fehler 0x%04X (%s)\n", nowS(),
                focp_state_name(st.state), st.udc, st.temp, st.faults, faultText(st.faults).c_str());
}

static void printLog(const Device& d, uint64_t& seq)
{
    for (const auto& e : d.logSince(seq)) {
        seq = e.seq;
        const char* what = "";
        std::string code = std::to_string(e.code);
        switch (e.kind) {
        case fochost::LogKind::State:        what = "Zustand"; code = focp_state_name(static_cast<uint16_t>(e.code)); break;
        case fochost::LogKind::FaultSet:     what = "Fehler +"; code = focp_fault_name(static_cast<uint16_t>(e.code)); break;
        case fochost::LogKind::FaultCleared: what = "Fehler -"; code = focp_fault_name(static_cast<uint16_t>(e.code)); break;
        case fochost::LogKind::ParamRejected: what = "Parameter abgelehnt"; break;
        case fochost::LogKind::Connected:    what = "verbunden"; break;
        case fochost::LogKind::VersionMismatch: what = "Protokollversion passt nicht"; break;
        default: what = "Ereignis"; break;
        }
        std::printf("%8.2f s  [%s] %s %s\n", nowS(), what, code.c_str(), e.detail.c_str());
    }
}

static bool parseSignals(const char* list, std::vector<int>& out)
{
    std::string s(list);
    size_t p = 0;
    while (p <= s.size()) {
        const size_t q = s.find(',', p);
        const std::string name = s.substr(p, q == std::string::npos ? std::string::npos : q - p);
        int idx = -1;
        for (int i = 0; i < FOCP_NUM_SIGNALS; ++i)
            if (name == focp_signal_table[i].name) idx = i;
        if (idx < 0) { std::fprintf(stderr, "unbekanntes Signal: %s\n", name.c_str()); return false; }
        out.push_back(idx);
        if (q == std::string::npos) break;
        p = q + 1;
    }
    return !out.empty();
}

static int paramIndex(const char* name)
{
    for (int i = 0; i < FOCP_NUM_PARAMS; ++i)
        if (std::strcmp(name, focp_param_table[i].name) == 0) return i;
    return -1;
}

static bool readParamWait(Device& d, int idx, float& v)
{
    d.readParam(idx);
    if (!waitFor([&] { auto p = d.params()[idx]; return p.valid && !p.pending; }, 1500ms)) return false;
    v = d.params()[idx].value;
    return true;
}

static void stopDrive(Device& d)
{
    d.setReference(0.0f);
    std::this_thread::sleep_for(300ms);
    d.disable();
    std::this_thread::sleep_for(200ms);
}

// Records for 'seconds'; returns false if a fault appeared (or Ctrl+C).
static bool recordLoop(Device& d, double seconds, uint64_t& seq, bool abortOnFault)
{
    const double tEnd = nowS() + seconds;
    double tPrint = 0;
    bool ok = true;
    while (nowS() < tEnd && !g_stop) {
        printLog(d, seq);
        const auto st = d.status();
        if (abortOnFault && st.valid && (st.faults || st.state == FOCP_ST_FAULT)) { ok = false; break; }
        if (nowS() >= tPrint) { printStatus(d); tPrint = nowS() + 1.0; }
        std::this_thread::sleep_for(20ms);
    }
    printLog(d, seq);
    return ok && !g_stop;
}

static bool saveCsv(Device& d, const char* path)
{
    std::string err;
    size_t n = 0;
    d.withScope([&](const fochost::ScopeBuffer& s) { n = s.count; });
    if (!d.exportCsv(path, err)) { std::fprintf(stderr, "%s\n", err.c_str()); return false; }
    std::printf("CSV: %s (%zu Abtastungen, Abtastzeit %.6f s, verloren %llu)\n", path, n,
                d.scopeSampleTime(), static_cast<unsigned long long>(d.lostSamples()));
    return true;
}

// Ring buffer for the whole recording (default 200000 samples = 100 s at 2 kHz), 10 % margin.
static size_t scopeCapacity(const Device& d, double seconds, int div)
{
    const double hz = d.telemetryBaseHz() / (div > 0 ? div : 1);
    const size_t n = static_cast<size_t>(seconds * hz * 1.1) + 1000;
    return n > 200000 ? n : 200000;
}

static int usage()
{
    std::fprintf(stderr,
        "focp_test [--sim] [--bitrate N] status | param <name> [value] | ack | watch <s> |\n"
        "          record <signals> <div> <s> <csv> | run <rpm> <s> <signals> <div> <csv> --freigabe\n");
    return 2;
}

int main(int argc, char** argv)
{
    std::signal(SIGINT, onSignal);
    bool sim = false, freigabe = false;
    int bitrate = 1000000;
    std::vector<const char*> a;
    for (int i = 1; i < argc; ++i) {
        if (!std::strcmp(argv[i], "--sim")) sim = true;
        else if (!std::strcmp(argv[i], "--freigabe")) freigabe = true;
        else if (!std::strcmp(argv[i], "--bitrate") && i + 1 < argc) bitrate = std::atoi(argv[++i]);
        else a.push_back(argv[i]);
    }
    if (a.empty()) return usage();
    const std::string cmd = a[0];
    if (cmd == "run" && !freigabe) { std::fprintf(stderr, "run nur mit --freigabe (Freigabe des Menschen)\n"); return 2; }

    Device dev;
    std::string err;
    bool ok;
    if (sim) {
        ok = dev.connect(std::make_unique<fochost::SimTransport>(1), 1, err);
    } else {
        fochost::TransportConfig cfg;
        cfg.kind = fochost::TransportKind::GsUsb;
        cfg.channel = "";
        cfg.bitrate = bitrate;
        ok = dev.connect(cfg, 1, err);
    }
    if (!ok) { std::fprintf(stderr, "Verbindung fehlgeschlagen: %s\n", err.c_str()); return 1; }
    uint64_t seq = 0;
    dev.requestInfo();
    if (!waitFor([&] { return dev.status().valid; }, 3000ms)) {
        std::fprintf(stderr, "kein STATUS vom Antrieb (Knoten 1) in 3 s\n");
        printLog(dev, seq);
        return 1;
    }
    waitFor([&] { return dev.info().valid; }, 1000ms);
    const auto inf = dev.info();
    std::printf("Antrieb: %s, Protokoll %d, FW %d.%d, %d Parameter, %d Signale\n", dev.transportName().c_str(),
                inf.protocol, inf.fwMajor, inf.fwMinor, inf.numParams, inf.numSignals);
    printLog(dev, seq);
    printStatus(dev);

    int rc = 0;
    if (cmd == "status") {
        const char* names[] = {"inv.I_max", "inv.Udc_min", "inv.Udc_max", "spd.Iq_max", "spd.Iq_min",
                               "cur.Kp", "cur.Ki", "spd.Kp", "spd.Ki", "comm.telem_f"};
        for (const char* n : names) {
            float v = 0; const int idx = paramIndex(n);
            if (idx >= 0 && readParamWait(dev, idx, v)) std::printf("  %-14s %g %s\n", n, v, focp_param_table[idx].unit);
            else std::printf("  %-14s (keine Antwort)\n", n);
        }
    } else if (cmd == "param" && (a.size() == 2 || a.size() == 3)) {
        const int idx = paramIndex(a[1]);
        if (idx < 0) { std::fprintf(stderr, "unbekannter Parameter: %s\n", a[1]); return 2; }
        float v = 0;
        if (a.size() == 3) {
            dev.writeParam(idx, static_cast<float>(std::atof(a[2])));
            waitFor([&] { return !dev.params()[idx].pending; }, 1500ms);
            const auto p = dev.params()[idx];
            if (p.status != FOCP_PS_OK) { std::printf("abgelehnt, Status %u\n", p.status); rc = 1; }
        }
        if (readParamWait(dev, idx, v)) std::printf("%s = %g %s\n", a[1], v, focp_param_table[idx].unit);
        else { std::printf("%s: keine Antwort\n", a[1]); rc = 1; }
    } else if (cmd == "ack" && a.size() == 1) {
        dev.ackFault();
        std::this_thread::sleep_for(500ms);
        printLog(dev, seq);
        printStatus(dev);
        rc = dev.status().faults ? 1 : 0;
    } else if (cmd == "watch" && a.size() == 2) {
        const double tEnd = nowS() + std::atof(a[1]);
        while (nowS() < tEnd && !g_stop) {
            printLog(dev, seq);
            printStatus(dev);
            std::this_thread::sleep_for(500ms);
        }
    } else if (cmd == "record" && a.size() == 5) {
        std::vector<int> sig;
        if (!parseSignals(a[1], sig)) return 2;
        dev.configureScope(sig, static_cast<uint16_t>(std::atoi(a[2])), scopeCapacity(dev, std::atof(a[3]), std::atoi(a[2])));
        dev.runScope(true);
        recordLoop(dev, std::atof(a[3]), seq, false);
        dev.runScope(false);
        std::this_thread::sleep_for(100ms);
        rc = saveCsv(dev, a[4]) ? 0 : 1;
    } else if (cmd == "run" && a.size() == 6) {
        const float rpm = static_cast<float>(std::atof(a[1]));
        if (!freigabe) { std::fprintf(stderr, "run nur mit --freigabe (Freigabe des Menschen)\n"); return 2; }
        if (std::fabs(rpm) > 3000.0f) { std::fprintf(stderr, "Drehzahl auf +/-3000 rpm begrenzt\n"); return 2; }
        std::vector<int> sig;
        if (!parseSignals(a[3], sig)) return 2;
        waitFor([&] { const auto s = dev.status(); return s.state == FOCP_ST_READY || s.state == FOCP_ST_FAULT; }, 3000ms);   // RUN = potentiometer mode until FOCP takes over
        printLog(dev, seq);
        const auto st = dev.status();
        if (st.state != FOCP_ST_READY || st.faults) {
            std::fprintf(stderr, "Antrieb nicht bereit (%s, Fehler 0x%04X) - erst 'ack'\n", focp_state_name(st.state), st.faults);
            return 1;
        }
        dev.configureScope(sig, static_cast<uint16_t>(std::atoi(a[4])), scopeCapacity(dev, std::atof(a[2]) + 2.0, std::atoi(a[4])));
        dev.runScope(true);
        std::this_thread::sleep_for(300ms);       // a little standstill before the start
        dev.setReference(rpm);
        dev.enable();
        std::printf("%8.2f s  ENABLE, Soll %.0f rpm\n", nowS(), rpm);
        const bool clean = recordLoop(dev, std::atof(a[2]), seq, true);
        stopDrive(dev);
        std::printf("%8.2f s  Soll 0, DISABLE%s\n", nowS(), clean ? "" : (g_stop ? " (abgebrochen)" : " (Fehler)"));
        recordLoop(dev, 1.0, seq, false);          // run-down
        dev.runScope(false);
        std::this_thread::sleep_for(100ms);
        saveCsv(dev, a[5]);
        rc = clean ? 0 : 1;
    } else {
        return usage();
    }
    printLog(dev, seq);
    dev.disconnect();
    return rc;
}
