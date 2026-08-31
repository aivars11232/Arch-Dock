#include "panel/IconOverrideTransaction.h"

#include <QFile>
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>

using ArchDock::IconOverrideTransaction;
using ArchDock::IconOverrideTransactionOutcome;
using ArchDock::IconOverrideTransactionRequest;
using ArchDock::IconOverrideTransactionStatus;
using ArchDock::PanelDefinition;
using ArchDock::PanelIconStyleDefinition;

namespace
{

QVariantMap style(const QString &id,
                  bool valid = true,
                  bool fellBack = false)
{
    return {
        {QStringLiteral("format"), QStringLiteral("org.archdock.icon-style")},
        {QStringLiteral("version"), 1},
        {QStringLiteral("id"), id},
        {QStringLiteral("resolvedStyleId"), id},
        {QStringLiteral("valid"), valid},
        {QStringLiteral("loadable"), valid},
        {QStringLiteral("fellBack"), fellBack},
    };
}

}

class IconOverrideTransactionTest final : public QObject
{
    Q_OBJECT

private slots:
    void completeOverrideIsTypedRevisionedAndRoundTrips();
    void changingOneEntryLeavesAnotherUntouched();
    void resetRemovesOnlyTheSelectedOverride();
    void staleAndMalformedRequestsFailWithoutMutation();
    void unavailableStyleIsRejectedBeforePersistence();
    void missingCustomAssetFallsBackToTheBaseGlyph();
    void resolutionUsesOverrideThenPanelThenSafeStyleFallback();
    void unavailableStoredOverrideFallsBackThroughPanelStyle();
};

void IconOverrideTransactionTest::completeOverrideIsTypedRevisionedAndRoundTrips()
{
    PanelDefinition current = PanelDefinition::defaults(
        QStringLiteral("bottom"), QStringLiteral("Bottom"),
        QStringLiteral("bottom"), true);
    current.settingsRevision = 4;
    const QString identity = QStringLiteral(
        "desktop.org.example.editor.desktop");
    const QVariantMap values{
        {QStringLiteral("customGlyph"), QStringLiteral("utilities-terminal")},
        {QStringLiteral("customLabel"), QStringLiteral("Editor override")},
        {QStringLiteral("tileEnabled"), false},
        {QStringLiteral("styleReference"), QStringLiteral("metallic-red")},
        {QStringLiteral("animationProfileReference"),
         QStringLiteral("future-bounce")},
    };
    IconOverrideTransactionOutcome outcome;
    const auto draft = IconOverrideTransaction::prepare(
        current,
        {QStringLiteral("bottom"), 4, identity, values, false},
        &outcome,
        [](const QString &styleId)
        {
            return styleId == QStringLiteral("metallic-red");
        });

    QVERIFY(draft.has_value());
    QCOMPARE(outcome.status, IconOverrideTransactionStatus::Prepared);
    QCOMPARE(draft->candidatePanel.settingsRevision, quint64{5});
    QCOMPARE(draft->previousPanel.iconStyle.perEntryOverrides.size(), 0);
    QCOMPARE(draft->candidatePanel.iconStyle.perEntryOverrides.size(), 1);
    const auto override = draft->candidatePanel.iconStyle.perEntryOverrides
        .value(identity);
    QCOMPARE(override.customGlyph, QStringLiteral("utilities-terminal"));
    QCOMPARE(override.customLabel, QStringLiteral("Editor override"));
    QVERIFY(override.tileEnabled.has_value());
    QVERIFY(!*override.tileEnabled);
    QCOMPARE(override.styleReference, QStringLiteral("metallic-red"));
    QCOMPARE(override.animationProfileReference,
             QStringLiteral("future-bounce"));

    const auto reparsed = PanelDefinition::fromLegacyMap(
        draft->candidatePanel.toPersistedMap());
    QVERIFY(reparsed.has_value());
    QCOMPARE(*reparsed, draft->candidatePanel);
}

void IconOverrideTransactionTest::changingOneEntryLeavesAnotherUntouched()
{
    PanelDefinition current = PanelDefinition::defaults(
        QStringLiteral("bottom"), QStringLiteral("Bottom"),
        QStringLiteral("bottom"), true);
    const QString first = QStringLiteral("application.first");
    const QString second = QStringLiteral("application.second");
    PanelIconStyleDefinition::EntryOverride existing;
    existing.customLabel = QStringLiteral("Second unchanged");
    current.iconStyle.perEntryOverrides.insert(second, existing);

    IconOverrideTransactionOutcome outcome;
    const auto draft = IconOverrideTransaction::prepare(
        current,
        {QStringLiteral("bottom"), 0, first,
         {{QStringLiteral("customLabel"), QStringLiteral("First changed")}},
         false},
        &outcome);

    QVERIFY(draft.has_value());
    QCOMPARE(draft->candidatePanel.iconStyle.perEntryOverrides.size(), 2);
    QCOMPARE(draft->candidatePanel.iconStyle.perEntryOverrides.value(second),
             existing);
    QVERIFY(!current.iconStyle.perEntryOverrides.contains(first));
}

void IconOverrideTransactionTest::resetRemovesOnlyTheSelectedOverride()
{
    PanelDefinition current = PanelDefinition::defaults(
        QStringLiteral("bottom"), QStringLiteral("Bottom"),
        QStringLiteral("bottom"), true);
    const QString first = QStringLiteral("application.first");
    const QString second = QStringLiteral("application.second");
    PanelIconStyleDefinition::EntryOverride firstOverride;
    firstOverride.styleReference = QStringLiteral("metallic-blue");
    PanelIconStyleDefinition::EntryOverride secondOverride;
    secondOverride.customLabel = QStringLiteral("Keep me");
    current.iconStyle.perEntryOverrides.insert(first, firstOverride);
    current.iconStyle.perEntryOverrides.insert(second, secondOverride);

    IconOverrideTransactionOutcome outcome;
    const auto draft = IconOverrideTransaction::prepare(
        current,
        {QStringLiteral("bottom"), 0, first, {}, true},
        &outcome);

    QVERIFY(draft.has_value());
    QVERIFY(!draft->candidatePanel.iconStyle.perEntryOverrides.contains(first));
    QCOMPARE(draft->candidatePanel.iconStyle.perEntryOverrides.size(), 1);
    QCOMPARE(draft->candidatePanel.iconStyle.perEntryOverrides.value(second),
             secondOverride);
}

void IconOverrideTransactionTest::staleAndMalformedRequestsFailWithoutMutation()
{
    PanelDefinition current = PanelDefinition::defaults(
        QStringLiteral("bottom"), QStringLiteral("Bottom"),
        QStringLiteral("bottom"), true);
    current.settingsRevision = 7;
    const QVariantMap before = current.toPersistedMap();
    IconOverrideTransactionOutcome outcome;

    QVERIFY(!IconOverrideTransaction::prepare(
        current,
        {QStringLiteral("bottom"), 6, QStringLiteral("application.example"),
         {{QStringLiteral("customLabel"), QStringLiteral("stale")}}, false},
        &outcome).has_value());
    QCOMPARE(outcome.status, IconOverrideTransactionStatus::RevisionConflict);
    QCOMPARE(outcome.errorCode, QStringLiteral("stale-revision"));

    QVERIFY(!IconOverrideTransaction::prepare(
        current,
        {QStringLiteral("bottom"), 7, QStringLiteral("unsafe key"),
         {{QStringLiteral("customLabel"), QStringLiteral("invalid")}}, false},
        &outcome).has_value());
    QCOMPARE(outcome.errorCode, QStringLiteral("invalid-entry-identity"));
    QCOMPARE(current.toPersistedMap(), before);
}

void IconOverrideTransactionTest::unavailableStyleIsRejectedBeforePersistence()
{
    const PanelDefinition current = PanelDefinition::defaults(
        QStringLiteral("bottom"), QStringLiteral("Bottom"),
        QStringLiteral("bottom"), true);
    IconOverrideTransactionOutcome outcome;
    int validatorCalls = 0;
    const auto draft = IconOverrideTransaction::prepare(
        current,
        {QStringLiteral("bottom"), 0, QStringLiteral("application.example"),
         {{QStringLiteral("styleReference"), QStringLiteral("missing-style")}},
         false},
        &outcome,
        [&validatorCalls](const QString &)
        {
            ++validatorCalls;
            return false;
        });

    QVERIFY(!draft.has_value());
    QCOMPARE(validatorCalls, 1);
    QCOMPARE(outcome.errorCode, QStringLiteral("unknown-icon-style"));
    QCOMPARE(current.settingsRevision, quint64{0});
}

void IconOverrideTransactionTest::missingCustomAssetFallsBackToTheBaseGlyph()
{
    PanelDefinition panel = PanelDefinition::defaults(
        QStringLiteral("bottom"), QStringLiteral("Bottom"),
        QStringLiteral("bottom"), true);
    const QString identity = QStringLiteral("application.example");
    PanelIconStyleDefinition::EntryOverride override;
    override.customGlyph = QStringLiteral("file:///definitely/missing/icon.svg");
    panel.iconStyle.perEntryOverrides.insert(identity, override);

    const QVariantMap resolved = IconOverrideTransaction::resolve(
        panel, identity, QStringLiteral("applications-system"),
        QStringLiteral("Example"),
        [](const QString &styleId)
        {
            return style(styleId);
        });

    QCOMPARE(resolved.value(QStringLiteral("resolvedGlyph")).toString(),
             QStringLiteral("applications-system"));
    QVERIFY(resolved.value(QStringLiteral("glyphFallbackApplied")).toBool());
    QCOMPARE(resolved.value(QStringLiteral("glyphFallbackReason")).toString(),
             QStringLiteral("custom-glyph-unavailable"));
}

void IconOverrideTransactionTest::resolutionUsesOverrideThenPanelThenSafeStyleFallback()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QFile glyph(directory.filePath(QStringLiteral("glyph.svg")));
    QVERIFY(glyph.open(QIODevice::WriteOnly));
    glyph.write("<svg xmlns=\"http://www.w3.org/2000/svg\"/>\n");
    glyph.close();

    PanelDefinition panel = PanelDefinition::defaults(
        QStringLiteral("bottom"), QStringLiteral("Bottom"),
        QStringLiteral("bottom"), true);
    panel.iconStyle.styleReference = QStringLiteral("metallic-blue");
    const QString identity = QStringLiteral("application.example");
    PanelIconStyleDefinition::EntryOverride override;
    override.customGlyph = QUrl::fromLocalFile(glyph.fileName()).toString();
    override.customLabel = QStringLiteral("Overridden label");
    override.tileEnabled = false;
    override.styleReference = QStringLiteral("dark-orb");
    override.animationProfileReference = QStringLiteral("future-orbit");
    panel.iconStyle.perEntryOverrides.insert(identity, override);

    const QVariantMap resolved = IconOverrideTransaction::resolve(
        panel, identity, QStringLiteral("base-glyph"),
        QStringLiteral("Base label"),
        [](const QString &styleId)
        {
            return styleId == QStringLiteral("dark-orb")
                ? style(styleId) : style(QStringLiteral("plain-original"), true, true);
        });
    QCOMPARE(resolved.value(QStringLiteral("resolvedGlyph")).toString(),
             QUrl::fromLocalFile(glyph.fileName()).toString());
    QCOMPARE(resolved.value(QStringLiteral("resolvedLabel")).toString(),
             QStringLiteral("Overridden label"));
    QVERIFY(!resolved.value(QStringLiteral("tileEnabled")).toBool());
    QCOMPARE(resolved.value(QStringLiteral("styleReference")).toString(),
             QStringLiteral("dark-orb"));
    QCOMPARE(resolved.value(
                 QStringLiteral("animationProfileReference")).toString(),
             QStringLiteral("future-orbit"));
    QVERIFY(resolved.value(QStringLiteral("overrideApplied")).toBool());

    const QVariantMap panelDefault = IconOverrideTransaction::resolve(
        panel, QStringLiteral("application.other"),
        QStringLiteral("other-glyph"), QStringLiteral("Other"),
        [](const QString &styleId)
        {
            return style(styleId);
        });
    QCOMPARE(panelDefault.value(QStringLiteral("styleReference")).toString(),
             QStringLiteral("metallic-blue"));
    QCOMPARE(panelDefault.value(QStringLiteral("resolvedGlyph")).toString(),
             QStringLiteral("other-glyph"));
    QVERIFY(!panelDefault.value(QStringLiteral("overrideApplied")).toBool());
}

void IconOverrideTransactionTest::unavailableStoredOverrideFallsBackThroughPanelStyle()
{
    PanelDefinition panel = PanelDefinition::defaults(
        QStringLiteral("bottom"), QStringLiteral("Bottom"),
        QStringLiteral("bottom"), true);
    panel.iconStyle.styleReference = QStringLiteral("metallic-blue");
    const QString identity = QStringLiteral("application.example");
    PanelIconStyleDefinition::EntryOverride override;
    override.styleReference = QStringLiteral("removed-style");
    panel.iconStyle.perEntryOverrides.insert(identity, override);

    const QVariantMap resolved = IconOverrideTransaction::resolve(
        panel, identity, QStringLiteral("base-glyph"),
        QStringLiteral("Base label"),
        [](const QString &styleId)
        {
            if (styleId == QStringLiteral("removed-style"))
            {
                QVariantMap fallback = style(
                    QStringLiteral("plain-original"), true, true);
                fallback.insert(QStringLiteral("fallbackReason"),
                                QStringLiteral("unknown-style"));
                return fallback;
            }
            return style(styleId);
        });

    QCOMPARE(resolved.value(QStringLiteral("requestedStyleReference")).toString(),
             QStringLiteral("removed-style"));
    QCOMPARE(resolved.value(QStringLiteral("panelStyleReference")).toString(),
             QStringLiteral("metallic-blue"));
    QCOMPARE(resolved.value(QStringLiteral("styleReference")).toString(),
             QStringLiteral("metallic-blue"));
    QVERIFY(resolved.value(QStringLiteral("styleFallbackApplied")).toBool());
    QCOMPARE(resolved.value(QStringLiteral("styleFallbackReason")).toString(),
             QStringLiteral("unknown-style"));
}

QTEST_GUILESS_MAIN(IconOverrideTransactionTest)

#include "IconOverrideTransactionTest.moc"
