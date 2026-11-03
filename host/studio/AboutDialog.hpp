#pragma once
#include <QDialog>

class DriveController;

/// Program, protocol and drive firmware version; licenses of the used libraries.
class AboutDialog : public QDialog {
    Q_OBJECT
public:
    explicit AboutDialog(DriveController* ctl, QWidget* parent = nullptr);
};
