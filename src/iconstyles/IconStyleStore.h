#pragma once

#include "IconStylePackage.h"

#include <QByteArray>
#include <QHash>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
#include <QVector>

#include <optional>

namespace ArchDock
{

struct IconStyleStoreLoadResult;

class IconStyleStore
{
public:
    static constexpr qsizetype MaximumCatalogBytes = 262144;
    static constexpr qsizetype MaximumPackages = 64;

    [[nodiscard]] static IconStyleStoreLoadResult loadCatalog(
        const QString &catalogPath,
        const QString &packageRoot = {});
    [[nodiscard]] static IconStyleStoreLoadResult loadCatalogBytes(
        const QByteArray &catalogBytes,
        const QString &packageRoot,
        const QString &catalogPath = {});

    [[nodiscard]] bool contains(const QString &styleId) const;
    [[nodiscard]] QStringList styleIds() const;
    [[nodiscard]] QString fallbackStyleId() const;
    [[nodiscard]] const IconStylePackage *packageById(
        const QString &styleId) const;
    [[nodiscard]] QVariantList catalogEntries() const;
    [[nodiscard]] QVariantMap resolve(const QString &requestedStyleId) const;
    [[nodiscard]] QVariantMap toVariantMap() const;

private:
    QString m_catalogPath;
    QString m_packageRoot;
    QString m_fallbackStyleId = QStringLiteral("plain-original");
    QVariantList m_catalogEntries;
    QHash<QString, IconStylePackage> m_packages;

    friend struct IconStyleStoreLoadResult;
};

struct IconStyleStoreLoadResult
{
    std::optional<IconStyleStore> store;
    QVector<IconStyleValidationDiagnostic> diagnostics;

    [[nodiscard]] bool isValid() const;
    [[nodiscard]] QString primaryCode() const;
    [[nodiscard]] QString primaryMessage() const;
    [[nodiscard]] QVariantMap toVariantMap() const;
};

}
