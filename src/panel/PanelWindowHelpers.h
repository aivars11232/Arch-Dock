// Small helpers shared by PanelWindow's implementation files
// (PanelWindow*.cpp in this folder) and their IPC regression fixture.

#pragma once

#include <QDBusInterface>
#include <QJsonArray>
#include <QJsonDocument>
#include <QScreen>
#include <QSize>
#include <QString>
#include <QWindow>

#include <functional>

namespace PanelWindowHelpers
{
// Keep the native script call in one seam so a two-process regression can
// exercise a drop arriving while background recovery awaits Plasma.
struct PlasmaScriptSuperseded final {};

inline QDBusMessage callPlasmaScript(QDBusInterface &shell, const QString &script,
                                    bool backgroundRecovery,
                                    const std::function<bool()> &stillCurrent = {})
{
    if (stillCurrent && !stillCurrent())
        throw PlasmaScriptSuperseded{};
    // A drop client can be waiting synchronously for this backend while
    // recovery awaits that client's geometry reply. Keep dispatching backend
    // method calls during recovery; never reenter the client's drag delivery.
    const QDBusMessage reply = shell.call(
        backgroundRecovery ? QDBus::BlockWithGui : QDBus::Block,
        QStringLiteral("evaluateScript"), script);
    // A foreground mutation can commit while this recovery call dispatches
    // events. Its newer intent must survive any subsequent write or rollback.
    if (stillCurrent && !stillCurrent())
        throw PlasmaScriptSuperseded{};
    return reply;
}

// `value` as a quoted, escaped JavaScript string literal, safe to splice into
// a Plasma desktop script.
inline QString plasmaScriptStringLiteral(const QString &value)
{
    QJsonArray values;
    values.append(value);
    const QString encoded = QString::fromUtf8(
        QJsonDocument(values).toJson(QJsonDocument::Compact));
    return encoded.mid(1, encoded.size() - 2);
}

// The content types an Arch Dock native panel can have.
inline bool isNativeDockPanelType(const QString &type)
{
    return type == QStringLiteral("empty") ||
        type == QStringLiteral("launcher") ||
        type == QStringLiteral("tasks") ||
        type == QStringLiteral("hybrid");
}

// The screen edges a native panel can stand on (a free panel's edge is "free").
inline bool isNativeDockPanelEdge(const QString &edge)
{
    return edge == QStringLiteral("top") ||
        edge == QStringLiteral("bottom") ||
        edge == QStringLiteral("left") ||
        edge == QStringLiteral("right");
}

// The content types drawn by Arch Dock's dock applet (org.archdock.dock). An
// empty native panel is a Plasma panel without that applet.
inline bool panelTypeNeedsDockApplet(const QString &type)
{
    return type == QStringLiteral("launcher") ||
        type == QStringLiteral("tasks") ||
        type == QStringLiteral("hybrid");
}

// Shrinks a utility window (and its minimum size) to its screen's available
// area, so no part of it opens off screen.
inline void fitUtilityWindow(QWindow *window)
{
    if (!window || !window->screen()) return;
    const QSize available = window->screen()->availableGeometry().size();
    if (available.isEmpty()) return;
    window->setMinimumSize(window->minimumSize().boundedTo(available));
    window->resize(window->size().boundedTo(available));
}
}
