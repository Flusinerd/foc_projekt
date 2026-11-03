#include "AboutDialog.hpp"

#include "DriveController.hpp"

#include <QApplication>
#include <QDialogButtonBox>
#include <QFile>
#include <QFormLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPixmap>
#include <QPushButton>
#include <QTabWidget>
#include <QTextBrowser>
#include <QVBoxLayout>

AboutDialog::AboutDialog(DriveController* ctl, QWidget* parent) : QDialog(parent)
{
    setWindowTitle(tr("About FOC Studio"));
    resize(720, 520);
    auto* root = new QVBoxLayout(this);
    auto* tabs = new QTabWidget;

    auto* info = new QWidget;
    auto* form = new QFormLayout(info);
    auto* logo = new QLabel;
    QPixmap pm(QStringLiteral(":/docs/img/logo/carpediem_logo.png"));
    if (!pm.isNull()) {
        pm.setDevicePixelRatio(pm.width() / 360.0);   // 360 px wide, sharp on high-DPI screens
        logo->setPixmap(pm);
    }
    logo->setAccessibleName(QStringLiteral("CarpeDiem"));
    form->addRow(logo);
    auto* title = new QLabel(QStringLiteral("<h2>FOC Studio</h2>") +
                             tr("Operating and commissioning tool for the FOC framework. Project CarpeDiem, "
                                "Laboratory for Actuators, Hochschule Bochum."));
    title->setWordWrap(true);
    form->addRow(title);
    form->addRow(tr("Program version:"), new QLabel(QApplication::applicationVersion()));
    form->addRow(tr("Protocol version (FOCP):"),
                 new QLabel(tr("%1 (%2 parameters, %3 signals)").arg(FOCP_PROTOCOL_VERSION).arg(FOCP_NUM_PARAMS).arg(FOCP_NUM_SIGNALS)));
    const auto& inf = ctl->info();
    form->addRow(tr("Drive firmware:"),
                 new QLabel(ctl->isConnected() && inf.valid
                                ? tr("%1.%2, board 0x%3, protocol %4")
                                      .arg(inf.fwMajor).arg(inf.fwMinor)
                                      .arg(inf.board, 2, 16, QLatin1Char('0')).arg(inf.protocol)
                                : tr("not connected")));
    form->addRow(tr("Qt version:"), new QLabel(QString::fromLatin1(qVersion())));
    tabs->addTab(info, tr("&Program"));

    auto* lic = new QTextBrowser;
    lic->setOpenExternalLinks(true);
    QFile f(QStringLiteral(":/docs/LICENSES.md"));
    if (f.open(QIODevice::ReadOnly)) lic->setMarkdown(QString::fromUtf8(f.readAll()));
    tabs->addTab(lic, tr("&Licenses"));
    root->addWidget(tabs, 1);

    auto* bb = new QDialogButtonBox(QDialogButtonBox::Close);
    auto* aboutQt = bb->addButton(tr("About &Qt"), QDialogButtonBox::ActionRole);
    connect(aboutQt, &QPushButton::clicked, this, [this] { QMessageBox::aboutQt(this); });
    connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(bb);
}
