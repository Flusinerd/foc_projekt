#pragma once
#include <focp/focp.h>

#include <memory>
#include <string>
#include <vector>

namespace fochost {

/// One link to one or more drives. Implementations must be thread-safe for
/// one sender thread + one receiver thread.
class Transport {
public:
    virtual ~Transport() = default;
    virtual std::string name() const = 0;
    virtual bool open(std::string& error) = 0;
    virtual void close() = 0;
    virtual bool isOpen() const = 0;
    virtual bool send(const focp_msg_t& m) = 0;
    /// Waits up to timeoutMs for one message. Returns false on timeout.
    virtual bool receive(focp_msg_t& m, int timeoutMs) = 0;
};

enum class TransportKind { Simulation, SocketCAN, PCAN };

struct TransportConfig {
    TransportKind kind = TransportKind::Simulation;
    std::string channel = "can0";   ///< SocketCAN interface or PCAN channel ("USB1".."USB16")
    int bitrate = 500000;           ///< PCAN only; SocketCAN bitrate is set via `ip link`
};

std::unique_ptr<Transport> makeTransport(const TransportConfig& cfg);
std::vector<TransportKind> availableTransports();
const char* transportName(TransportKind k);

/// CAN frame <-> FOCP message mapping (shared by all CAN transports).
bool canToMsg(uint32_t canId, const uint8_t* data, uint8_t dlc, focp_msg_t& m);
void msgToCan(const focp_msg_t& m, uint32_t& canId, uint8_t* data, uint8_t& dlc);

} // namespace fochost
