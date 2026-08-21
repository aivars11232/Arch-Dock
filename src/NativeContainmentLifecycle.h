#pragma once

#include <optional>

namespace ArchDock
{
enum class NativeContainmentHostStatus
{
    Missing,
    Owned,
    UnownedOrUnverified,
};

enum class NativeContainmentPresentation
{
    Hidden,
    Shown,
};

enum class NativeContainmentLifecycleRequest
{
    CreateNew,
    Synchronize,
    RemovePermanently,
};

enum class NativeContainmentLifecycleIntent
{
    NoAction,
    CreateHost,
    AttachRenderer,
    ShowHost,
    HideHost,
    RecreateMissingHost,
    RemoveHostPermanently,
};

struct NativeContainmentLifecycleState
{
    bool recordVisible = false;
    NativeContainmentHostStatus hostStatus = NativeContainmentHostStatus::Missing;
    bool rendererAttached = false;
    NativeContainmentPresentation presentation = NativeContainmentPresentation::Hidden;
};

struct NativeContainmentAssociation
{
    int containmentId = -1;
    int controlAppletId = -1;
};

enum class NativeContainmentMatchStatus
{
    QueryFailed,
    Missing,
    Unique,
    Conflict,
};

struct NativeContainmentMatch
{
    NativeContainmentMatchStatus status = NativeContainmentMatchStatus::QueryFailed;
    int containmentId = -1;
};

[[nodiscard]] NativeContainmentMatch classifyNativeContainmentMatch(
    std::optional<int> queryResult);

[[nodiscard]] NativeContainmentLifecycleIntent nativeContainmentLifecycleIntent(
    NativeContainmentLifecycleRequest request,
    const NativeContainmentLifecycleState &state);

[[nodiscard]] NativeContainmentAssociation reconciledNativeContainmentAssociation(
    int containmentId,
    int controlAppletId,
    bool containmentExists,
    bool ownershipConfirmed);
}
