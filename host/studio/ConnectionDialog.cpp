#include "ConnectionDialog.hpp"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>

using namespace fochost;

namespace {
const int kBitrates[] = {125000, 250000, 500000, 1000000};
}

ConnectionDialog::ConnectionDialog(QWidget* parent) : QDialog(parent), kinds_(availableTransports())
{
    setWindowTitle(tr("Connect to drive"));
    auto* form = new QFormLayout(this);

    transport_ = new QComboBox;
    for (TransportKind k : kinds_) {
        // Built-in names of the core; the simulation gets a user-facing label.
        transport_->addItem(k == TransportKind::Simulation ? tr("Virtual drive (simulation)")
                                                           : QString::fromUtf8(transportName(k)));
    }
    form->addRow(tr("&Adapter:"), transport_);

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

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    buttons->button(QDialogButtonBox::Ok)->setText(tr("Co&nnect"));
    connect(buttons, &QDialogButtonBox::accepted, this, &ConnectionDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &ConnectionDialog::reject);
    form->addRow(buttons);

    connect(transport_, &QComboBox::currentIndexChanged, this, &ConnectionDialog::updateFields);
    load();
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
    s.setValue(QStringLiteral("bitrate"), bitrate_->currentData().toInt());
    s.setValue(QStringLiteral("node"), node_->value());
    QDialog::accept();
}

void ConnectionDialog::updateFields()
{
    const TransportKind k = config().kind;
    const bool can = k != TransportKind::Simulation;
    channelLabel_->setVisible(can);
    channel_->setVisible(can);
    bitrateLabel_->setVisible(k == TransportKind::PCAN);
    bitrate_->setVisible(k == TransportKind::PCAN);
    if (can && channel_->text().isEmpty())
        channel_->setText(k == TransportKind::PCAN ? QStringLiteral("USB1") : QStringLiteral("can0"));
}

TransportConfig ConnectionDialog::config() const
{
    TransportConfig c;
    const int i = transport_->currentIndex();
    c.kind = i >= 0 ? kinds_[size_t(i)] : TransportKind::Simulation;
    c.channel = channel_->text().toStdString();
    c.bitrate = bitrate_->currentData().toInt();
    return c;
}

int ConnectionDialog::node() const { return node_->value(); }
