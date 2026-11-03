#include "DashboardPage.hpp"

#include "DriveController.hpp"
#include "FaultText.hpp"

#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLocale>
#include <QVBoxLayout>

DashboardPage::DashboardPage(DriveController* ctl, QWidget* parent) : QWidget(parent), ctl_(ctl)
{
    auto* root = new QVBoxLayout(this);

    state_ = new QLabel;
    state_->setAlignment(Qt::AlignCenter);
    state_->setMinimumHeight(72);
    QFont f = state_->font();
    f.setPointSizeF(f.pointSizeF() * 2.2);
    f.setBold(true);
    state_->setFont(f);
    state_->setAccessibleName(tr("Drive state"));
    root->addWidget(state_);

    auto* grid = new QGridLayout;
    speed_ = addTile(grid, 0, 0, tr("Speed"));
    udc_ = addTile(grid, 0, 1, tr("Supply voltage"));
    temp_ = addTile(grid, 0, 2, tr("Temperature"));
    mode_ = addTile(grid, 1, 0, tr("Control mode"));
    sensor_ = addTile(grid, 1, 1, tr("Position sensor"));
    root->addLayout(grid);

    auto* faults = new QGroupBox(tr("Active faults"));
    faultList_ = new QVBoxLayout(faults);
    noFaults_ = new QLabel(tr("No active faults."));
    faultList_->addWidget(noFaults_);
    root->addWidget(faults);
    root->addStretch(1);

    connect(ctl_, &DriveController::updated, this, &DashboardPage::refresh);
    refresh();
}

QLabel* DashboardPage::addTile(QGridLayout* grid, int row, int col, const QString& caption)
{
    auto* box = new QFrame;
    box->setFrameShape(QFrame::StyledPanel);
    auto* l = new QVBoxLayout(box);
    auto* cap = new QLabel(caption);
    cap->setEnabled(false);   // greyed caption
    auto* value = new QLabel(QStringLiteral("–"));
    QFont f = value->font();
    f.setPointSizeF(f.pointSizeF() * 1.8);
    value->setFont(f);
    value->setTextInteractionFlags(Qt::TextSelectableByMouse);
    value->setAccessibleName(caption);
    l->addWidget(cap);
    l->addWidget(value);
    grid->addWidget(box, row, col);
    return value;
}

void DashboardPage::refresh()
{
    const bool conn = ctl_->isConnected();
    const auto& st = ctl_->status();
    const bool valid = conn && st.valid;
    const QString dash = QStringLiteral("–");

    const QColor bg = valid ? DriveTexts::stateColor(st.state) : QColor(0x6b, 0x6b, 0x6b);
    state_->setText(!conn ? tr("Not connected") : valid ? DriveTexts::state(st.state) : tr("Waiting for drive…"));
    state_->setStyleSheet(QStringLiteral("QLabel { background:%1; color:white; border-radius:6px; padding:8px; }")
                              .arg(bg.name()));

    const QLocale loc;   // decimal comma in German
    speed_->setText(conn && ctl_->speedValid() ? tr("%1 rpm").arg(loc.toString(ctl_->speed(), 'f', 0)) : dash);
    udc_->setText(valid ? tr("%1 V").arg(loc.toString(double(st.udc), 'f', 1)) : dash);
    temp_->setText(valid ? tr("%1 °C").arg(loc.toString(double(st.temp), 'f', 1)) : dash);
    mode_->setText(valid ? DriveTexts::mode(st.mode) : dash);
    sensor_->setText(valid ? DriveTexts::sensor(st.sensor) : dash);

    // Rebuild the fault list only when the bits change.
    const uint16_t faults = valid ? st.faults : 0;
    if (faults == shownFaults_) return;
    shownFaults_ = faults;
    while (faultList_->count() > 1) {
        QLayoutItem* it = faultList_->takeAt(1);
        delete it->widget();
        delete it;
    }
    noFaults_->setVisible(faults == 0);
    for (const FaultText& ft : DriveTexts::faults(faults)) {
        auto* l = new QLabel(QStringLiteral("<b style='color:#c62b2b'>%1</b><br>%2")
                                 .arg(ft.title.toHtmlEscaped(), ft.action.toHtmlEscaped()));
        l->setWordWrap(true);
        l->setTextFormat(Qt::RichText);
        faultList_->addWidget(l);
    }
}
