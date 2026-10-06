// Sampling a look's platform colours from its artwork.
#include "LookPalette.h"

#include <QColor>
#include <QImage>
#include <QImageReader>
#include <QPainter>

#include <algorithm>
#include <vector>

namespace ArchDock::LookPalette
{
namespace
{
// Artwork is read at this width at most: enough to tell a rim from a body,
// small enough to read on every projection without a cost worth caching.
constexpr int SampleWidth = 320;

struct Sample
{
    double luminance;
    double r;
    double g;
    double b;
};

QVariantList colour(double r, double g, double b)
{
    return {std::clamp(r, 0.0, 1.0), std::clamp(g, 0.0, 1.0), std::clamp(b, 0.0, 1.0), 1.0};
}

// The mean colour of the samples between two luminance quantiles.
QVariantList band(const std::vector<Sample> &sorted, double from, double to, double factor = 1.0)
{
    const auto count = static_cast<double>(sorted.size());
    auto first = static_cast<std::size_t>(std::clamp(from, 0.0, 1.0) * count);
    auto last = static_cast<std::size_t>(std::clamp(to, 0.0, 1.0) * count);
    last = std::min(std::max(last, first + 1), sorted.size());
    first = std::min(first, last - 1);
    double r = 0;
    double g = 0;
    double b = 0;
    for (std::size_t index = first; index < last; ++index)
    {
        r += sorted[index].r;
        g += sorted[index].g;
        b += sorted[index].b;
    }
    const double n = static_cast<double>(last - first);
    return colour(r / n * factor, g / n * factor, b / n * factor);
}
}

std::optional<QVariantMap> fromArtwork(const QVariantMap &projection)
{
    const QVariantMap paths = projection.value(QStringLiteral("assetPaths")).toMap();
    QImage canvas;
    for (const QVariant &value : projection.value(QStringLiteral("layers")).toList())
    {
        const QVariantMap layer = value.toMap();
        const QString role = layer.value(QStringLiteral("role")).toString();
        if (role != QStringLiteral("rear") && role != QStringLiteral("foreground"))
            continue;
        const QString path = paths.value(layer.value(QStringLiteral("asset")).toString()).toString();
        if (path.isEmpty())
            continue;
        QImageReader reader(path);
        const QSize natural = reader.size();
        if (!reader.canRead() || !natural.isValid() || natural.width() <= 0)
            continue;
        const QSize size = natural.width() > SampleWidth
            ? QSize(SampleWidth, std::max(1, natural.height() * SampleWidth / natural.width()))
            : natural;
        reader.setScaledSize(size);
        const QImage image = reader.read().convertToFormat(QImage::Format_ARGB32);
        if (image.isNull())
            continue;
        if (canvas.isNull())
        {
            canvas = QImage(image.size(), QImage::Format_ARGB32);
            canvas.fill(Qt::transparent);
        }
        QPainter painter(&canvas);
        painter.setOpacity(std::clamp(layer.value(QStringLiteral("opacity"), 1.0).toDouble(), 0.0, 1.0));
        painter.drawImage(QRect(QPoint(0, 0), canvas.size()), image);
    }
    if (canvas.isNull())
        return std::nullopt;

    // Only the platform itself: fully covered pixels, not its soft edges.
    std::vector<Sample> samples;
    samples.reserve(static_cast<std::size_t>(canvas.width()) * canvas.height() / 4);
    for (int y = 0; y < canvas.height(); ++y)
    {
        const auto *line = reinterpret_cast<const QRgb *>(canvas.constScanLine(y));
        for (int x = 0; x < canvas.width(); ++x)
        {
            if (qAlpha(line[x]) < 200)
                continue;
            const double r = qRed(line[x]) / 255.0;
            const double g = qGreen(line[x]) / 255.0;
            const double b = qBlue(line[x]) / 255.0;
            samples.push_back({0.2126 * r + 0.7152 * g + 0.0722 * b, r, g, b});
        }
    }
    if (samples.size() < 64)
        return std::nullopt;
    std::sort(samples.begin(), samples.end(),
              [](const Sample &a, const Sample &b) { return a.luminance < b.luminance; });
    return QVariantMap{
        {QStringLiteral("source"), QStringLiteral("artwork")},
        // Most of the platform is its body; its strokes are its brightest.
        {QStringLiteral("top"), band(samples, 0.40, 0.75)},
        {QStringLiteral("wall"), band(samples, 0.05, 0.25)},
        {QStringLiteral("under"), band(samples, 0.05, 0.25, 0.6)},
        {QStringLiteral("rim"), band(samples, 0.98, 1.0)},
    };
}
}
