// SPDX-License-Identifier: GPL-3.0-or-later
#include "BarRecord.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QStringList>

#include <algorithm>

namespace ArchDock::Bars
{
namespace
{
QString choice(const QJsonObject &object, const char *key, const QStringList &choices, const QString &fallback)
{
    const QString value = object.value(QLatin1String(key)).toString().trimmed().toLower();
    return choices.contains(value) ? value : fallback;
}

int number(const QJsonObject &object, const char *key, int fallback, int low, int high)
{
    const QJsonValue value = object.value(QLatin1String(key));
    return std::clamp(value.isDouble() ? value.toInt(fallback) : fallback, low, high);
}
}

QList<BarRecord> parseBarRecords(const QByteArray &json, QString *error)
{
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(json, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isArray())
    {
        if (error)
            *error = parseError.error != QJsonParseError::NoError
                ? parseError.errorString() : QStringLiteral("the bar list is not a list");
        return {};
    }

    static const QStringList edges{QStringLiteral("top"), QStringLiteral("bottom"),
                                   QStringLiteral("left"), QStringLiteral("right")};
    QList<BarRecord> records;
    QSet<QString> seen;
    const QJsonArray list = document.array();
    for (int index = 0; index < list.size(); ++index)
    {
        const QJsonObject object = list.at(index).toObject();
        BarRecord record;
        record.id = object.value(QLatin1String("id")).toString().trimmed();
        record.edge = choice(object, "edge", edges, QString());
        if (record.id.isEmpty() || record.edge.isEmpty() || seen.contains(record.id))
            continue;
        seen.insert(record.id);

        record.type = choice(object, "type",
                             {QStringLiteral("empty"), QStringLiteral("launcher"), QStringLiteral("tasks"),
                              QStringLiteral("hybrid")}, record.type);
        record.screenId = object.value(QLatin1String("screenId")).toString().trimmed();
        record.screenIndex = number(object, "screen", 0, 0, 64);
        record.alignment = choice(object, "alignment",
                                  {QStringLiteral("start"), QStringLiteral("center"), QStringLiteral("end")},
                                  record.alignment);
        record.order = number(object, "order", index, -100000, 100000);
        record.offset = number(object, "offset", 0, -16384, 16384);
        record.thickness = number(object, "thickness", record.thickness, 8, 4096);
        record.lengthMode = choice(object, "lengthMode",
                                   {QStringLiteral("fit"), QStringLiteral("fixed"), QStringLiteral("fill")},
                                   record.lengthMode);
        record.fixedLength = number(object, "fixedLength", record.fixedLength, 1, 16384);
        record.minimumLength = number(object, "minimumLength", record.minimumLength, 1, 16384);
        record.maximumLength = std::max(record.minimumLength,
                                        number(object, "maximumLength", record.maximumLength, 1, 16384));
        record.floatingMargin = number(object, "floatingMargin", 0, 0, 1024);
        record.visibilityMode = choice(object, "visibilityMode",
                                       {QStringLiteral("always"), QStringLiteral("auto-hide"),
                                        QStringLiteral("dodge"), QStringLiteral("cover")},
                                       record.visibilityMode);
        const QJsonValue reserves = object.value(QLatin1String("reservesSpace"));
        record.reservesSpace = reserves.isBool() ? reserves.toBool()
                                                 : record.visibilityMode == QStringLiteral("always");
        const QJsonValue visible = object.value(QLatin1String("visible"));
        record.visible = visible.isBool() ? visible.toBool() : true;
        records.append(record);
    }
    return records;
}
}
