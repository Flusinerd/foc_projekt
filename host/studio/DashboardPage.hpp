#pragma once
#include <QWidget>

#include <cstdint>

class DriveController;
class QGridLayout;
class QLabel;
class QVBoxLayout;

/// Overview: state (large, coloured), key values, active faults with instructions.
class DashboardPage : public QWidget {
    Q_OBJECT
public:
    explicit DashboardPage(DriveController* ctl, QWidget* parent = nullptr);

private:
    void refresh();
    QLabel* addTile(QGridLayout* grid, int row, int col, const QString& caption);

    DriveController* ctl_;
    QLabel* state_ = nullptr;
    QLabel* speed_ = nullptr;
    QLabel* udc_ = nullptr;
    QLabel* temp_ = nullptr;
    QLabel* mode_ = nullptr;
    QLabel* sensor_ = nullptr;
    QLabel* noFaults_ = nullptr;
    QVBoxLayout* faultList_ = nullptr;
    uint16_t shownFaults_ = 0xFFFF;
};
