#pragma once
#include <focp/focp_dev.h>

#include <cstdint>
#include <random>

namespace fochost {

/// Software model of one drive: PMSM (dq), ideal inverter with voltage limit,
/// Hall emulation (incl. spurious edges), simplified sensorless observer, the
/// cascaded FOC controllers and the supervisor state machine.
///
/// The protocol side is the real firmware code (focp_dev.c), so the GUI talks to
/// the virtual drive exactly as to the target.
///
/// Not thread-safe: SimTransport drives it from one thread and guards rx/step.
class VirtualDrive {
public:
    using SendFn = int (*)(void* ctx, const focp_msg_t* m);

    VirtualDrive(uint16_t node, SendFn send, void* sendCtx);

    void rx(const focp_msg_t& m) { focp_dev_rx(&dev_, &m); }

    /// Advance the simulation by one control period (1 / inv.f_pwm).
    void step();

    double time() const { return t_; }
    double controlPeriod() const { return 1.0 / dev_.param[FOCP_P_INV_F_PWM]; }

    // test access
    double speedRpm() const;
    double positionDeg() const;
    double measuredPositionDeg() const { return posMeasDeg_; }
    double measuredSpeedRpm() const { return speedMeasRpm_; }
    double speedCommandRpm() const { return spdRefRamped_; }
    double iq() const { return iq_; }
    double id() const { return id_; }
    uint16_t state() const { return state_; }
    uint16_t faults() const { return faults_; }
    float param(focp_param_idx_t i) const { return dev_.param[i]; }

    /// Simulated hall sensor mounting error [deg el]; hall calibration recovers it.
    static constexpr double kTrueHallOffsetDeg = 23.0;

private:
    static int  sendCb(void* ctx, const focp_msg_t* m);
    static int  isLockedCb(void* ctx);
    static void onCommandCb(void* ctx, uint16_t cmd, const focp_byte_t* args, uint16_t n);
    static void onParamChangedCb(void* ctx, uint16_t idx);

    void onCommand(uint16_t cmd, const focp_byte_t* args, uint16_t n);
    void setState(uint16_t s);
    void raiseFault(uint16_t bits);
    void resetControllers();
    void stepPlant(double ud, double uq, double dt);
    void updateHall(double dt);
    void updateObserver(double dt);
    uint16_t hallStateFromAngle(double thetaEl) const;
    uint32_t ms() const { return static_cast<uint32_t>(t_ * 1000.0); }

    SendFn send_ = nullptr;
    void* sendCtx_ = nullptr;
    focp_dev_t dev_{};
    std::mt19937 rng_{1234};

    // supervisor
    uint16_t state_ = FOCP_ST_INIT;
    uint16_t faults_ = 0;
    uint16_t calib_ = 0;
    double stateTime_ = 0.0;
    double t_ = 0.0;
    uint32_t cycle_ = 0;
    float ref_ = 0.0f;

    // plant
    double id_ = 0, iq_ = 0;          // true dq currents [A]
    double wm_ = 0;                   // mech. speed [rad/s]
    double thm_ = 0;                  // mech. angle [rad], multi-turn
    double thmPrev_ = 0;              // mech. angle at previous hall update
    double udc_ = 24.0;
    double temp_ = 25.0;
    double torque_ = 0.0;
    bool inverterOn_ = false;

    // hall
    uint16_t hall_ = 0;
    double hallEdgeAngle_ = 0;        // el. angle at last edge [rad] (as seen by sensor)
    double hallEdgeTime_ = 0;
    double hallSpeed_ = 0;            // el. rad/s from edge timing (interpolation)
    double hallSpeedCtrl_ = 0;        // el. rad/s for the control loops (decays if edge overdue)
    double thetaHall_ = 0;
    int hallDir_ = 1;
    bool hallInvalid_ = false;
    double calibAccF_ = 0.0, calibAccB_ = 0.0;
    int calibNF_ = 0, calibNB_ = 0;

    // observer
    double thetaObs_ = 0, speedObs_ = 0;
    bool obsLocked_ = false;
    double errAvg_ = 1.0;
    bool useObserver_ = false;

    // control
    double thetaCtrl_ = 0;
    double thetaOl_ = 0, wOl_ = 0;     // open loop / I/f startup [el rad, el rad/s]
    bool startup_ = false;
    double intD_ = 0, intQ_ = 0, intSpd_ = 0;
    double ud_ = 0, uq_ = 0;
    double idRef_ = 0, iqRef_ = 0;
    double spdRefRamped_ = 0, posRef_ = 0, spdCmd_ = 0;
    double posMeasDeg_ = 0, speedMeasRpm_ = 0, posAccEl_ = 0;
    double idMeas_ = 0, iqMeas_ = 0;

    float sig_[FOCP_NUM_SIGNALS] = {};
};

} // namespace fochost
