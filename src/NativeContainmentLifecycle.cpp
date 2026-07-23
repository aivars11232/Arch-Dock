#include "NativeContainmentLifecycle.h"

#include <QtGlobal>

namespace ArchDock
{
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