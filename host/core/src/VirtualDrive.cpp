#include "fochost/VirtualDrive.hpp"

#include <algorithm>
#include <cmath>

namespace fochost {
namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kTwoPi = 2.0 * kPi;
constexpr double kDeg = kPi / 180.0;
constexpr double kRpmToRad = kTwoPi / 60.0;

/// Hall code per 60-degree sector (sequence ABC, same as Hall Speed and Position block).
constexpr uint16_t kHallSeq[6] = {5, 4, 6, 2, 3, 1};

double wrap2Pi(double a)
{
    a = std::fmod(a, kTwoPi);
    return a < 0 ? a + kTwoPi : a;
}

double wrapPi(double a)
{
    a = wrap2Pi(a + kPi);
    return a - kPi;
}

int hallSector(uint16_t code)
{
    for (int i = 0; i < 6; ++i)
        if (kHallSeq[i] == code) return i;
    return -1;
}

template <typename T> T clamp(T v, T lo, T hi) { return v < lo ? lo : (v > hi ? hi : v); }

} // namespace

VirtualDrive::VirtualDrive(uint16_t node, SendFn send, void* sendCtx)
{
    focp_dev_cfg_t cfg{};
    cfg.node = node;
    cfg.fw_major = 0;
    cfg.fw_minor = 1;
    cfg.board_id = 0xFE;      // virtual
    cfg.ctx = this;
    cfg.send = &VirtualDrive::sendCb;
    cfg.on_command = &VirtualDrive::onCommandCb;
    cfg.is_locked = &VirtualDrive::isLockedCb;
    cfg.on_param_changed = &VirtualDrive::onParamChangedCb;
    send_ = send;
    sendCtx_ = sendCtx;
    focp_dev_init(&dev_, &cfg);
}

int VirtualDrive::sendCb(void* ctx, const focp_msg_t* m)
{
    auto* d = static_cast<VirtualDrive*>(ctx);
    return d->send_ ? d->send_(d->sendCtx_, m) : -1;
}

// --- callbacks ----------------------------------------------------------------------------
int VirtualDrive::isLockedCb(void* ctx)
{
    auto* d = static_cast<VirtualDrive*>(ctx);
    return (d->state_ == FOCP_ST_RUN || d->state_ == FOCP_ST_CALIB) ? 1 : 0;
}

void VirtualDrive::onCommandCb(void* ctx, uint16_t cmd, const focp_byte_t* args, uint16_t n)
{
    static_cast<VirtualDrive*>(ctx)->onCommand(cmd, args, n);
}

void VirtualDrive::onParamChangedCb(void*, uint16_t) {}

// --- supervisor ---------------------------------------------------------------------------
void VirtualDrive::setState(uint16_t s)
{
    if (s == state_) return;
    state_ = s;
    stateTime_ = 0.0;
    focp_dev_send_event(&dev_, FOCP_EV_STATE, s, ms());
}

void VirtualDrive::raiseFault(uint16_t bits)
{
    const uint16_t fresh = static_cast<uint16_t>(bits & ~faults_);
    if (!fresh) return;
    faults_ |= fresh;
    focp_dev_send_event(&dev_, FOCP_EV_FAULT_SET, fresh, ms());
    setState(FOCP_ST_FAULT);
}

void VirtualDrive::resetControllers()
{
    intD_ = intQ_ = intSpd_ = 0.0;
    ud_ = uq_ = 0.0;
    idRef_ = iqRef_ = 0.0;
    spdRefRamped_ = speedMeasRpm_;
    posRef_ = posMeasDeg_;
    spdCmd_ = 0.0;
    thetaOl_ = thetaCtrl_;
    wOl_ = 0.0;
}

void VirtualDrive::onCommand(uint16_t cmd, const focp_byte_t* args, uint16_t n)
{
    switch (cmd) {
    case FOCP_CMD_ENABLE:
        if (state_ == FOCP_ST_READY && faults_ == 0) {
            resetControllers();
            startup_ = (static_cast<int>(dev_.param[FOCP_P_CTRL_POS_SENSOR]) == FOCP_SENS_SENSORLESS);
            setState(FOCP_ST_RUN);
        }
        break;
    case FOCP_CMD_DISABLE:
        if (state_ == FOCP_ST_RUN || state_ == FOCP_ST_CALIB) setState(FOCP_ST_READY);
        break;
    case FOCP_CMD_ACK_FAULT:
        if (state_ == FOCP_ST_FAULT) {
            focp_dev_send_event(&dev_, FOCP_EV_FAULT_CLEAR, faults_, ms());
            faults_ = 0;
            setState(FOCP_ST_READY);
        }
        break;
    case FOCP_CMD_SET_REF:
        if (n >= 4) ref_ = focp_get_f32(args);
        break;
    case FOCP_CMD_CALIBRATE:
        if (n >= 1 && state_ == FOCP_ST_READY) {
            calib_ = static_cast<uint16_t>(args[0] & 0xFF);
            resetControllers();
            calibAccF_ = calibAccB_ = 0.0;
            calibNF_ = calibNB_ = 0;
            setState(FOCP_ST_CALIB);
        }
        break;
    case FOCP_CMD_SAVE_PARAMS:
        focp_dev_send_event(&dev_, FOCP_EV_PARAMS_SAVED, 0, ms());
        break;
    default:
        break;
    }
}

// --- sensors -----------------------------------------------------------------------------
uint16_t VirtualDrive::hallStateFromAngle(double thetaEl) const
{
    const double s = wrap2Pi(thetaEl + kTrueHallOffsetDeg * kDeg);
    const int sector = clamp(static_cast<int>(s / (kPi / 3.0)), 0, 5);
    return kHallSeq[sector];
}

void VirtualDrive::updateHall(double dt)
{
    const double pp = dev_.param[FOCP_P_MOTOR_POLE_PAIRS];
    uint16_t code = hallStateFromAngle(pp * thm_);

    // spurious edges (noise on the hall lines, as observed in the thesis)
    const double pNoise = dev_.param[FOCP_P_SIM_HALL_NOISE];
    if (pNoise > 0.0 && std::uniform_real_distribution<double>(0, 1)(rng_) < pNoise)
        code ^= static_cast<uint16_t>(1u << std::uniform_int_distribution<int>(0, 2)(rng_));

    const int newSec = hallSector(code);
    const int oldSec = hallSector(hall_);
    hallInvalid_ = (newSec < 0);

    if (code != hall_ && newSec >= 0 && oldSec >= 0) {
        const int d = (newSec - oldSec + 6) % 6;
        const int dir = (d == 1) ? 1 : (d == 5 ? -1 : 0);
        if (dir != 0) {
            const double edge = (dir > 0 ? newSec : oldSec) * (kPi / 3.0);   // sensor frame
            // Timestamp the edge between the two control steps, like the eCAP counter does
            // (200 MHz on the target). Without this, the 50 us step quantises the speed.
            const double sNow = wrap2Pi(pp * thm_ + kTrueHallOffsetDeg * kDeg);
            const double sPrev = wrap2Pi(pp * thmPrev_ + kTrueHallOffsetDeg * kDeg);
            const double span = std::abs(wrapPi(sNow - sPrev));
            const double past = std::abs(wrapPi(sNow - edge));
            const double frac = (span > 1e-12 && past <= span) ? past / span : 0.0;
            const double tEdge = t_ - frac * dt;
            const double dtEdge = tEdge - hallEdgeTime_;
            hallSpeed_ = (dir == hallDir_ && dtEdge > 0) ? dir * (kPi / 3.0) / dtEdge : 0.0;
            hallDir_ = dir;
            hallEdgeAngle_ = edge;
            hallEdgeTime_ = tEdge;

            if (state_ == FOCP_ST_CALIB && calib_ == FOCP_CALIB_HALL) {
                // offset = sensor angle - electrical angle. The rotor lags the open-loop angle by
                // the load angle (friction); measuring in both directions cancels it.
                const double e = wrapPi(edge - thetaOl_);
                if (stateTime_ > 0.3 && stateTime_ < 1.3) { calibAccF_ += e; ++calibNF_; }
                if (stateTime_ > 1.6 && stateTime_ < 2.6) { calibAccB_ += e; ++calibNB_; }
            }
        } else {
            hallSpeed_ = 0.0;   // skipped sector -> implausible
        }
    }
    hall_ = code;

    // speed timeout: slower than hall.n_min -> standstill
    const double nMin = dev_.param[FOCP_P_HALL_N_MIN];
    const double wMinEl = std::max(1.0, nMin * kRpmToRad * pp);
    const double sinceEdge = t_ - hallEdgeTime_;
    if (sinceEdge > (kPi / 3.0) / wMinEl) hallSpeed_ = 0.0;

    // Speed for the control loops: if the next edge is overdue, the rotor is slower than
    // 60 deg / sinceEdge - use that as a decaying upper bound instead of holding the old value.
    hallSpeedCtrl_ = hallSpeed_;
    if (hallSpeed_ != 0.0 && sinceEdge > (kPi / 3.0) / std::abs(hallSpeed_))
        hallSpeedCtrl_ = (hallSpeed_ > 0 ? 1.0 : -1.0) * (kPi / 3.0) / sinceEdge;

    // angle: interpolate inside the sector, or sector centre at standstill
    double thetaSensor;
    if (hallSpeed_ != 0.0 && oldSec >= 0 && newSec >= 0) {
        double adv = hallSpeed_ * (t_ - hallEdgeTime_);
        adv = clamp(adv, -0.95 * kPi / 3.0, 0.95 * kPi / 3.0);   // never beyond the next edge
        thetaSensor = hallEdgeAngle_ + adv;
    } else if (newSec >= 0) {
        thetaSensor = (newSec + 0.5) * (kPi / 3.0);
    } else {
        thetaSensor = thetaHall_ + dev_.param[FOCP_P_HALL_OFFSET] * kDeg;   // hold
    }
    thetaHall_ = wrap2Pi(thetaSensor - dev_.param[FOCP_P_HALL_OFFSET] * kDeg);
    thmPrev_ = thm_;
}

void VirtualDrive::updateObserver(double dt)
{
    // Back-EMF based estimate: usable only when the EMF is large compared to noise.
    const double pp = dev_.param[FOCP_P_MOTOR_POLE_PAIRS];
    const double we = pp * wm_;
    const double emf = std::abs(we) * dev_.param[FOCP_P_MOTOR_PSI];
    std::normal_distribution<double> n01(0.0, 1.0);
    double meas;
    if (emf > 0.05) meas = pp * thm_ + n01(rng_) * std::min(1.0, 0.01 / emf);
    else            meas = std::uniform_real_distribution<double>(0, kTwoPi)(rng_);

    const double wn = kTwoPi * dev_.param[FOCP_P_OBS_PLL_BW];
    const double kp = 2.0 * 0.707 * wn, ki = wn * wn;
    const double err = wrapPi(meas - thetaObs_);
    speedObs_ += ki * err * dt;
    thetaObs_ = wrap2Pi(thetaObs_ + (speedObs_ + kp * err) * dt);

    errAvg_ += (std::abs(err) - errAvg_) * std::min(1.0, dt * 50.0);
    const double wLock = 0.5 * dev_.param[FOCP_P_OBS_N_SWITCH] * kRpmToRad * pp;
    obsLocked_ = (std::abs(speedObs_) > wLock) && errAvg_ < 0.2;
}

// --- plant -------------------------------------------------------------------------------
void VirtualDrive::stepPlant(double ud, double uq, double dt)
{
    const double pp = dev_.param[FOCP_P_MOTOR_POLE_PAIRS];
    const double rs = dev_.param[FOCP_P_MOTOR_RS];
    const double ld = dev_.param[FOCP_P_MOTOR_LD];
    const double lq = dev_.param[FOCP_P_MOTOR_LQ];
    const double psi = dev_.param[FOCP_P_MOTOR_PSI];
    const double j = dev_.param[FOCP_P_MOTOR_J];
    const double b = dev_.param[FOCP_P_MOTOR_B];
    const double tl = dev_.param[FOCP_P_SIM_T_LOAD];
    const double tf = dev_.param[FOCP_P_SIM_T_FRIC];

    constexpr int kSub = 4;
    const double h = dt / kSub;
    for (int k = 0; k < kSub; ++k) {
        const double we = pp * wm_;
        if (inverterOn_) {
            id_ += h * (ud - rs * id_ + we * lq * iq_) / ld;
            iq_ += h * (uq - rs * iq_ - we * ld * id_ - we * psi) / lq;
        } else {
            // gates off: currents commutate into the diodes and decay quickly
            id_ *= std::exp(-h / 2e-4);
            iq_ *= std::exp(-h / 2e-4);
        }
        torque_ = 1.5 * pp * (psi * iq_ + (ld - lq) * id_ * iq_);
        // Load and Coulomb friction oppose the motion; at rest they hold the shaft
        // as long as the motor torque does not exceed them (stiction).
        const double resist = std::abs(tl) + tf;
        if (std::abs(wm_) < 1e-6 && std::abs(torque_) <= resist) {
            wm_ = 0.0;
        } else {
            const double dir = std::abs(wm_) >= 1e-6 ? (wm_ > 0 ? 1.0 : -1.0) : (torque_ > 0 ? 1.0 : -1.0);
            const double w0 = wm_;
            wm_ += h * (torque_ - b * wm_ - dir * resist) / j;
            if (w0 != 0.0 && (w0 > 0) != (wm_ > 0) && std::abs(torque_) <= resist) wm_ = 0.0;   // stops, does not reverse
        }
        thm_ += h * wm_;
    }
}

// --- main step ---------------------------------------------------------------------------
void VirtualDrive::step()
{
    const double fpwm = dev_.param[FOCP_P_INV_F_PWM];
    const double dt = 1.0 / fpwm;
    const double pp = dev_.param[FOCP_P_MOTOR_POLE_PAIRS];
    const auto cyclesPerMs = static_cast<uint32_t>(fpwm / 1000.0);
    t_ += dt;
    stateTime_ += dt;
    ++cycle_;
    udc_ = dev_.param[FOCP_P_SIM_UDC];

    // timed supervisor transitions
    if (state_ == FOCP_ST_INIT && stateTime_ > 0.05) setState(FOCP_ST_OFFSET_CAL);
    if (state_ == FOCP_ST_OFFSET_CAL && stateTime_ > 0.2) setState(FOCP_ST_READY);

    updateHall(dt);
    updateObserver(dt);

    const int sensor = static_cast<int>(dev_.param[FOCP_P_CTRL_POS_SENSOR]);
    const int mode = static_cast<int>(dev_.param[FOCP_P_CTRL_MODE]);
    const double thetaTrue = pp * thm_;

    // protections
    if (state_ != FOCP_ST_INIT) {
        if (udc_ < dev_.param[FOCP_P_INV_UDC_MIN]) raiseFault(FOCP_FAULT_UNDERVOLTAGE);
        if (udc_ > dev_.param[FOCP_P_INV_UDC_MAX]) raiseFault(FOCP_FAULT_OVERVOLTAGE);
        if (temp_ > dev_.param[FOCP_P_INV_T_MAX]) raiseFault(FOCP_FAULT_OVERTEMP);
        if (std::hypot(id_, iq_) > dev_.param[FOCP_P_INV_I_MAX]) raiseFault(FOCP_FAULT_OC_SW);
        if (std::abs(wm_) / kRpmToRad > 1.1 * dev_.param[FOCP_P_MOTOR_N_MAX]) raiseFault(FOCP_FAULT_OVERSPEED);
    }
    const bool running = (state_ == FOCP_ST_RUN || state_ == FOCP_ST_CALIB);
    if (running && sensor != FOCP_SENS_SENSORLESS && hallInvalid_) raiseFault(FOCP_FAULT_HALL);
    if (cyclesPerMs && cycle_ % cyclesPerMs == 0) {
        if (focp_dev_tick_ms(&dev_, 1) && state_ == FOCP_ST_RUN) raiseFault(FOCP_FAULT_COMM);
    }

    inverterOn_ = (state_ == FOCP_ST_RUN || state_ == FOCP_ST_CALIB);

    // ---------------------------------------------------------------- position / speed sensing
    double speedMeasEl = hallSpeedCtrl_;
    double thetaMeas = thetaHall_;
    if (sensor == FOCP_SENS_SENSORLESS) {
        thetaMeas = thetaObs_;
        speedMeasEl = speedObs_;
    } else if (sensor == FOCP_SENS_HYBRID) {
        const double nAbs = std::abs((useObserver_ ? speedObs_ : hallSpeedCtrl_) / pp / kRpmToRad);
        const double nSw = dev_.param[FOCP_P_OBS_N_SWITCH], nHy = dev_.param[FOCP_P_OBS_N_HYST];
        if (!useObserver_ && nAbs > nSw + nHy && obsLocked_) useObserver_ = true;
        if (useObserver_ && (nAbs < nSw - nHy || !obsLocked_)) useObserver_ = false;
        if (useObserver_) { thetaMeas = thetaObs_; speedMeasEl = speedObs_; }
    }
    speedMeasRpm_ = speedMeasEl / pp / kRpmToRad;

    // ---------------------------------------------------------------- outer loops
    const double fSpd = 2000.0, fPos = 1000.0;
    const auto divSpd = std::max<uint32_t>(1, static_cast<uint32_t>(fpwm / fSpd));
    const auto divPos = std::max<uint32_t>(1, static_cast<uint32_t>(fpwm / fPos));
    const double iqMax = dev_.param[FOCP_P_SPD_IQ_MAX], iqMin = dev_.param[FOCP_P_SPD_IQ_MIN];
    idRef_ = dev_.param[FOCP_P_CUR_ID_REF];

    bool openLoop = false;
    if (state_ == FOCP_ST_CALIB) {
        openLoop = true;
        if (calib_ == FOCP_CALIB_HALL) {
            // rotate slowly in open loop, 1 s forward then 1 s backward (2 Hz electrical)
            wOl_ = (stateTime_ < 1.3 ? 1.0 : -1.0) * kTwoPi * 2.0;
            idRef_ = dev_.param[FOCP_P_OL_I];
            iqRef_ = 0.0;
            if (stateTime_ > 2.6) {
                if (calibNF_ >= 6 && calibNB_ >= 6) {
                    const double fwd = calibAccF_ / calibNF_, bwd = calibAccB_ / calibNB_;
                    const float off = static_cast<float>(wrapPi(0.5 * (fwd + bwd)) / kDeg);
                    focp_dev_write_param(&dev_, FOCP_P_HALL_OFFSET, off, 0);
                    focp_dev_send_event(&dev_, FOCP_EV_CALIB_DONE, calib_, ms());
                } else {
                    focp_dev_send_event(&dev_, FOCP_EV_CALIB_FAILED, calib_, ms());
                }
                setState(FOCP_ST_READY);
            }
        } else if (calib_ == FOCP_CALIB_CURRENT_OFFSET) {
            wOl_ = 0.0; idRef_ = 0.0; iqRef_ = 0.0;
            if (stateTime_ > 0.3) {
                focp_dev_send_event(&dev_, FOCP_EV_CALIB_DONE, calib_, ms());
                setState(FOCP_ST_READY);
            }
        } else {
            // motor identification: not yet available in the virtual drive
            focp_dev_send_event(&dev_, FOCP_EV_CALIB_FAILED, calib_, ms());
            setState(FOCP_ST_READY);
        }
    } else if (state_ == FOCP_ST_RUN) {
        const bool needStartup = (sensor == FOCP_SENS_SENSORLESS) && startup_;
        if (mode == FOCP_MODE_OPENLOOP || needStartup) {
            openLoop = true;
            double target = (mode == FOCP_MODE_OPENLOOP) ? ref_ : (ref_ >= 0 ? 1.0 : -1.0) * 1.2 * dev_.param[FOCP_P_OBS_N_SWITCH];
            const double wTarget = target * kRpmToRad * pp;
            const double acc = dev_.param[FOCP_P_OL_ACC] * kRpmToRad * pp * dt;
            wOl_ += clamp(wTarget - wOl_, -acc, acc);
            idRef_ = 0.0;
            iqRef_ = (wOl_ >= 0 ? 1.0 : -1.0) * dev_.param[FOCP_P_OL_I];
            if (needStartup && obsLocked_ && std::abs(wOl_) >= 0.99 * std::abs(wTarget)) {
                startup_ = false;                   // hand over to the observer
                spdRefRamped_ = speedObs_ / pp / kRpmToRad;
                intSpd_ = iqRef_;
            }
        } else if (mode == FOCP_MODE_TORQUE) {
            iqRef_ = clamp<double>(ref_, -iqMax, iqMax);
        } else if (mode == FOCP_MODE_SPEED || mode == FOCP_MODE_POSITION) {
            if (mode == FOCP_MODE_POSITION && cycle_ % divPos == 0) {
                posRef_ = ref_;
                const double vMax = dev_.param[FOCP_P_POS_V_MAX];
                const double e = posRef_ - posMeasDeg_;
                const double db = dev_.param[FOCP_P_POS_DEADBAND];
                const double eDb = std::abs(e) <= db ? 0.0 : e - (e > 0 ? db : -db);
                spdCmd_ = clamp(dev_.param[FOCP_P_POS_KP] * eDb, -vMax, vMax);
            }
            if (cycle_ % divSpd == 0) {
                const double dts = divSpd * dt;
                const double target = (mode == FOCP_MODE_SPEED) ? ref_ : spdCmd_;
                const double rmp = dev_.param[FOCP_P_SPD_RAMP] * dts;
                spdRefRamped_ += clamp(target - spdRefRamped_, -rmp, rmp);
                const double e = spdRefRamped_ - speedMeasRpm_;
                // limits refer to the direction of motion: Iq_min = braking limit
                const double hi = spdRefRamped_ >= 0 ? iqMax : -iqMin;
                const double lo = spdRefRamped_ >= 0 ? iqMin : -iqMax;
                intSpd_ = clamp(intSpd_ + dev_.param[FOCP_P_SPD_KI] * e * dts, lo, hi);
                iqRef_ = clamp(dev_.param[FOCP_P_SPD_KP] * e + intSpd_, lo, hi);
            }
        } else {
            iqRef_ = 0.0;
        }
    }

    if (openLoop) thetaOl_ = wrap2Pi(thetaOl_ + wOl_ * dt);
    const double thetaNew = openLoop ? thetaOl_ : thetaMeas;

    // multi-turn position from the angle the controller sees
    posAccEl_ += wrapPi(thetaNew - thetaCtrl_);
    thetaCtrl_ = thetaNew;
    posMeasDeg_ = posAccEl_ / pp / kDeg;

    // ---------------------------------------------------------------- current loop (controller frame)
    const double delta = thetaTrue - thetaCtrl_;
    const double cd = std::cos(delta), sd = std::sin(delta);
    idMeas_ = id_ * cd - iq_ * sd;
    iqMeas_ = id_ * sd + iq_ * cd;

    if (inverterOn_) {
        const double kp = dev_.param[FOCP_P_CUR_KP], ki = dev_.param[FOCP_P_CUR_KI];
        const double ed = idRef_ - idMeas_, eq = iqRef_ - iqMeas_;
        const double nIntD = intD_ + ki * ed * dt, nIntQ = intQ_ + ki * eq * dt;
        double ud = kp * ed + nIntD, uq = kp * eq + nIntQ;
        const double uMax = 0.95 * udc_ / std::sqrt(3.0);
        const double uAbs = std::hypot(ud, uq);
        if (uAbs > uMax) {                       // voltage limit + conditional integration
            ud *= uMax / uAbs;
            uq *= uMax / uAbs;
        } else {
            intD_ = nIntD;
            intQ_ = nIntQ;
        }
        ud_ = ud;
        uq_ = uq;
    } else {
        ud_ = uq_ = 0.0;
        intD_ = intQ_ = 0.0;
        if (!running) { iqRef_ = 0.0; intSpd_ = 0.0; spdRefRamped_ = speedMeasRpm_; }
    }
    // controller-frame voltage back into the true rotor frame
    const double udT = ud_ * cd + uq_ * sd;
    const double uqT = -ud_ * sd + uq_ * cd;
    stepPlant(udT, uqT, dt);

    // thermal model of the power stage (Rds_on 3 mOhm, 5 K/W, tau 30 s)
    const double pLoss = 1.5 * (id_ * id_ + iq_ * iq_) * 0.006;
    temp_ += dt * (25.0 + 5.0 * pLoss - temp_) / 30.0;

    // ---------------------------------------------------------------- telemetry
    const double ct = std::cos(thetaTrue), st = std::sin(thetaTrue);
    const double ia = id_ * ct - iq_ * st;
    const double ib_beta = id_ * st + iq_ * ct;
    const double ib = -0.5 * ia + 0.5 * std::sqrt(3.0) * ib_beta;
    const double cc = std::cos(thetaCtrl_), sc = std::sin(thetaCtrl_);
    const double ualpha = ud_ * cc - uq_ * sc;

    sig_[FOCP_S_IA] = float(ia);
    sig_[FOCP_S_IB] = float(ib);
    sig_[FOCP_S_IC] = float(-ia - ib);
    sig_[FOCP_S_ID] = float(idMeas_);
    sig_[FOCP_S_IQ] = float(iqMeas_);
    sig_[FOCP_S_ID_REF] = float(idRef_);
    sig_[FOCP_S_IQ_REF] = float(iqRef_);
    sig_[FOCP_S_UD] = float(ud_);
    sig_[FOCP_S_UQ] = float(uq_);
    sig_[FOCP_S_THETA_EL] = float(wrap2Pi(thetaCtrl_) / kDeg);
    sig_[FOCP_S_SPEED] = float(speedMeasRpm_);
    sig_[FOCP_S_SPEED_REF] = float(spdRefRamped_);
    sig_[FOCP_S_POS] = float(posMeasDeg_);
    sig_[FOCP_S_POS_REF] = float(posRef_);
    sig_[FOCP_S_UDC] = float(udc_);
    sig_[FOCP_S_TEMP] = float(temp_);
    sig_[FOCP_S_HALL_STATE] = float(hall_);
    sig_[FOCP_S_THETA_HALL] = float(thetaHall_ / kDeg);
    sig_[FOCP_S_THETA_OBS] = float(thetaObs_ / kDeg);
    sig_[FOCP_S_SPEED_OBS] = float(speedObs_ / pp / kRpmToRad);
    sig_[FOCP_S_DUTY_A] = float(clamp(0.5 + ualpha / udc_, 0.0, 1.0));
    sig_[FOCP_S_TORQUE] = float(torque_);
    focp_dev_sample(&dev_, sig_);

    // ---------------------------------------------------------------- cyclic status (10 Hz)
    if (cycle_ % static_cast<uint32_t>(fpwm / 10.0) == 0) {
        dev_.param[FOCP_P_SYS_ISR_LOAD] = 26.5f + float(std::uniform_real_distribution<double>(0, 1)(rng_));
        uint16_t stMode = static_cast<uint16_t>(mode);
        focp_dev_send_status(&dev_, state_, stMode, static_cast<uint16_t>(sensor), faults_,
                             float(udc_), float(temp_));
    }
}

double VirtualDrive::speedRpm() const { return wm_ / kRpmToRad; }
double VirtualDrive::positionDeg() const { return thm_ / kDeg; }

} // namespace fochost
