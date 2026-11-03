#pragma once
#include <fochost/Device.hpp>

#include <QObject>
#include <QString>
#include <QTimer>

#include <vector>

/// Owns the Qt-free fochost::Device and polls it at ~30 Hz. Pages read the cached
/// snapshot after `updated()`; commands go straight to the device.
class DriveController : public QObject {
    Q_OBJECT
public:
    explicit DriveController(QObject* parent = nullptr);
    ~DriveController() override;

    bool connectTo(const fochost::TransportConfig& cfg, int node, QString& error);
    void disconnectFrom();
    bool isConnected() const { return dev_.connected(); }

    void stop() { if (dev_.connected()) dev_.disable(); }

    /// Sends the setpoint and remembers it, so every view shows what the drive really got.
    void setReference(float v);
    float lastReference() const { return lastRef_; }
    unsigned referenceSerial() const { return refSerial_; }   ///< changes with every send

    /// Connect to the virtual drive and run the demo profile (Hall calibration, speed steps, load).
    bool startDemo(QString& error);

    fochost::Device& device() { return dev_; }

    /// Expert mode: editing parameters, full scope, calibrations, virtual drive settings.
    bool expertMode() const { return expert_; }
    void setExpertMode(bool on)
    {
        if (on == expert_) return;
        expert_ = on;
        Q_EMIT expertModeChanged(on);
    }

    // snapshot, refreshed every poll
    const fochost::DriveStatus& status() const { return status_; }
    const fochost::DriveInfo& info() const { return info_; }
    const std::vector<fochost::ParamState>& params() const { return params_; }
    bool speedValid() const { return speedValid_; }
    double speed() const { return speed_; }
    QString transportName() const { return QString::fromStdString(dev_.transportName()); }
    int node() const { return node_; }

Q_SIGNALS:
    void connectionChanged(bool connected);
    void expertModeChanged(bool on);
    void updated();

private:
    void poll();
    void startTelemetry();
    void demoStep();

    fochost::Device dev_;
    QTimer timer_;
    int node_ = 1;

    fochost::DriveStatus status_;
    fochost::DriveInfo info_;
    std::vector<fochost::ParamState> params_;
    bool speedValid_ = false;
    double speed_ = 0.0;
    float lastRef_ = 0.0f;
    unsigned refSerial_ = 0;

    bool expert_ = false;
    bool demo_ = false;
    int demoPhase_ = -1;
    qint64 demoT0_ = 0;
};
