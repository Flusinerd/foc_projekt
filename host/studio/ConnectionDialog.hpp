#pragma once
#include <fochost/Transport.hpp>

#include <QDialog>

#include <vector>

class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;

/// Choose adapter, channel, bitrate and node; search for CANable adapters; test the
/// connection (waits for the drive's INFO answer). Remembers the last choice (QSettings).
class ConnectionDialog : public QDialog {
    Q_OBJECT
public:
    explicit ConnectionDialog(QWidget* parent = nullptr);

    fochost::TransportConfig config() const;
    int node() const;

private:
    void updateFields();
    void searchAdapters();
    void testConnection();
    void load();
    void accept() override;

    std::vector<fochost::TransportKind> kinds_;
    QComboBox* transport_ = nullptr;
    QLabel* channelLabel_ = nullptr;
    QLineEdit* channel_ = nullptr;
    QLabel* adapterLabel_ = nullptr;
    QComboBox* adapter_ = nullptr;
    QPushButton* search_ = nullptr;
    QLabel* bitrateLabel_ = nullptr;
    QComboBox* bitrate_ = nullptr;
    QSpinBox* node_ = nullptr;
    QLabel* result_ = nullptr;
    QString savedAdapter_;
};
