// Plot benchmark: JKQtPlotter. Options see bench_common.hpp.
//   --fast  no grid, no toolbar/mouse position (JKQtPlotter 4.0 has no point reduction)
#include "bench_common.hpp"

#include <QVBoxLayout>

#include <jkqtplotter/graphs/jkqtpscatter.h>
#include <jkqtplotter/jkqtplotter.h>

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    const bench::Options o = bench::parseArgs(app.arguments());
    bench::Source src;

    QWidget window;
    window.setWindowTitle("plotbench – JKQtPlotter");
    auto* layout = new QVBoxLayout(&window);
    std::vector<JKQTPlotter*> plots;
    for (int p = 0; p < o.plots; ++p) {
        auto* plot = new JKQTPlotter;
        plot->setToolbarEnabled(false);
        plot->setMousePositionShown(false);
        plot->getPlotter()->setUseAntiAliasingForGraphs(o.aa);
        plot->getPlotter()->setUseAntiAliasingForSystem(false);
        plot->getPlotter()->setShowKey(false);
        if (o.fast) plot->setGrid(false);
        plot->setY(-2.0, 2.0);
        layout->addWidget(plot);
        plots.push_back(plot);
    }
    // All plots reference the same external arrays (addColumn(double*, ...) does not copy).
    for (int s = 0; s < bench::kSignals; ++s) {
        JKQTPlotter* plot = plots[size_t(s * o.plots / bench::kSignals)];
        JKQTPDatastore* ds = plot->getDatastore();
        auto* g = new JKQTPXYLineGraph(plot);
        g->setXColumn(ds->addColumn(src.x(), bench::kWindow, "t"));
        g->setYColumn(ds->addColumn(src.y(s), bench::kWindow, QString("y%1").arg(s)));
        g->setSymbolType(JKQTPNoSymbol);
        g->setLineColor(QColor::fromHsv(s * 45, 220, 200));
        g->setLineWidth(1.0);
        plot->addGraph(g);
    }

    return bench::run(app, window, "jkqt", o, src, [&] {
        for (auto* plot : plots) {
            plot->setPlotUpdateEnabled(false);
            plot->setX(src.tMin(), src.tMax());
            plot->setPlotUpdateEnabled(true);
            plot->redrawPlot();  // renders into the internal image
            plot->repaint();     // blit synchronously
        }
    });
}
