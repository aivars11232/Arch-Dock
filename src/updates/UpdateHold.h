// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// While an update runs, from Update Now (after the unapplied-changes dialog)
// or Restart Arch Dock until the restart, every setting change is refused,
// wherever it comes from: Studio, panel menus, applets, profile shortcuts or
// D-Bus. So the restart cuts off nothing a person is changing. The hold lives
// only in this process: no slot or property on the bus sets or lifts it.
//
// Changes already under way that wait on Plasma are counted, and the
// finisher starts only when none is left.

#include <QString>
#include <QVariantMap>

namespace ArchDock::Updates
{
class UpdateHold
{
public:
    [[nodiscard]] static bool held() { return state().held; }
    static void take() { state().held = true; }
    static void release() { state().held = false; }

    static void beginChange() { ++state().changes; }
    static void endChange()
    {
        if (state().changes > 0)
            --state().changes;
    }
    [[nodiscard]] static int changesUnderWay() { return state().changes; }

    // The refusal a held change gives.
    [[nodiscard]] static QString errorCode() { return QStringLiteral("update-active"); }
    [[nodiscard]] static QString refusal()
    {
        return QStringLiteral("Arch Dock is being updated. Try again when it has finished.");
    }
    // A refused change, as the writing slots answer.
    [[nodiscard]] static QVariantMap refusedChange()
    {
        return {{QStringLiteral("success"), false}, {QStringLiteral("errorCode"), errorCode()},
                {QStringLiteral("message"), refusal()}};
    }

private:
    struct State
    {
        bool held = false;
        int changes = 0;
    };
    static State &state()
    {
        static State instance;
        return instance;
    }
};

// A change under way while it waits on Plasma, counted for as long as it lives.
class ChangeUnderWay
{
public:
    ChangeUnderWay() { UpdateHold::beginChange(); }
    ~ChangeUnderWay() { UpdateHold::endChange(); }
    ChangeUnderWay(const ChangeUnderWay &) = delete;
    ChangeUnderWay &operator=(const ChangeUnderWay &) = delete;
};
} // namespace ArchDock::Updates
