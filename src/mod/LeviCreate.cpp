#include "mod/LeviCreate.h"

#include "cmds/Commands.h"

#include "ll/api/event/EventBus.h"
#include "ll/api/mod/RegisterHelper.h"

#include "mc/world/actor/player/Player.h"
#include "mc/world/item/ItemStack.h"
#include "mc/world/level/BlockPos.h"

#include <filesystem>

namespace lc {

LeviCreate& LeviCreate::getInstance() {
    static LeviCreate inst;
    return inst;
}

std::string const& LeviCreate::wandItem() {
    static const std::string kWand = "minecraft:iron_axe";
    return kWand;
}

std::filesystem::path LeviCreate::modelsDir() const { return getSelf().getDataDir() / "models"; }

std::filesystem::path LeviCreate::modelFile(std::string const& name) const { return modelsDir() / (name + ".json"); }

bool LeviCreate::load() {
    std::error_code ec;
    std::filesystem::create_directories(modelsDir(), ec);
    getSelf().getLogger().info("levi-create loaded. Models are stored in: {}", modelsDir().string());
    return true;
}

bool LeviCreate::enable() {
    using ll::event::PlayerInteractBlockEvent;

    // Wand tool: iron axe. Sneak + right-click sets pos #1, plain right-click sets pos #2.
    mWandListener = ll::event::EventBus::getInstance().emplaceListener<PlayerInteractBlockEvent>(
        [this](PlayerInteractBlockEvent& ev) {
            auto&      player = ev.self();
            ItemStack& item   = ev.item();
            if (item.getTypeName() != wandItem()) return;

            auto&           sel = mStates.get(player.getRealName()).sel;
            BlockPos const& bp  = ev.blockPos();
            int3            p{bp.x, bp.y, bp.z};
            sel.dimId = static_cast<int>(player.getDimensionId());

            if (player.isSneaking()) {
                sel.p1  = p;
                sel.has1 = true;
                player.sendMessage(
                    "§a[LeviCreate]§r Pos #1 set: " + std::to_string(p.x) + ", " + std::to_string(p.y) + ", "
                    + std::to_string(p.z)
                );
            } else {
                sel.p2  = p;
                sel.has2 = true;
                player.sendMessage(
                    "§b[LeviCreate]§r Pos #2 set: " + std::to_string(p.x) + ", " + std::to_string(p.y) + ", "
                    + std::to_string(p.z)
                );
            }
            ev.cancel();
        }
    );

    cmd::registerCommands();

    getSelf().getLogger().info("levi-create enabled. Use /vc help or /vc menu in game.");
    return true;
}

bool LeviCreate::disable() {
    if (mWandListener) {
        ll::event::EventBus::getInstance()
            .removeListener<ll::event::PlayerInteractBlockEvent>(mWandListener);
        mWandListener.reset();
    }
    getSelf().getLogger().info("levi-create disabled.");
    return true;
}

} // namespace lc

LL_REGISTER_MOD(lc::LeviCreate, lc::LeviCreate::getInstance());
