#include "fochost/SimTransport.hpp"

#include <chrono>

namespace fochost {

SimTransport::SimTransport(uint16_t node) : node_(node) {}

SimTransport::~SimTransport() { close(); }

bool SimTransport::open(std::string&)
{
    if (running_) return true;
    drive_ = std::make_unique<VirtualDrive>(node_, &SimTransport::driveSend, this);
    running_ = true;
    thread_ = std::thread([this] { run(); });
    return true;
}

void SimTransport::close()
{
    if (!running_) return;
    running_ = false;
    if (thread_.joinable()) thread_.join();
    std::lock_guard<std::mutex> lk(mx_);
    toHost_.clear();
    toDrive_.clear();
}

bool SimTransport::send(const focp_msg_t& m)
{
    if (!running_) return false;
    std::lock_guard<std::mutex> lk(mx_);
    toDrive_.push_back(m);
    return true;
}

bool SimTransport::receive(focp_msg_t& m, int timeoutMs)
{
    std::unique_lock<std::mutex> lk(mx_);
    if (!cv_.wait_for(lk, std::chrono::milliseconds(timeoutMs), [this] { return !toHost_.empty() || !running_; }))
        return false;
    if (toHost_.empty()) return false;
    m = toHost_.front();
    toHost_.pop_front();
    return true;
}

int SimTransport::driveSend(void* ctx, const focp_msg_t* m)
{
    auto* self = static_cast<SimTransport*>(ctx);
    // called from the sim thread while it holds no lock
    std::lock_guard<std::mutex> lk(self->mx_);
    if (self->toHost_.size() > kMaxQueue) return -1;   // "CAN bus overloaded"
    self->toHost_.push_back(*m);
    self->cv_.notify_one();
    return 0;
}

void SimTransport::run()
{
    using clock = std::chrono::steady_clock;
    auto last = clock::now();
    double backlog = 0.0;
    std::deque<focp_msg_t> inbox;

    while (running_) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        const auto now = clock::now();
        const double elapsed = std::chrono::duration<double>(now - last).count();
        last = now;

        {
            std::lock_guard<std::mutex> lk(mx_);
            inbox.swap(toDrive_);
        }
        for (const auto& m : inbox) drive_->rx(m);
        inbox.clear();

        // keep simulated time aligned with wall time (max 20 ms catch-up per round)
        backlog += elapsed * speedFactor_;
        if (backlog > 0.02) backlog = 0.02;
        const double tc = drive_->controlPeriod();
        while (backlog >= tc) {
            drive_->step();
            backlog -= tc;
        }
    }
}

} // namespace fochost
