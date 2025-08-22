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
#include <mc/world/containers/managers/models/ContainerManagerModel.h>
#include <mc/world/containers/managers/models/HudContainerManagerModel.h>
#include <mc/world/inventory/network/ContainerScreenContext.h>
#include <mc/world/inventory/network/ItemStackNetResult.h>
#include <mc/world/inventory/network/ItemStackRequestActionHandler.h>
#include <mc/world/inventory/network/ItemStackRequestActionTransferBase.h>
#include <mc/world/inventory/network/ItemStackRequestActionType.h>
#include <mc/world/inventory/network/ItemStackRequestSlotInfo.h>
#include <mc/world/item/Item.h>
#include <mc/world/item/ItemLockHelper.h>
#include <mc/world/item/ItemStack.h>
#include <mc/world/item/ItemStackBase.h>
#include <mc/world/level/BlockPos.h>
#include <mc/world/level/BlockSource.h>
#include <mc/world/level/block/Block.h>
#include <mc/world/level/block/actor/BlockActor.h>
#include <mc/world/level/block/actor/ShulkerBoxBlockActor.h>
#include <optional>


namespace lk::hook {

inline void sortContainer(Container& container) {
    auto items = container.getSlotCopies();
    std::sort(items.begin(), items.end(), [](ItemStack const& itemA, ItemStack const& itemB) {
        auto pItemA = itemA.getItem();
        auto pItemB = itemB.getItem();
        if (pItemA != nullptr && pItemB != nullptr && pItemA->mCreativeCategory != pItemB->mCreativeCategory)
            return (short)pItemA->mCreativeCategory < (short)pItemA->mCreativeCategory;
        if (itemA.getId() != itemB.getId()) return itemA.getId() < itemB.getId();
        if (itemA.getAuxValue() != itemB.getAuxValue()) return itemA.getAuxValue() < itemB.getAuxValue();
        return itemA.mCount < itemB.mCount;
    });
    container.removeAllItems();
    for (auto& item : items) {
        container.addItem(item);
    }
}

inline void sortInventory(Container& container) {
    auto items = container.getSlotCopies();
    std::sort(items.begin() + 9, items.end(), [](ItemStack const& itemA, ItemStack const& itemB) {
        auto pItemA = itemA.getItem();
        auto pItemB = itemB.getItem();
        if (pItemA != nullptr && pItemB != nullptr && pItemA->mCreativeCategory != pItemB->mCreativeCategory)
            return (short)pItemA->mCreativeCategory < (short)pItemA->mCreativeCategory;
        if (itemA.getId() != itemB.getId()) return itemA.getId() < itemB.getId();
        if (itemA.getAuxValue() != itemB.getAuxValue()) return itemA.getAuxValue() < itemB.getAuxValue();
        return itemA.mCount < itemB.mCount;
    });
    container.removeAllItems();
    for (auto& item : items) {
        container.addItem(item);
    }
}

inline void transferInventory2Container(Container& inventory, Container& container, bool all = true) {
    for (auto item = inventory.begin() + 9; item != inventory.end(); ++item) {
        if (item->isNull()) continue;
        ItemStackBase const& it = *item;
        if (ItemLockHelper::getItemLockMode(it) != ItemLockMode::None) continue;
        if (!all) {
            if (container.getItemCount(*item) == 0) continue;
        }
        if (container.addItem(*item)) {
            item->setNull(std::nullopt);
        } else {
            return;
        }
    }
}

inline void transferInventory2ShulkerBox(Container& inventory, Container& container, bool all = true) {
    for (auto item = inventory.begin() + 9; item != inventory.end(); ++item) {
        if (item->isNull()) continue;
        ItemStackBase const& it = *item;
        if (ItemLockHelper::getItemLockMode(it) != ItemLockMode::None) continue;
        if (!ShulkerBoxBlockActor::itemAllowed(*item)) continue;
        if (!all) {
            if (container.getItemCount(*item) == 0) continue;
        }
        if (container.addItem(*item)) {
            item->setNull(std::nullopt);
        } else {
            return;
        }
    }
}

inline void transferContainer2Inventory(Container& inventory, Container& container, bool all = true) {
    for (auto slot = 0; slot < container.getContainerSize(); slot++) {
        auto& item = container[slot];
        if (item.isNull()) continue;
        if (!all) {
            if (inventory.getItemCount(item) == 0) continue;
        }
        if (inventory.addItem(item)) {
            item.setNull(std::nullopt);
            container.setContainerChanged(slot);
        } else {
            return;
        }
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
    auto inventory = mPlayer.mInventory->mInventory.get();
    switch (dst.mFullContainerName.mName) {
    case ContainerEnumName::InventoryContainer:
    case ContainerEnumName::CombinedHotbarAndInventoryContainer: {
        if (dst.mSlot != 35) return origin(requestAction, isSwap, isSrcHintSlot, isDstHintSlot);
        sortInventory(*inventory);
        mPlayer.refreshInventory();
        return ItemStackNetResult::Error;
    }
    case ContainerEnumName::LevelEntityContainer:
    case ContainerEnumName::ShulkerBoxContainer:
    case ContainerEnumName::BarrelContainer: {
        // if (dst.mSlot != 0 && dst.mSlot != 1 && dst.mSlot != 2)
        //     return origin(requestAction, isSwap, isSrcHintSlot, isDstHintSlot);
        auto& screenCtx  = getScreenContext();
        auto  screenType = screenCtx.mScreenContainerType;
        if (screenType != SharedTypes::Legacy::ContainerType::Container)
            return origin(requestAction, isSwap, isSrcHintSlot, isDstHintSlot);
        Container* ct         = nullptr;
        auto       blockActor = screenCtx.tryGetBlockActor();
        auto       actor      = screenCtx.tryGetActor();
        if (actor) {
            Vec3 pos = actor->getPosition();
            ct       = actor->getDimensionBlockSource().tryGetContainer(BlockPos(pos));
        } else if (blockActor) {
            if (blockActor->mType == BlockActorType::EnderChest) {
                ct = mPlayer.getEnderChestContainer();
            } else {
                ct = blockActor->getContainer();
            }
        }
        if (!ct) return origin(requestAction, isSwap, isSrcHintSlot, isDstHintSlot);
        switch (dst.mSlot) {
        case 0:
            sortContainer(*ct);
            break;
        case 1:
            if (blockActor && blockActor->mType == BlockActorType::ShulkerBox) {
                transferInventory2ShulkerBox(*inventory, *ct);
                break;
            }
            transferInventory2Container(*inventory, *ct);
            break;
        case 2:
            transferContainer2Inventory(*inventory, *ct);
            break;
        case 3:
            if (blockActor && blockActor->mType == BlockActorType::ShulkerBox) {
                transferInventory2ShulkerBox(*inventory, *ct, false);
                break;
            }
            transferInventory2Container(*inventory, *ct, false);
            break;
        case 4:
            transferContainer2Inventory(*inventory, *ct, false);
            break;
        default:
            return origin(requestAction, isSwap, isSrcHintSlot, isDstHintSlot);
        }
        mPlayer.refreshInventory();
        return ItemStackNetResult::Error;
    }
    default:
        return origin(requestAction, isSwap, isSrcHintSlot, isDstHintSlot);
    }
}
void hookItemStackRequestActionHandlerTransfer() { trasferHandlerHook::hook(); };
} // namespace lk::hook