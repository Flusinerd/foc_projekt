#pragma once
#include <QWidget>

#include <vector>

class DriveController;
class QLabel;
class QVBoxLayout;
class QwtPlot;
class QwtPlotCurve;

/// Live plots of streamed telemetry, one plot per unit. Shows the wanted signals that are
/// currently streamed. Redraws only while visible (Qwt, min/max reduction per pixel column).
class ScopeWidget : public QWidget {
    Q_OBJECT
public:
    explicit ScopeWidget(DriveController* ctl, QWidget* parent = nullptr);

    /// Signals to show (focp_signal_idx_t); empty = everything that is streamed.
    void setWanted(std::vector<int> signalIdx);
    void setWindow(double seconds) { window_ = seconds; }
    double window() const { return window_; }
    void setPaused(bool p) { paused_ = p; }
    bool paused() const { return paused_; }

private:
    void refresh();
    void rebuild(const std::vector<int>& slotSignals);

    struct Plot {
        QwtPlot* plot;
        std::vector<std::pair<int, QwtPlotCurve*>> curves;   ///< (slot, curve)
    };

    DriveController* ctl_;
    QVBoxLayout* layout_ = nullptr;
    QLabel* empty_ = nullptr;
    std::vector<int> wanted_;
    std::vector<int> shownSlots_;   ///< slot signals the plots were built for
    std::vector<Plot> plots_;
    std::vector<double> t_;
    std::vector<std::vector<double>> y_;
    double window_ = 5.0;
    bool paused_ = false;
};
