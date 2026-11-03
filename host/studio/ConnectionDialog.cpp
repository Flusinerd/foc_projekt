#include "ConnectionDialog.hpp"

#include <fochost/Device.hpp>
#include <fochost/GsUsb.hpp>

#include <QApplication>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QElapsedTimer>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QThread>

using namespace fochost;

namespace {
const int kBitrates[] = {125000, 250000, 500000, 1000000};
constexpr int kTestTimeoutMs = 1500;
}

ConnectionDialog::ConnectionDialog(QWidget* parent) : QDialog(parent), kinds_(availableTransports())
{
    setWindowTitle(tr("Connect to drive"));
    auto* form = new QFormLayout(this);

    transport_ = new QComboBox;
    for (TransportKind k : kinds_) {
        switch (k) {
        case TransportKind::Simulation: transport_->addItem(tr("Virtual drive (simulation)")); break;
        case TransportKind::GsUsb:      transport_->addItem(tr("CANable / candleLight (USB)")); break;
        default:                        transport_->addItem(QString::fromUtf8(transportName(k))); break;
        }
    }
    form->addRow(tr("&Interface:"), transport_);

    adapter_ = new QComboBox;
    adapter_->setMinimumContentsLength(24);
    search_ = new QPushButton(tr("&Search"));
    auto* adapterRow = new QWidget;
    auto* ar = new QHBoxLayout(adapterRow);
    ar->setContentsMargins(0, 0, 0, 0);
    ar->addWidget(adapter_, 1);
    ar->addWidget(search_);
    adapterLabel_ = new QLabel(tr("&Adapter:"));
    adapterLabel_->setBuddy(adapter_);
    form->addRow(adapterLabel_, adapterRow);

    channel_ = new QLineEdit;
    channelLabel_ = new QLabel(tr("&Channel:"));
    channelLabel_->setBuddy(channel_);
    form->addRow(channelLabel_, channel_);

    bitrate_ = new QComboBox;
    for (int br : kBitrates)
        bitrate_->addItem(br >= 1000000 ? tr("%1 Mbit/s").arg(br / 1000000) : tr("%1 kbit/s").arg(br / 1000), br);
    bitrateLabel_ = new QLabel(tr("&Bitrate:"));
    bitrateLabel_->setBuddy(bitrate_);
    form->addRow(bitrateLabel_, bitrate_);

    node_ = new QSpinBox;
    node_->setRange(0, 15);
    form->addRow(tr("&Node:"), node_);

    result_ = new QLabel;
    result_->setWordWrap(true);
    form->addRow(result_);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    buttons->button(QDialogButtonBox::Ok)->setText(tr("Co&nnect"));
    auto* test = buttons->addButton(tr("&Test connection"), QDialogButtonBox::ActionRole);
    connect(buttons, &QDialogButtonBox::accepted, this, &ConnectionDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &ConnectionDialog::reject);
    connect(test, &QPushButton::clicked, this, &ConnectionDialog::testConnection);
    connect(search_, &QPushButton::clicked, this, &ConnectionDialog::searchAdapters);
    form->addRow(buttons);

    connect(transport_, &QComboBox::currentIndexChanged, this, &ConnectionDialog::updateFields);
    load();
    searchAdapters();
    updateFields();
}

void ConnectionDialog::load()
{
    QSettings s;
    s.beginGroup(QStringLiteral("connection"));
    const int kind = s.value(QStringLiteral("kind"), int(TransportKind::Simulation)).toInt();
    for (int i = 0; i < int(kinds_.size()); ++i)
        if (int(kinds_[size_t(i)]) == kind) transport_->setCurrentIndex(i);
    channel_->setText(s.value(QStringLiteral("channel")).toString());
    savedAdapter_ = s.value(QStringLiteral("adapter")).toString();
    const int br = bitrate_->findData(s.value(QStringLiteral("bitrate"), 500000).toInt());
    bitrate_->setCurrentIndex(br >= 0 ? br : 2);
    node_->setValue(s.value(QStringLiteral("node"), 1).toInt());
}

void ConnectionDialog::accept()
{
    QSettings s;
    s.beginGroup(QStringLiteral("connection"));
    s.setValue(QStringLiteral("kind"), int(config().kind));
    s.setValue(QStringLiteral("channel"), channel_->text());
    s.setValue(QStringLiteral("adapter"), adapter_->currentData().toString());
    s.setValue(QStringLiteral("bitrate"), bitrate_->currentData().toInt());
    s.setValue(QStringLiteral("node"), node_->value());
    QDialog::accept();
}

void ConnectionDialog::searchAdapters()
{
    // Only lists the devices; nothing is opened.
    const QString keep = adapter_->count() ? adapter_->currentData().toString() : savedAdapter_;
    adapter_->clear();
    const auto devs = gsusb::listDevices();
    for (size_t i = 0; i < devs.size(); ++i)
        adapter_->addItem(tr("%1 (#%2)").arg(QString::fromStdString(devs[i].description)).arg(i + 1),
                          QString::fromStdString(devs[i].path));
    if (devs.empty()) adapter_->addItem(tr("No adapter found"), QString());
    const int k = adapter_->findData(keep);
    if (k >= 0) adapter_->setCurrentIndex(k);
}

void ConnectionDialog::updateFields()
{
    const TransportKind k = config().kind;
    const bool gs = k == TransportKind::GsUsb;
    const bool chan = k == TransportKind::SocketCAN || k == TransportKind::PCAN;
    adapterLabel_->setVisible(gs);
    adapter_->parentWidget()->setVisible(gs);
    channelLabel_->setVisible(chan);
    channel_->setVisible(chan);
    bitrateLabel_->setVisible(k == TransportKind::PCAN || gs);
    bitrate_->setVisible(k == TransportKind::PCAN || gs);
    if (chan && channel_->text().isEmpty())
        channel_->setText(k == TransportKind::PCAN ? QStringLiteral("USB1") : QStringLiteral("can0"));
    result_->clear();
}

void ConnectionDialog::testConnection()
{
    // Opens the interface, asks for the drive's INFO and disconnects again. No command
    // other than INFO, parameter reads and heartbeat is sent.
    result_->setText(tr("Testing…"));
    QApplication::setOverrideCursor(Qt::WaitCursor);
    Device dev;
    std::string err;
    QString msg;
    bool ok = false;
    if (!dev.connect(config(), uint16_t(node()), err)) {
        msg = tr("Interface could not be opened: %1").arg(QString::fromStdString(err));
    } else {
        QElapsedTimer t;
        t.start();
        while (!dev.info().valid && t.elapsed() < kTestTimeoutMs) {
            QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
            QThread::msleep(10);
        }
        const DriveInfo inf = dev.info();
        ok = inf.valid;
        msg = ok ? tr("Drive answers: firmware %1.%2, protocol v%3.").arg(inf.fwMajor).arg(inf.fwMinor).arg(inf.protocol)
                 : tr("Interface open, but node %1 does not answer. Check bitrate, node number, wiring and termination.")
                       .arg(node());
        if (ok && inf.protocol != int(FOCP_PROTOCOL_VERSION))
            msg += QLatin1Char(' ') + tr("Protocol version differs from this program (v%1).").arg(FOCP_PROTOCOL_VERSION);
        dev.disconnect();
    }
    QApplication::restoreOverrideCursor();
    result_->setStyleSheet(ok ? QStringLiteral("color:#2e7d32") : QStringLiteral("color:#c62b2b"));
    result_->setText(msg);
}

TransportConfig ConnectionDialog::config() const
{
    TransportConfig c;
    const int i = transport_->currentIndex();
    c.kind = i >= 0 ? kinds_[size_t(i)] : TransportKind::Simulation;
    c.channel = c.kind == TransportKind::GsUsb ? adapter_->currentData().toString().toStdString()
                                               : channel_->text().toStdString();
    c.bitrate = bitrate_->currentData().toInt();
    return c;
}

int ConnectionDialog::node() const { return node_->value(); }
