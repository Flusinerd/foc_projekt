// Shared data source and measurement for the plot benchmark.
//
// Load: 8 signals, 20 000 points each in the visible window, 30 frames/s, new samples
// pushed every frame (fs = 10 kHz -> window 2 s, ~333 new points per signal and frame).
// Every frame is drawn synchronously inside the timer slot, so the measured frame time
// contains data update + full replot + paint.
#pragma once

#include <QApplication>
#include <QElapsedTimer>
#include <QStringList>
#include <QTimer>
#include <QWidget>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <functional>
#include <random>
#include <vector>

#ifdef _WIN32
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#else
#  include <sys/resource.h>
#endif

namespace bench {

constexpr int    kSignals = 8;
constexpr int    kWindow  = 20000;
constexpr double kFs      = 10000.0;
constexpr int    kFps     = 30;

struct Options {
    int plots = 1;         // plot widgets; signals are spread evenly over them
    double duration = 10;  // s
    bool fast = false;     // library-specific speed settings
    bool aa = false;       // antialiased curves
    QString csv;           // append one result line
};

inline Options parseArgs(const QStringList& a)
{
    Options o;
    for (int i = 1; i < a.size(); ++i) {
        if (a[i] == "--plots" && i + 1 < a.size()) o.plots = std::clamp(a[++i].toInt(), 1, kSignals);
        else if (a[i] == "--duration" && i + 1 < a.size()) o.duration = a[++i].toDouble();
        else if (a[i] == "--fast") o.fast = true;
        else if (a[i] == "--aa") o.aa = true;
        else if (a[i] == "--csv" && i + 1 < a.size()) o.csv = a[++i];
    }
    return o;
}

// Sliding window, contiguous arrays (both libraries can plot raw pointers without copying).
class Source {
public:
    Source() : x_(kWindow), y_(kSignals, std::vector<double>(kWindow))
    {
        for (int i = 0; i < kWindow; ++i) push(i);
    }

    // Shift left by n and append n new samples.
    void advance(int n)
    {
        n = std::min(n, kWindow);
        std::move(x_.begin() + n, x_.end(), x_.begin());
        for (auto& y : y_) std::move(y.begin() + n, y.end(), y.begin());
        for (int i = kWindow - n; i < kWindow; ++i) push(i);
    }

    double* x() { return x_.data(); }
    double* y(int s) { return y_[s].data(); }
    double tMin() const { return x_.front(); }
    double tMax() const { return x_.back(); }

private:
    void push(int i)
    {
        const double t = double(k_++) / kFs;
        x_[i] = t;
        for (int s = 0; s < kSignals; ++s) {
            const double f = 3.0 + 7.0 * s;  // 3 ... 52 Hz
            y_[s][i] = std::sin(2.0 * 3.14159265358979 * f * t + s) + 0.05 * noise_(rng_) + 0.2 * (s - 3.5);
        }
    }

    long long k_ = 0;
    std::vector<double> x_;
    std::vector<std::vector<double>> y_;
    std::mt19937 rng_{42};
    std::normal_distribution<double> noise_{0.0, 1.0};
};

inline double processCpuSeconds()
{
#ifdef _WIN32
    FILETIME c, e, k, u;
    GetProcessTimes(GetCurrentProcess(), &c, &e, &k, &u);
    auto toS = [](FILETIME f) { return (double(f.dwHighDateTime) * 4294967296.0 + f.dwLowDateTime) * 1e-7; };
    return toS(k) + toS(u);
#else
    rusage r{};
    getrusage(RUSAGE_SELF, &r);
    return r.ru_utime.tv_sec + r.ru_stime.tv_sec + 1e-6 * (r.ru_utime.tv_usec + r.ru_stime.tv_usec);
#endif
}

// Drives the timer, measures, prints and exits. `frame` must update and synchronously
// repaint all plots for the current Source state.
inline int run(QApplication& app, QWidget& window, const char* lib, const Options& o, Source& src,
               const std::function<void()>& frame)
{
    window.resize(1600, 900);
    window.show();

    std::vector<double> ms;
    QElapsedTimer wall, ft;
    double cpu0 = 0, lastT = 0, sampleDebt = 0;
    int warmup = 15;

    QTimer timer;
    timer.setTimerType(Qt::PreciseTimer);
    timer.setInterval(1000 / kFps);
    QObject::connect(&timer, &QTimer::timeout, [&] {
        // Push as many samples as real time demands, so a slow plot also gets more data.
        const double now = wall.isValid() ? wall.nsecsElapsed() * 1e-9 : 0.0;
        sampleDebt += (wall.isValid() ? now - lastT : 1.0 / kFps) * kFs;
        lastT = now;
        const int n = int(sampleDebt);
        sampleDebt -= n;

        ft.start();
        src.advance(n);
        frame();
        const double dt = ft.nsecsElapsed() * 1e-6;

        if (warmup > 0) {
            if (--warmup == 0) { wall.start(); lastT = 0; cpu0 = processCpuSeconds(); }
            return;
        }
        ms.push_back(dt);
        if (wall.elapsed() >= o.duration * 1000) app.quit();
    });
    timer.start();
    app.exec();

    const double secs = wall.nsecsElapsed() * 1e-9;
    const double cpu = 100.0 * (processCpuSeconds() - cpu0) / secs;
    std::vector<double> s = ms;
    std::sort(s.begin(), s.end());
    double mean = 0;
    for (double v : s) mean += v;
    mean /= double(s.size());
    const double p95 = s[size_t(0.95 * double(s.size() - 1))];
    const double fps = double(ms.size()) / secs;

    std::printf("%s plots=%d fast=%d aa=%d  fps=%.1f  frame_mean=%.2f ms  frame_p95=%.2f ms  frame_max=%.2f ms  cpu=%.0f %%\n",
                lib, o.plots, o.fast ? 1 : 0, o.aa ? 1 : 0, fps, mean, p95, s.back(), cpu);
    if (!o.csv.isEmpty()) {
        if (FILE* f = std::fopen(o.csv.toLocal8Bit().constData(), "a")) {
            std::fprintf(f, "%s,%d,%d,%d,%.1f,%.2f,%.2f,%.2f,%.0f\n", lib, o.plots, o.fast ? 1 : 0,
                         o.aa ? 1 : 0, fps, mean, p95, s.back(), cpu);
            std::fclose(f);
        }
    }
    return 0;
}

}  // namespace bench
