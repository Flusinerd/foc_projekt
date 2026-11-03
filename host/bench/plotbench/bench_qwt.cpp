// Plot benchmark: Qwt. Options see bench_common.hpp.
//   --fast  QwtPlotCurve::FilterPointsAggressive (pixel-column min/max reduction)
#include "bench_common.hpp"

#include <QVBoxLayout>

#include <qwt_plot.h>
#include <qwt_plot_canvas.h>
#include <qwt_plot_curve.h>
#include <qwt_plot_grid.h>

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    const bench::Options o = bench::parseArgs(app.arguments());
    bench::Source src;

    QWidget window;
    window.setWindowTitle("plotbench – Qwt");
    auto* layout = new QVBoxLayout(&window);
    std::vector<QwtPlot*> plots;
    std::vector<QwtPlotCurve*> curves;
    for (int p = 0; p < o.plots; ++p) {
        auto* plot = new QwtPlot;
        plot->setAutoReplot(false);
        auto* canvas = new QwtPlotCanvas;
        canvas->setPaintAttribute(QwtPlotCanvas::ImmediatePaint, true);  // replot() paints synchronously
        plot->setCanvas(canvas);
        plot->setAxisScale(QwtAxis::YLeft, -2.0, 2.0);
        (new QwtPlotGrid)->attach(plot);
        layout->addWidget(plot);
        plots.push_back(plot);
    }
    for (int s = 0; s < bench::kSignals; ++s) {
        auto* c = new QwtPlotCurve;
        c->setPen(QColor::fromHsv(s * 45, 220, 200), 1.0);
        c->setRenderHint(QwtPlotItem::RenderAntialiased, o.aa);
        if (o.fast) c->setPaintAttribute(QwtPlotCurve::FilterPointsAggressive, true);
        c->setRawSamples(src.x(), src.y(s), bench::kWindow);  // no copy, arrays stay in place
        c->attach(plots[size_t(s * o.plots / bench::kSignals)]);
        curves.push_back(c);
    }

    return bench::run(app, window, "qwt", o, src, [&] {
        for (auto* plot : plots) {
            plot->setAxisScale(QwtAxis::XBottom, src.tMin(), src.tMax());
            plot->replot();
        }
    });
}
