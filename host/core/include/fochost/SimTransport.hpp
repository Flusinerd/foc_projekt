#pragma once
#include "fochost/Transport.hpp"
#include "fochost/VirtualDrive.hpp"

#include <atomic>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>

namespace fochost {

/// Transport to an in-process VirtualDrive running in real time on its own thread.
class SimTransport final : public Transport {
public:
    explicit SimTransport(uint16_t node = 1);
    ~SimTransport() override;

    std::string name() const override { return "Simulation"; }
    bool open(std::string& error) override;
    void close() override;
    bool isOpen() const override { return running_; }
    bool send(const focp_msg_t& m) override;
    bool receive(focp_msg_t& m, int timeoutMs) override;

private:
    static int driveSend(void* ctx, const focp_msg_t* m);
    void run();

    static constexpr size_t kMaxQueue = 20000;
    uint16_t node_;
    double speedFactor_ = 1.0;
    std::unique_ptr<VirtualDrive> drive_;
    std::atomic<bool> running_{false};
    std::thread thread_;
    std::mutex mx_;
    std::condition_variable cv_;
    std::deque<focp_msg_t> toHost_, toDrive_;
};

} // namespace fochost
