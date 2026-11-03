// Integration: Device <-> SimTransport (threads, real time).
#include "check.hpp"

#include <fochost/Device.hpp>
#include <fochost/SimTransport.hpp>

#include <chrono>
#include <filesystem>
#include <thread>

using namespace std::chrono_literals;

template <class F> static bool waitFor(F cond, std::chrono::milliseconds timeout)
{
    const auto end = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < end) {
        if (cond()) return true;
        std::this_thread::sleep_for(5ms);
    }
    return cond();
}

int main()
{
    fochost::Device dev;
    std::string err;
    CHECK(dev.connect(std::make_unique<fochost::SimTransport>(1), 1, err));

    std::puts("parameters and info");
    CHECK(waitFor([&] { for (auto& p : dev.params()) if (!p.valid) return false; return true; }, 2000ms));
    CHECK(waitFor([&] { return dev.info().valid; }, 1000ms));
    CHECK(dev.info().numParams == FOCP_NUM_PARAMS);
    CHECK(waitFor([&] { return dev.status().valid && dev.status().state == FOCP_ST_READY; }, 2000ms));

    dev.writeParam(FOCP_P_HALL_OFFSET, 23.0f);
    CHECK(waitFor([&] { auto p = dev.params()[FOCP_P_HALL_OFFSET]; return !p.pending && p.value == 23.0f; }, 1000ms));
    dev.writeParam(FOCP_P_CUR_KP, 999.0f);          // out of range -> rejected, value unchanged
    CHECK(waitFor([&] { return !dev.params()[FOCP_P_CUR_KP].pending; }, 1000ms));
    CHECK(dev.params()[FOCP_P_CUR_KP].status == FOCP_PS_OUT_OF_RANGE);
    CHECK(dev.params()[FOCP_P_CUR_KP].value == 0.255f);

    std::puts("telemetry while running");
    dev.configureScope({FOCP_S_SPEED, FOCP_S_SPEED_REF, FOCP_S_IQ, FOCP_S_ID}, 20);
    dev.runScope(true);
    dev.setReference(1500.0f);
    dev.enable();
    std::this_thread::sleep_for(1500ms);

    size_t n = 0; double lastSpeed = 0, tLast = 0;
    dev.withScope([&](const fochost::ScopeBuffer& s) {
        n = s.count;
        tLast = s.tLast();
        if (s.count) {
            const size_t last = (s.count < s.capacity) ? s.count - 1 : (s.head + s.capacity - 1) % s.capacity;
            lastSpeed = s.y[0][last];
        }
    });
    std::printf("  %zu samples, t=%.3f s, speed=%.1f rpm, lost=%llu\n", n, tLast, lastSpeed,
                static_cast<unsigned long long>(dev.lostSamples()));
    CHECK(n > 1000);                                  // 1 kHz for ~1.5 s
    CHECK_NEAR(lastSpeed, 1500.0, 60.0);
    CHECK(dev.lostSamples() < n / 100);
    CHECK(dev.status().state == FOCP_ST_RUN);

    std::puts("heartbeat keeps the drive alive, stopping it trips COMM");
    dev.writeParam(FOCP_P_COMM_TIMEOUT, 300.0f);
    std::this_thread::sleep_for(600ms);
    CHECK(dev.status().state == FOCP_ST_RUN);
    dev.heartbeat = false;
    CHECK(waitFor([&] { return dev.status().faults & FOCP_FAULT_COMM; }, 2000ms));
    dev.heartbeat = true;
    dev.ackFault();
    CHECK(waitFor([&] { return dev.status().state == FOCP_ST_READY; }, 1000ms));

    std::puts("structured log entries");
    {
        using fochost::LogKind;
        const auto log = dev.log();
        auto has = [&](LogKind k, int code) {
            for (const auto& e : log) if (e.kind == k && e.code == code) return true;
            return false;
        };
        CHECK(!log.empty() && log.front().kind == LogKind::Connected && log.front().detail == "Simulation");
        CHECK(has(LogKind::FaultSet, FOCP_FAULT_COMM));
        CHECK(has(LogKind::FaultCleared, FOCP_FAULT_COMM));
        CHECK(has(LogKind::State, FOCP_ST_RUN));
        CHECK(has(LogKind::ParamRejected, FOCP_PS_OUT_OF_RANGE));
        bool increasing = true;
        for (size_t i = 1; i < log.size(); ++i) increasing = increasing && log[i].seq > log[i - 1].seq;
        CHECK(increasing);
        const auto tail = dev.logSince(log[log.size() - 3].seq);
        CHECK(tail.size() >= 2 && tail.front().seq == log[log.size() - 2].seq);
        CHECK(dev.logSince(log.back().seq + 1000).empty() || dev.logSince(log.back().seq + 1000).front().seq > log.back().seq);
    }

    CHECK(dev.exportCsv((std::filesystem::temp_directory_path() / "focp_test_scope.csv").string(), err));
    dev.disconnect();
    return TEST_RESULT();
}
