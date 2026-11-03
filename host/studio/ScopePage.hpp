#pragma once
#include <QWidget>

class DriveController;
class QCheckBox;
class QDoubleSpinBox;
class QLabel;
class QListWidget;
class QSpinBox;
class ScopeWidget;

/// Expert scope: signal selection (max. 8 incl. speed), sample rate, time window, pause,
/// clear, CSV export.
class ScopePage : public QWidget {
    Q_OBJECT
public:
    explicit ScopePage(DriveController* ctl, QWidget* parent = nullptr);

private:
    void syncFromController();
    void applySelection();
    void updateRateLabel();
    void exportCsv();

    DriveController* ctl_;
    QListWidget* signals_ = nullptr;
    QSpinBox* divider_ = nullptr;
    QLabel* rate_ = nullptr;
    QDoubleSpinBox* window_ = nullptr;
    QCheckBox* pause_ = nullptr;
    QLabel* lost_ = nullptr;
    ScopeWidget* scope_ = nullptr;
    bool updating_ = false;
};
