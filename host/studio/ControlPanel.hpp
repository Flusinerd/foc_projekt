#pragma once
#include <QGroupBox>

#include <array>

class DriveController;
class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QPushButton;
class QSlider;

/// Operation: enable (with safety confirmation), acknowledge, control mode, setpoint.
/// STOP lives in the main window toolbar so it is visible on every page.
class ControlPanel : public QGroupBox {
    Q_OBJECT
public:
    explicit ControlPanel(DriveController* ctl, QWidget* parent = nullptr);

private:
    void refresh();
    void onEnable();
    void onModeChosen(int index);
    void sendSetpoint();
    void setSetpoint(double v, bool fromSlider);
    void applyLimits(int mode);
    bool confirmSafety();

    DriveController* ctl_;
    QPushButton* enable_ = nullptr;
    QPushButton* ack_ = nullptr;
    QComboBox* mode_ = nullptr;
    QLabel* refLabel_ = nullptr;
    QDoubleSpinBox* ref_ = nullptr;
    QSlider* slider_ = nullptr;
    QPushButton* zero_ = nullptr;
    QPushButton* send_ = nullptr;
    QLabel* hint_ = nullptr;

    std::array<double, 5> refPerMode_{{0.0, 1.0, 1000.0, 0.0, 300.0}};
    int shownMode_ = -1;
    double lo_ = -1.0, hi_ = 1.0;
    unsigned shownRefSerial_ = 0;
    bool safetyConfirmed_ = false;   ///< once per program session
    bool updating_ = false;
};
