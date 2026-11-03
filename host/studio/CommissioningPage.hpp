#pragma once
#include <QWidget>

class DriveController;
class QComboBox;
class QLabel;
class QPushButton;

/// Expert: position sensor choice and calibrations (current offset, Hall offset, motor ID).
class CommissioningPage : public QWidget {
    Q_OBJECT
public:
    explicit CommissioningPage(DriveController* ctl, QWidget* parent = nullptr);

private:
    void refresh();

    DriveController* ctl_;
    QComboBox* sensor_ = nullptr;
    QPushButton* offset_ = nullptr;
    QPushButton* hall_ = nullptr;
    QPushButton* motorId_ = nullptr;
    QLabel* hallValue_ = nullptr;
    QLabel* state_ = nullptr;
};
