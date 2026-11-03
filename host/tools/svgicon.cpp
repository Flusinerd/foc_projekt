// svgicon - renders an SVG to PNG files and a multi-size Windows .ico (PNG-compressed
// entries, supported since Windows Vista). Used to regenerate host/studio/icons.
//
//   svgicon <in.svg> <out-prefix> [--ico <file.ico>] [--width <w>] size...
//     square:  svgicon mark.svg icons/carpediem --ico icons/carpediem.ico 16 24 32 48 64 128 256
//              -> icons/carpediem_16.png ... and the .ico
//     banner:  svgicon logo.svg img/carpediem_logo --width 1200     (height from aspect ratio)
#include <QBuffer>
#include <QFile>
#include <QGuiApplication>
#include <QImage>
#include <QPainter>
#include <QStringList>
#include <QSvgRenderer>

#include <cstdio>
#include <vector>

namespace {

QImage render(QSvgRenderer& svg, int w, int h)
{
    QImage img(w, h, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);
    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing);
    svg.render(&p);
    return img;
}

void put16(QByteArray& b, int v) { b.append(char(v & 0xFF)).append(char((v >> 8) & 0xFF)); }
void put32(QByteArray& b, quint32 v) { put16(b, int(v & 0xFFFF)); put16(b, int(v >> 16)); }

bool writeIco(const QString& path, const std::vector<std::pair<int, QByteArray>>& pngs)
{
    QByteArray out;
    put16(out, 0);                       // reserved
    put16(out, 1);                       // type: icon
    put16(out, int(pngs.size()));
    quint32 offset = 6 + 16 * quint32(pngs.size());
    for (const auto& [size, data] : pngs) {
        out.append(char(size >= 256 ? 0 : size));   // 0 means 256
        out.append(char(size >= 256 ? 0 : size));
        out.append(char(0));             // palette
        out.append(char(0));             // reserved
        put16(out, 1);                   // planes
        put16(out, 32);                  // bits per pixel
        put32(out, quint32(data.size()));
        put32(out, offset);
        offset += quint32(data.size());
    }
    for (const auto& e : pngs) out.append(e.second);
    QFile f(path);
    return f.open(QIODevice::WriteOnly) && f.write(out) == out.size();
}

} // namespace

int main(int argc, char** argv)
{
    QGuiApplication app(argc, argv);
    QStringList a = app.arguments();
    if (a.size() < 4) {
        std::fprintf(stderr, "usage: svgicon <in.svg> <out-prefix> [--ico file] [--width w] size...\n");
        return 2;
    }
    QSvgRenderer svg(a[1]);
    if (!svg.isValid()) { std::fprintf(stderr, "cannot read %s\n", qPrintable(a[1])); return 1; }
    const QString prefix = a[2];
    QString ico;
    std::vector<int> sizes;
    int width = 0;
    for (int i = 3; i < a.size(); ++i) {
        if (a[i] == QLatin1String("--ico") && i + 1 < a.size()) ico = a[++i];
        else if (a[i] == QLatin1String("--width") && i + 1 < a.size()) width = a[++i].toInt();
        else sizes.push_back(a[i].toInt());
    }

    if (width > 0) {
        const QSizeF vb = svg.viewBoxF().size();
        const int h = int(width * vb.height() / vb.width() + 0.5);
        const QString file = prefix + QStringLiteral(".png");
        if (!render(svg, width, h).save(file)) return 1;
        std::printf("%s (%dx%d)\n", qPrintable(file), width, h);
    }

    std::vector<std::pair<int, QByteArray>> pngs;
    for (int s : sizes) {
        const QImage img = render(svg, s, s);
        const QString file = prefix + QStringLiteral("_%1.png").arg(s);
        if (!img.save(file)) return 1;
        QByteArray data;
        QBuffer buf(&data);
        buf.open(QIODevice::WriteOnly);
        img.save(&buf, "PNG");
        pngs.emplace_back(s, data);
        std::printf("%s\n", qPrintable(file));
    }
    if (!ico.isEmpty()) {
        if (!writeIco(ico, pngs)) return 1;
        std::printf("%s (%zu sizes)\n", qPrintable(ico), pngs.size());
    }
    return 0;
}
