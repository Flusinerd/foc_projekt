#include "ScopeWidget.hpp"

#include "DriveController.hpp"

#include <QLabel>
#include <QVBoxLayout>

#include <qwt_legend.h>
#include <qwt_plot.h>
#include <qwt_plot_curve.h>
#include <qwt_plot_grid.h>
#include <qwt_plot_legenditem.h>

#include <algorithm>
#include <map>

namespace {
constexpr size_t kMaxPoints = 50000;   // window copy limit; Qwt reduces to pixel columns anyway

/// By position within one plot, so curves sharing a plot are always well apart.
QColor curveColor(size_t n)
{
    static const QColor palette[] = {QColor(0x1f, 0x77, 0xb4), QColor(0xd6, 0x27, 0x28), QColor(0x2c, 0xa0, 0x2c),
                                     QColor(0xff, 0x7f, 0x0e), QColor(0x94, 0x67, 0xbd), QColor(0x8c, 0x56, 0x4b),
                                     QColor(0xe3, 0x77, 0xc2), QColor(0x17, 0xbe, 0xcf)};
    return palette[n % 8];
}
} // namespace

ScopeWidget::ScopeWidget(DriveController* ctl, QWidget* parent) : QWidget(parent), ctl_(ctl)
{
    layout_ = new QVBoxLayout(this);
    layout_->setContentsMargins(0, 0, 0, 0);
    empty_ = new QLabel;
    empty_->setAlignment(Qt::AlignCenter);
    empty_->setEnabled(false);
    layout_->addWidget(empty_);
    connect(ctl_, &DriveController::updated, this, &ScopeWidget::refresh);
}

void ScopeWidget::setWanted(std::vector<int> signalIdx)
{
    wanted_ = std::move(signalIdx);
    shownSlots_.clear();   // force rebuild
    rebuild({});
}

void ScopeWidget::rebuild(const std::vector<int>& slotSignals)
{
    for (Plot& p : plots_) delete p.plot;
    plots_.clear();
    shownSlots_ = slotSignals;

    std::map<QString, size_t> byUnit;
    for (size_t k = 0; k < slotSignals.size(); ++k) {
        const int sig = slotSignals[k];
        if (sig < 0) continue;
        if (!wanted_.empty() && std::find(wanted_.begin(), wanted_.end(), sig) == wanted_.end()) continue;
        const auto& info = focp_signal_table[sig];
        QString unit = QString::fromUtf8(info.unit);
        if (unit.isEmpty()) unit = QStringLiteral("–");
        auto it = byUnit.find(unit);
        if (it == byUnit.end()) {
            auto* plot = new QwtPlot;
            plot->setAutoReplot(false);
            plot->setCanvasBackground(Qt::white);
            plot->setAxisTitle(QwtAxis::YLeft, unit);
            plot->setAxisAutoScale(QwtAxis::YLeft, true);
            plot->setMinimumHeight(120);
            auto* grid = new QwtPlotGrid;
            grid->setPen(QColor(0xdd, 0xdd, 0xdd), 0.0, Qt::DotLine);
            grid->attach(plot);
            auto* legend = new QwtPlotLegendItem;
            legend->setAlignmentInCanvas(Qt::AlignTop | Qt::AlignLeft);
            legend->setBackgroundBrush(QColor(255, 255, 255, 200));
            legend->attach(plot);
            layout_->addWidget(plot, 1);
            plots_.push_back({plot, {}});
            it = byUnit.emplace(unit, plots_.size() - 1).first;
        }
        auto* c = new QwtPlotCurve(QString::fromUtf8(info.name));
        c->setPen(curveColor(plots_[it->second].curves.size()), 1.5);
        c->setPaintAttribute(QwtPlotCurve::FilterPointsAggressive, true);
        c->attach(plots_[it->second].plot);
        plots_[it->second].curves.emplace_back(int(k), c);
    }
    empty_->setVisible(plots_.empty());
    empty_->setText(!ctl_->isConnected() ? tr("Not connected.") : tr("No signal selected or streamed."));
}

void ScopeWidget::refresh()
{
    if (!isVisible()) return;
    if (!ctl_->isConnected()) {
        if (!plots_.empty() || empty_->text().isEmpty()) rebuild({});
        empty_->setText(tr("Not connected."));
        return;
    }
    if (paused_) return;

    std::vector<int> slotSig;
    double tLast = 0;
    ctl_->device().withScope([&](const fochost::ScopeBuffer& s) {
        slotSig = s.signals;
        tLast = s.tLast();
        s.copyWindow(tLast - window_, kMaxPoints, t_, y_);
    });
    if (slotSig != shownSlots_) rebuild(slotSig);

    for (Plot& p : plots_) {
        for (auto& [slot, curve] : p.curves)
            if (size_t(slot) < y_.size()) curve->setRawSamples(t_.data(), y_[size_t(slot)].data(), int(t_.size()));
        p.plot->setAxisScale(QwtAxis::XBottom, tLast - window_, tLast);
        p.plot->replot();
    }
}
