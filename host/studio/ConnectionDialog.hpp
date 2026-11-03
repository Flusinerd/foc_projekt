#pragma once
#include <fochost/Transport.hpp>

#include <QDialog>

#include <vector>

class QComboBox;
class QLabel;
class QLineEdit;
class QSpinBox;

/// Choose adapter, channel, bitrate and node. Remembers the last choice (QSettings).
/// Adapter search and the connection test come with the CANable transport.
class ConnectionDialog : public QDialog {
    Q_OBJECT
public:
    explicit ConnectionDialog(QWidget* parent = nullptr);

    fochost::TransportConfig config() const;
    int node() const;

private:
    void updateFields();
    void load();
    void accept() override;

    std::vector<fochost::TransportKind> kinds_;
    QComboBox* transport_ = nullptr;
    QLabel* channelLabel_ = nullptr;
    QLineEdit* channel_ = nullptr;
    QLabel* bitrateLabel_ = nullptr;
    QComboBox* bitrate_ = nullptr;
    QSpinBox* node_ = nullptr;
};
