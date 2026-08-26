#include "panel/PanelSettingsTransaction.h"

#include <QTest>

using ArchDock::PanelDefinition;
using ArchDock::PanelSettingsHostResult;
using ArchDock::PanelSettingsTransaction;
using ArchDock::PanelSettingsTransactionOutcome;
using ArchDock::PanelSettingsTransactionRequest;
using ArchDock::PanelSettingsTransactionStatus;

class PanelSettingsTransactionTest final : public QObject
{
    Q_OBJECT

private slots:
    void completeCandidateIsNormalizedAndRevisioned();
    void invalidCandidateIsRejectedWithoutChangingSnapshot();
    void unknownAndHiddenCandidatesAreRejectedWithoutChangingSnapshot();
    void staleRevisionIsAConflict();
    void protectedRevisionCannotBeInjected();
    void capabilityValidationRunsAfterCandidateNormalization();
    void capabilityRejectionStopsBeforePersistenceAndHostApplication();
    void requiredHostFailuresAreAttachedAndFailTheOutcome();
};

void PanelSettingsTransactionTest::completeCandidateIsNormalizedAndRevisioned()
{
    PanelDefinition current = PanelDefinition::defaults(
        QStringLiteral("bottom"),
        QStringLiteral("Bottom panel"),
        QStringLiteral("bottom"),
        true);
    current.settingsRevision = 7;
    const QVariantMap globals{{QStringLiteral("magnification"), 1.4}};
    const QVariantMap candidateGlobals{{QStringLiteral("magnification"), 1.8}};
    PanelSettingsTransactionRequest request{
        QStringLiteral("bottom"),
        7,
        {
            {QStringLiteral("iconSize"), 999},
            {QStringLiteral("opacity"), 0.75},
        },
        {{QStringLiteral("magnification"), 1.8}},
    };
    PanelSettingsTransactionOutcome outcome;

    const auto draft = PanelSettingsTransaction::prepare(
        current, globals, candidateGlobals, request, &outcome);

    QVERIFY(draft.has_value());
    QCOMPARE(outcome.status, PanelSettingsTransactionStatus::Prepared);
    QCOMPARE(draft->previousPanel, current);
    QCOMPARE(draft->previousPanel.iconStyle.size, 52);
    QCOMPARE(draft->candidatePanel.iconStyle.size, 128);
    QCOMPARE(draft->candidatePanel.surface.opacity, 0.75);
    QCOMPARE(draft->candidatePanel.settingsRevision, quint64{8});
    QCOMPARE(draft->previousGlobals, globals);
    QCOMPARE(draft->candidateGlobals, candidateGlobals);
}

void PanelSettingsTransactionTest::invalidCandidateIsRejectedWithoutChangingSnapshot()
{
    const PanelDefinition current = PanelDefinition::defaults(
        QStringLiteral("bottom"),
        QStringLiteral("Bottom panel"),
        QStringLiteral("bottom"),
        true);
    PanelSettingsTransactionRequest request{
        QStringLiteral("bottom"),
        0,
        {{QStringLiteral("hostKind"), QStringLiteral("free-desktop")}},
        {},
    };
    PanelSettingsTransactionOutcome outcome;

    const auto draft = PanelSettingsTransaction::prepare(
        current, {}, {}, request, &outcome);

    QVERIFY(!draft.has_value());
    QCOMPARE(outcome.status, PanelSettingsTransactionStatus::ValidationFailed);
    QCOMPARE(outcome.errorCode, QStringLiteral("protected-panel-field"));
    QCOMPARE(current.settingsRevision, quint64{0});
    QCOMPARE(current.placement.edge, QStringLiteral("bottom"));
}

void PanelSettingsTransactionTest::unknownAndHiddenCandidatesAreRejectedWithoutChangingSnapshot()
{
    const PanelDefinition current = PanelDefinition::defaults(
        QStringLiteral("bottom"),
        QStringLiteral("Bottom panel"),
        QStringLiteral("bottom"),
        true);
    const QVariantMap persistedBefore = current.toPersistedMap();

    const auto reject = [&current](const QString &key,
                                   const QVariant &value,
                                   const QString &expectedError)
    {
        PanelSettingsTransactionRequest request{
            QStringLiteral("bottom"),
            current.settingsRevision,
            {{key, value}},
            {},
        };
        PanelSettingsTransactionOutcome outcome;
        QVERIFY(!PanelSettingsTransaction::prepare(
            current, {}, {}, request, &outcome).has_value());
        QCOMPARE(outcome.status, PanelSettingsTransactionStatus::ValidationFailed);
        QCOMPARE(outcome.errorCode, expectedError);
        QCOMPARE(outcome.revision, current.settingsRevision);
    };

    reject(QStringLiteral("notASettingsField"), true,
           QStringLiteral("unknown-panel-field"));
    reject(QStringLiteral("physicsEnabled"), true,
           QStringLiteral("unavailable-panel-field"));
    reject(QStringLiteral("surface3D"),
           QVariantMap{{QStringLiteral("depth"), 12}},
           QStringLiteral("unavailable-panel-field"));
    reject(QStringLiteral("themeSource"), QStringLiteral("file:///forged"),
           QStringLiteral("unavailable-panel-field"));

    QCOMPARE(current.settingsRevision, quint64{0});
    QCOMPARE(current.toPersistedMap(), persistedBefore);
}

void PanelSettingsTransactionTest::staleRevisionIsAConflict()
{
    PanelDefinition current = PanelDefinition::defaults(
        QStringLiteral("bottom"),
        QStringLiteral("Bottom panel"),
        QStringLiteral("bottom"),
        true);
    current.settingsRevision = 12;
    PanelSettingsTransactionRequest request{
        QStringLiteral("bottom"),
        11,
        {{QStringLiteral("opacity"), 0.5}},
        {},
    };
    PanelSettingsTransactionOutcome outcome;

    QVERIFY(!PanelSettingsTransaction::prepare(
        current, {}, {}, request, &outcome).has_value());
    QCOMPARE(outcome.status, PanelSettingsTransactionStatus::RevisionConflict);
    QCOMPARE(outcome.errorCode, QStringLiteral("stale-revision"));
    QCOMPARE(outcome.previousRevision, quint64{12});
}

void PanelSettingsTransactionTest::protectedRevisionCannotBeInjected()
{
    const PanelDefinition current = PanelDefinition::defaults(
        QStringLiteral("bottom"),
        QStringLiteral("Bottom panel"),
        QStringLiteral("bottom"),
        true);
    PanelSettingsTransactionRequest request{
        QStringLiteral("bottom"),
        0,
        {{QStringLiteral("settingsRevision"), 44}},
        {},
    };
    PanelSettingsTransactionOutcome outcome;

    QVERIFY(!PanelSettingsTransaction::prepare(
        current, {}, {}, request, &outcome).has_value());
    QCOMPARE(outcome.status, PanelSettingsTransactionStatus::ValidationFailed);
    QCOMPARE(outcome.errorCode, QStringLiteral("protected-panel-field"));
}

void PanelSettingsTransactionTest::capabilityValidationRunsAfterCandidateNormalization()
{
    PanelDefinition current = PanelDefinition::defaults(
        QStringLiteral("bottom"),
        QStringLiteral("Bottom panel"),
        QStringLiteral("bottom"),
        true);
    current.settingsRevision = 7;
    PanelSettingsTransactionRequest request{
        QStringLiteral("bottom"),
        7,
        {{QStringLiteral("iconSize"), 999}},
        {},
    };
    PanelSettingsTransactionOutcome outcome;
    int validatorCalls = 0;
    PanelDefinition observedCandidate;

    const auto draft = PanelSettingsTransaction::prepare(
        current,
        {},
        {},
        request,
        &outcome,
        [&validatorCalls, &observedCandidate](const PanelDefinition &candidate)
        {
            ++validatorCalls;
            observedCandidate = candidate;
            return ArchDock::PanelCapabilityResolver::resolve(
                candidate,
                ArchDock::PanelCapabilityResolver::productionHostProfile(
                    candidate.host.kind),
                ArchDock::PanelCapabilityResolver::proceduralThemeProfile(),
                ArchDock::PanelCapabilityResolver::productionRenderers(),
                ArchDock::PanelCapabilityResolver::productionPlatform());
        });

    QVERIFY(draft.has_value());
    QCOMPARE(validatorCalls, 1);
    QCOMPARE(observedCandidate.iconStyle.size, 128);
    QCOMPARE(observedCandidate.settingsRevision, quint64{8});
    QCOMPARE(outcome.status, PanelSettingsTransactionStatus::Prepared);
    QVERIFY(outcome.capabilityResolution.has_value());
    QVERIFY(outcome.capabilityResolution->available);
}

void PanelSettingsTransactionTest::capabilityRejectionStopsBeforePersistenceAndHostApplication()
{
    PanelDefinition current = PanelDefinition::defaults(
        QStringLiteral("bottom"),
        QStringLiteral("Bottom panel"),
        QStringLiteral("bottom"),
        true);
    current.settingsRevision = 9;
    const PanelDefinition original = current;
    const QVariantMap savedDefinition = current.toPersistedMap();
    PanelSettingsTransactionRequest request{
        QStringLiteral("bottom"),
        9,
        {{QStringLiteral("layout"), QStringLiteral("ring")}},
        {},
    };
    PanelSettingsTransactionOutcome outcome;
    int validatorCalls = 0;
    int persistenceCalls = 0;
    int hostCalls = 0;
    PanelDefinition observedCandidate;

    const auto draft = PanelSettingsTransaction::prepare(
        current,
        {},
        {},
        request,
        &outcome,
        [&validatorCalls, &observedCandidate](const PanelDefinition &candidate)
        {
            ++validatorCalls;
            observedCandidate = candidate;
            return ArchDock::PanelCapabilityResolver::resolve(
                candidate,
                ArchDock::PanelCapabilityResolver::productionHostProfile(
                    candidate.host.kind),
                ArchDock::PanelCapabilityResolver::proceduralThemeProfile(),
                ArchDock::PanelCapabilityResolver::productionRenderers(),
                ArchDock::PanelCapabilityResolver::productionPlatform());
        });
    if (draft.has_value())
    {
        ++persistenceCalls;
        ++hostCalls;
    }

    QVERIFY(!draft.has_value());
    QCOMPARE(validatorCalls, 1);
    QCOMPARE(observedCandidate.layout.pathType, QStringLiteral("ring"));
    QCOMPARE(observedCandidate.settingsRevision, quint64{10});
    QCOMPARE(persistenceCalls, 0);
    QCOMPARE(hostCalls, 0);
    QCOMPARE(current, original);
    QCOMPARE(current.toPersistedMap(), savedDefinition);
    QCOMPARE(current.settingsRevision, quint64{9});
    QCOMPARE(outcome.previousRevision, quint64{9});
    QCOMPARE(outcome.revision, quint64{9});
    QCOMPARE(outcome.status, PanelSettingsTransactionStatus::ValidationFailed);
    QCOMPARE(outcome.errorCode, QStringLiteral("capability-unavailable"));
    QCOMPARE(outcome.errorMessage, QStringLiteral("host-layout-unsupported"));
    QVERIFY(outcome.capabilityResolution.has_value());
    QVERIFY(!outcome.capabilityResolution->available);
    QCOMPARE(outcome.capabilityResolution->reason,
             ArchDock::CapabilityReasonCode::HostLayoutUnsupported);
    QVERIFY(outcome.hostResults.isEmpty());

    const QVariantMap serialized = outcome.toVariantMap();
    QCOMPARE(serialized.value(QStringLiteral("errorCode")).toString(),
             QStringLiteral("capability-unavailable"));
    QCOMPARE(serialized.value(QStringLiteral("revision")).toULongLong(),
             quint64{9});
    QCOMPARE(serialized.value(QStringLiteral("capabilityResolution"))
                 .toMap()
                 .value(QStringLiteral("reasonCode"))
                 .toString(),
             QStringLiteral("host-layout-unsupported"));
}

void PanelSettingsTransactionTest::requiredHostFailuresAreAttachedAndFailTheOutcome()
{
    const QList<PanelSettingsHostResult> results{
        {
            QStringLiteral("native-placement"),
            true,
            true,
            QStringLiteral("applied"),
            {},
            {{QStringLiteral("verified"), true}},
        },
        {
            QStringLiteral("native-visibility"),
            true,
            false,
            QStringLiteral("failed"),
            QStringLiteral("readback-mismatch"),
            {{QStringLiteral("rollbackSucceeded"), true}},
        },
    };
    QVERIFY(!PanelSettingsTransaction::requiredHostsSucceeded(results));

    PanelSettingsTransactionOutcome outcome;
    outcome.status = PanelSettingsTransactionStatus::HostFailed;
    outcome.panelId = QStringLiteral("bottom");
    outcome.expectedRevision = 3;
    outcome.previousRevision = 3;
    outcome.revision = 4;
    outcome.rollbackRevision = 5;
    outcome.rolledBack = true;
    outcome.errorCode = QStringLiteral("required-host-apply-failed");
    outcome.hostResults = results;
    const QVariantMap serialized = outcome.toVariantMap();

    QVERIFY(!serialized.value(QStringLiteral("success")).toBool());
    QCOMPARE(serialized.value(QStringLiteral("status")).toString(),
             QStringLiteral("host-failed"));
    QCOMPARE(serialized.value(QStringLiteral("rollbackRevision")).toULongLong(),
             quint64{5});
    QCOMPARE(serialized.value(QStringLiteral("hostResults")).toList().size(), 2);
}

QTEST_MAIN(PanelSettingsTransactionTest)

#include "PanelSettingsTransactionTest.moc"
