#include "NativeContainmentLifecycle.h"

#include <QtGlobal>

namespace ArchDock
{
NativeContainmentMatch classifyNativeContainmentMatch(std::optional<int> queryResult)
{
    if (!queryResult.has_value())
    {
        return {};
    }

    if (*queryResult >= 0)
    {
        return {NativeContainmentMatchStatus::Unique, *queryResult};
    }

    if (*queryResult == -1)
    {
        return {NativeContainmentMatchStatus::Missing, -1};
    }

    if (*queryResult == -2)
    {
        return {NativeContainmentMatchStatus::Conflict, -1};
    }

    return {};
}

NativeContainmentLifecycleIntent nativeContainmentLifecycleIntent(
    NativeContainmentLifecycleRequest request,
    const NativeContainmentLifecycleState &state)
{
    if (request == NativeContainmentLifecycleRequest::RemovePermanently)
    {
        return state.hostStatus == NativeContainmentHostStatus::Owned &&
                state.rendererAttached
            ? NativeContainmentLifecycleIntent::RemoveHostPermanently
            : NativeContainmentLifecycleIntent::NoAction;
    }

    if (state.hostStatus == NativeContainmentHostStatus::UnownedOrUnverified)
    {
        return NativeContainmentLifecycleIntent::NoAction;
    }

    if (state.hostStatus == NativeContainmentHostStatus::Missing)
    {
        if (request == NativeContainmentLifecycleRequest::CreateNew)
        {
            return NativeContainmentLifecycleIntent::CreateHost;
        }
        return state.recordVisible
            ? NativeContainmentLifecycleIntent::RecreateMissingHost
            : NativeContainmentLifecycleIntent::NoAction;
    }

    if (!state.rendererAttached)
    {
        return NativeContainmentLifecycleIntent::AttachRenderer;
    }

    if (state.recordVisible &&
        state.presentation == NativeContainmentPresentation::Hidden)
    {
        return NativeContainmentLifecycleIntent::ShowHost;
    }

    if (!state.recordVisible &&
        state.presentation == NativeContainmentPresentation::Shown)
    {
        return NativeContainmentLifecycleIntent::HideHost;
    }

    return NativeContainmentLifecycleIntent::NoAction;
}

NativeContainmentAssociation reconciledNativeContainmentAssociation(int containmentId,
                                                                     int controlAppletId,
                                                                     bool containmentExists,
                                                                     bool ownershipConfirmed)
{
    if (containmentId < 0 || !containmentExists || !ownershipConfirmed)
    {
        return {};
    }

    return {containmentId, qMax(-1, controlAppletId)};
}
}
