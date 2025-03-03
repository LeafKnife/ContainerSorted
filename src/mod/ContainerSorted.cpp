#include "mod/MyMod.h"

#include <algorithm>
#include <ll/api/memory/Hook.h>
#include <ll/api/memory/Memory.h>
#include <mc/deps/core/math/Vec3.h>
#include <mc/deps/ecs/WeakEntityRef.h>
#include <mc/deps/shared_types/legacy/ContainerType.h>
#include <mc/world/Container.h>
#include <mc/world/SimpleSparseContainer.h>
#include <mc/world/actor/Actor.h>
#include <mc/world/actor/player/Inventory.h>
#include <mc/world/actor/player/Player.h>
#include <mc/world/actor/player/PlayerInventory.h>
#include <mc/world/containers/ContainerEnumName.h>
#include <mc/world/containers/FullContainerName.h>
#include <mc/world/inventory/network/ContainerScreenContext.h>
#include <mc/world/inventory/network/ItemStackNetResult.h>
#include <mc/world/inventory/network/ItemStackRequestActionHandler.h>
#include <mc/world/inventory/network/ItemStackRequestActionTransferBase.h>
#include <mc/world/inventory/network/ItemStackRequestActionType.h>
#include <mc/world/inventory/network/ItemStackRequestSlotInfo.h>
#include <mc/world/item/Item.h>
#include <mc/world/item/ItemStack.h>
#include <mc/world/item/ItemStackBase.h>
#include <mc/world/level/BlockPos.h>
#include <mc/world/level/BlockSource.h>
#include <mc/world/level/block/Block.h>
#include <mc/world/level/block/actor/BlockActor.h>

namespace lk::hook {

inline void sortContainer(Container& container) {
    auto items = container.getSlotCopies();
    std::sort(items.begin(), items.end(), [](ItemStack const& itemA, ItemStack const& itemB) {
        if (itemA.getId() != itemB.getId()) return itemA.getId() > itemB.getId();
        if (itemA.getAuxValue() != itemB.getAuxValue()) return itemA.getAuxValue() > itemB.getAuxValue();
        return itemA.mCount > itemB.mCount;
    });
    container.removeAllItems();
    for (auto& item : items) {
        container.addItem(item);
    }
}

LL_TYPE_INSTANCE_HOOK(
    trasferHandlerHook,
    HookPriority::Normal,
    ItemStackRequestActionHandler,
    &ItemStackRequestActionHandler::_handleTransfer,
    ::ItemStackNetResult,
    ::ItemStackRequestActionTransferBase const& requestAction,
    bool                                        isSwap,
    bool                                        isSrcHintSlot,
    bool                                        isDstHintSlot
) {
    auto& src = ll::memory::dAccess<ItemStackRequestSlotInfo>(&requestAction.mSrc, 4);
    auto& dst = ll::memory::dAccess<ItemStackRequestSlotInfo>(&requestAction.mDst, 4);
    if (requestAction.mActionType != ItemStackRequestActionType::Swap
        && requestAction.mActionType != ItemStackRequestActionType::Place) {
        return origin(requestAction, isSwap, isSrcHintSlot, isDstHintSlot);
    }
    ::std::shared_ptr<::SimpleSparseContainer> srcContainer = _getOrInitSparseContainer(src.mFullContainerName);
    if (!srcContainer) return origin(requestAction, isSwap, isSrcHintSlot, isDstHintSlot);
    auto const& srcItem = srcContainer->getItem(src.mSlot);
    if (srcItem.getTypeName() != "minecraft:stick") return origin(requestAction, isSwap, isSrcHintSlot, isDstHintSlot);
    switch (dst.mFullContainerName.mName) {
    case ContainerEnumName::InventoryContainer:
    case ContainerEnumName::CombinedHotbarAndInventoryContainer: {
        if (dst.mSlot != 35) return origin(requestAction, isSwap, isSrcHintSlot, isDstHintSlot);
        auto inventory = mPlayer.mInventory->mInventory.get();
        sortContainer(*inventory);
        mPlayer.refreshInventory();
        return ItemStackNetResult::Error;
    }
    case ContainerEnumName::LevelEntityContainer:
    case ContainerEnumName::ShulkerBoxContainer:
    case ContainerEnumName::BarrelContainer: {
        // if (dst.mSlot != 0 && dst.mSlot != 1 && dst.mSlot != 2)
        if (dst.mSlot != 0) return origin(requestAction, isSwap, isSrcHintSlot, isDstHintSlot);
        auto& screenCtx  = getScreenContext();
        auto  screenType = screenCtx.mUnk2a0ccb.as<SharedTypes::Legacy::ContainerType>();
        if (screenType != SharedTypes::Legacy::ContainerType::Container)
            return origin(requestAction, isSwap, isSrcHintSlot, isDstHintSlot);

        Container* ct         = nullptr;
        auto       blockActor = screenCtx.tryGetBlockActor();
        auto       actor      = screenCtx.tryGetActor();
        if (actor) {
            Vec3 pos = actor->getPosition();
            ct       = actor->getDimensionBlockSource().tryGetContainer(BlockPos(pos));
        } else if (blockActor) {
            if (blockActor->mBlock->getTypeName() == "minecraft:ender_chest") {
                ct = mPlayer.getEnderChestContainer();
            } else {
                ct = blockActor->getContainer();
            }
        }
        if (!ct) return origin(requestAction, isSwap, isSrcHintSlot, isDstHintSlot);
        sortContainer(*ct);
        mPlayer.refreshInventory();
        return ItemStackNetResult::Error;
    }
    default:
        return origin(requestAction, isSwap, isSrcHintSlot, isDstHintSlot);
    }
}
void hookItemStackRequestActionHandlerTransfer() { trasferHandlerHook::hook(); };
} // namespace lk::hook