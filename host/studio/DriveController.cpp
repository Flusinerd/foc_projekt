#include "DriveController.hpp"

#include <QDateTime>

using namespace fochost;

namespace {
constexpr int kPollMs = 33;
constexpr uint16_t kTelemDivider = 20;   // f_PWM / 20, e.g. 1 kHz at 20 kHz
}

DriveController::DriveController(QObject* parent) : QObject(parent), params_(FOCP_NUM_PARAMS)
{
    timer_.setInterval(kPollMs);
    connect(&timer_, &QTimer::timeout, this, &DriveController::poll);
    timer_.start();
}

DriveController::~DriveController() { dev_.disconnect(); }

bool DriveController::connectTo(const TransportConfig& cfg, int node, QString& error)
{
    demo_ = false;
    std::string err;
    if (!dev_.connect(cfg, uint16_t(node), err)) {
        error = QString::fromStdString(err);
        Q_EMIT connectionChanged(false);
        return false;
    }
    node_ = node;
    startTelemetry();
    Q_EMIT connectionChanged(true);
    poll();
    return true;
}

void DriveController::disconnectFrom()
{
    demo_ = false;
    dev_.disconnect();
    speedValid_ = false;
    Q_EMIT connectionChanged(false);
    poll();
}

void DriveController::startTelemetry()
{
    // The dashboard needs the speed; the remaining slots are filled by the scope later.
    dev_.configureScope({FOCP_S_SPEED, FOCP_S_SPEED_REF, FOCP_S_IQ, FOCP_S_IQ_REF}, kTelemDivider);
    dev_.runScope(true);
}

void DriveController::poll()
{
    const bool conn = dev_.connected();
    status_ = dev_.status();
    info_ = dev_.info();
    params_ = dev_.params();

    speedValid_ = false;
    if (conn) {
        dev_.withScope([this](const ScopeBuffer& s) {
            if (s.count == 0) return;
            const size_t last = s.count < s.capacity ? s.count - 1 : (s.head + s.capacity - 1) % s.capacity;
            for (size_t k = 0; k < s.signals.size(); ++k)
                if (s.signals[k] == FOCP_S_SPEED && k < s.y.size()) {
                    speed_ = s.y[k][last];
                    speedValid_ = true;
                }
        });
    }
    if (demo_) demoStep();
    Q_EMIT updated();
}

// ------------------------------------------------------------------------------------------
bool DriveController::startDemo(QString& error)
{
    TransportConfig cfg;
    cfg.kind = TransportKind::Simulation;
    if (!connectTo(cfg, 1, error)) return false;
    demo_ = true;
    demoPhase_ = -1;
    demoT0_ = QDateTime::currentMSecsSinceEpoch();
    return true;
}

void DriveController::demoStep()
{
    // Only ever drives the virtual drive.
    if (!dev_.connected() || dev_.transportName() != "Simulation") { demo_ = false; return; }
    const double t = (QDateTime::currentMSecsSinceEpoch() - demoT0_) * 1e-3;
    auto phase = [&](int p, double at) { return demoPhase_ < p && t >= at; };

    if (phase(0, 0.6) && status_.state == FOCP_ST_READY) { dev_.calibrate(FOCP_CALIB_HALL); demoPhase_ = 0; }
    if (phase(1, 3.4) && status_.state == FOCP_ST_READY) {
        dev_.writeParam(FOCP_P_CTRL_MODE, FOCP_MODE_SPEED);
        dev_.setReference(1500);
        dev_.enable();
        demoPhase_ = 1;
    }
    if (phase(2, 5.5)) { dev_.writeParam(FOCP_P_SIM_T_LOAD, 0.3f); demoPhase_ = 2; }
    if (phase(3, 7.0)) { dev_.setReference(3000); demoPhase_ = 3; }
    if (phase(4, 9.0)) { dev_.writeParam(FOCP_P_SIM_T_LOAD, 0.0f); dev_.setReference(-1000); demoPhase_ = 4; }
    if (phase(5, 11.5)) { demo_ = false; demoPhase_ = 5; }
}
