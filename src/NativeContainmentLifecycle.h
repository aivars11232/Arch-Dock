#pragma once

namespace ArchDock
{
struct NativeContainmentAssociation
{
    int containmentId = -1;
    int controlAppletId = -1;
};

[[nodiscard]] NativeContainmentAssociation reconciledNativeContainmentAssociation(
    int containmentId,
    int controlAppletId,
    bool containmentExists,
    bool ownershipConfirmed);
}