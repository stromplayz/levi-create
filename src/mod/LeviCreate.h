#pragma once

#include "ll/api/event/Listener.h"
#include "ll/api/event/command/ServerCommandRegisterEvent.h"
#include "ll/api/event/player/PlayerInteractBlockEvent.h"
#include "ll/api/mod/NativeMod.h"

#include "core/PlayerState.h"

#include <filesystem>
#include <memory>
#include <string>

namespace lc {

class LeviCreate {
public:
    static LeviCreate& getInstance();

    LeviCreate() : mSelf(*ll::mod::NativeMod::current()) {}

    [[nodiscard]] ll::mod::NativeMod& getSelf() const { return mSelf; }

    bool load();
    bool enable();
    bool disable();

    PlayerStateMap& states() { return mStates; }

    std::filesystem::path modelsDir() const;
    std::filesystem::path modelFile(std::string const& name) const;

    static std::string const& wandItem();

private:
    ll::mod::NativeMod& mSelf;
    PlayerStateMap      mStates;
    std::shared_ptr<ll::event::Listener<ll::event::PlayerInteractBlockEvent>>   mWandListener;
    std::shared_ptr<ll::event::Listener<ll::event::ServerCommandRegisterEvent>> mCommandRegisterListener;
};

} // namespace lc
