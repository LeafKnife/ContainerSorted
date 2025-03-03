#include "mod/MyMod.h"

#include "ll/api/mod/RegisterHelper.h"

namespace lk {

MyMod& MyMod::getInstance() {
    static MyMod instance;
    return instance;
}

bool MyMod::load() {
    getSelf().getLogger().debug("Loading...");
    // Code for loading the mod goes here.
    return true;
}

bool MyMod::enable() {
    getSelf().getLogger().debug("Enabling...");
    hook::hookItemStackRequestActionHandlerTransfer();
    // Code for enabling the mod goes here.
    return true;
}

bool MyMod::disable() {
    getSelf().getLogger().debug("Disabling...");
    // Code for disabling the mod goes here.
    return true;
}

} // namespace lk

LL_REGISTER_MOD(lk::MyMod, lk::MyMod::getInstance());
