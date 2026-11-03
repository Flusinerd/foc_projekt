// FOC Studio - end-user application for the FOC framework (Qt 6 Widgets).
//
//   focstudio                               normal start
//   focstudio --demo                        connect to the virtual drive and run a demo profile
//   focstudio --screenshot f.png [--delay ms] [--size WxH]   save the main window after delay, exit
#include "DriveController.hpp"
#include "MainWindow.hpp"

#include <QApplication>
#include <QCommandLineParser>
#include <QLocale>
#include <QSettings>
#include <QTimer>
#include <QTranslator>

#include <cstdio>

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    QApplication::setOrganizationName(QStringLiteral("Hochschule Bochum"));
    QApplication::setOrganizationDomain(QStringLiteral("hs-bochum.de"));
    QApplication::setApplicationName(QStringLiteral("FOC Studio"));
    QApplication::setApplicationVersion(QStringLiteral(FOC_STUDIO_VERSION));

    // UI language: setting "ui/language" ("de", "en"), empty = system language.
    // The .qm files contain Qt's own strings too (MERGE_QT_TRANSLATIONS).
    QTranslator translator;
    const QString lang = QSettings().value(QStringLiteral("ui/language")).toString();
    const QLocale locale = lang.isEmpty() ? QLocale::system() : QLocale(lang);
    if (translator.load(locale, QStringLiteral("focstudio"), QStringLiteral("_"), QStringLiteral(":/i18n")))
        QApplication::installTranslator(&translator);
    QLocale::setDefault(locale);

    QCommandLineParser cli;
    cli.setApplicationDescription(QStringLiteral("FOC Studio"));
    cli.addHelpOption();
    cli.addVersionOption();
    QCommandLineOption demo(QStringLiteral("demo"), QStringLiteral("Connect to the virtual drive and run a demo."));
    QCommandLineOption shot(QStringLiteral("screenshot"), QStringLiteral("Save the main window to <file> and exit."),
                            QStringLiteral("file"));
    QCommandLineOption delay(QStringLiteral("delay"), QStringLiteral("Screenshot delay in ms (default 3000)."),
                             QStringLiteral("ms"), QStringLiteral("3000"));
    QCommandLineOption size(QStringLiteral("size"), QStringLiteral("Window size WxH."), QStringLiteral("WxH"));
    QCommandLineOption tab(QStringLiteral("tab"), QStringLiteral("Open page <n> (0 = overview)."), QStringLiteral("n"));
    cli.addOptions({demo, shot, delay, size, tab});
    cli.process(app);

    DriveController ctl;   // expert mode only via View menu with confirmation
    MainWindow win(&ctl);
    if (cli.isSet(tab)) QTimer::singleShot(0, &win, [&win, &cli, tab] { win.showTab(cli.value(tab).toInt()); });
    if (cli.isSet(size)) {
        const QStringList wh = cli.value(size).split(QLatin1Char('x'));
        if (wh.size() == 2) win.resize(wh[0].toInt(), wh[1].toInt());
    }
    win.show();
    if (cli.isSet(demo)) QTimer::singleShot(0, &win, &MainWindow::startDemo);

    if (cli.isSet(shot)) {
        const QString file = cli.value(shot);
        QTimer::singleShot(cli.value(delay).toInt(), &win, [&win, file] {
            const bool ok = win.grab().save(file);
            std::printf("%s %s\n", ok ? "saved" : "FAILED", qPrintable(file));
            QApplication::exit(ok ? 0 : 1);
        });
    }
    return app.exec();
}
