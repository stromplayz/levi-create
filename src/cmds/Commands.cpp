#include "cmds/Commands.h"

#include "core/PlayerOps.h"
#include "core/VoxelModel.h"
#include "core/WorldOps.h"
#include "mod/LeviCreate.h"
#include "ui/Forms.h"

#include "ll/api/command/CommandHandle.h"
#include "ll/api/command/CommandRegistrar.h"

#include "mc/deps/core/math/Vec3.h"
#include "mc/server/commands/CommandOrigin.h"
#include "mc/server/commands/CommandOutput.h"
#include "mc/server/commands/CommandPermissionLevel.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/level/BlockSource.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <utility>
#include <vector>
#include <cctype>

namespace lc::cmd {

namespace {

Player* playerOf(CommandOrigin const& origin, CommandOutput& output) {
    Actor* ent = origin.getEntity();
    if (!ent || !ent->isPlayer()) {
        output.error("This command must be used by a player in the world.");
        return nullptr;
    }
    return static_cast<Player*>(ent);
}

bool requireSelectionOrErr(Player& p, CommandOutput& output, int3& mn, int3& mx) {
    std::string err;
    if (!ops::requireSelection(p, mn, mx, err)) {
        output.error(err);
        return false;
    }
    return true;
}

Clipboard* clipboardOrErr(Player& p, CommandOutput& output) {
    Clipboard* clip = ops::clipboardOf(p);
    if (!clip) output.error("Clipboard is empty. Copy something first with /vc copy.");
    return clip;
}

std::string normBlock(std::string const& n) {
    if (n.empty()) return n;
    if (n.find(':') == std::string::npos) return "minecraft:" + n;
    return n;
}

void reportPlace(CommandOutput& output, std::string const& label, PlaceResult const& res, int3 const* origin) {
    std::string msg = label + ": placed " + formatVolume(res.placed) + " blocks";
    if (res.airSkipped) msg += ", skipped " + formatVolume(res.airSkipped) + " air";
    if (res.failed) msg += ", §cfailed " + formatVolume(res.failed) + "§r";
    if (origin) {
        msg += " at (" + std::to_string(origin->x) + " " + std::to_string(origin->y) + " "
             + std::to_string(origin->z) + ")";
    }
    output.success(msg);
}

bool validModelName(std::string const& n) {
    if (n.empty() || n.size() > 32) return false;
    for (char c : n) {
        if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '-')) return false;
    }
    return true;
}

} // namespace (helpers)

// ---- param structs (reflection-based command parameters) ----------------
// NOTE: these MUST have external linkage (named namespace) — the command
// system's boost::pfr reflection rejects anonymous-namespace types.

struct PasteParams {
    float scale;
    float yaw;
    float pitch;
};
struct MoveParams {
    int dx;
    int dy;
    int dz;
};
struct RotateParams {
    float       degrees;
    std::string axis;
};
struct ScaleParams {
    float factor;
};
struct AxisParams {
    std::string axis;
};
struct FillParams {
    std::string block;
};
struct ReplaceParams {
    std::string from;
    std::string to;
};
struct SaveParams {
    std::string name;
};
struct LoadParams {
    std::string name;
    bool        paste;
};
struct DeleteParams {
    std::string name;
};
struct MiniParams {
    int factor;
};

void registerCommands() {
    auto& cmd = ll::command::CommandRegistrar::getServerInstance().getOrCreateCommand(
        "vc",
        "LeviCreate — voxel building toolkit (build anything, any shape, any scale)",
        CommandPermissionLevel::GameDirectors
    );
    cmd.alias("levicreate");

    // ---- help ------------------------------------------------------------
    cmd.overload().text("help").execute([](CommandOrigin const&, CommandOutput& output, ll::command::EmptyParam const&) {
        output.success(forms::helpText());
    });

    // ---- UI --------------------------------------------------------------
    cmd.overload().text("menu").execute([](CommandOrigin const& origin, CommandOutput& output, ll::command::EmptyParam const&) {
        auto* pl = playerOf(origin, output);
        if (!pl) return;
        forms::openMainMenu(*pl);
        output.success("Opened the LeviCreate menu.");
    });
    cmd.overload().text("ui").execute([](CommandOrigin const& origin, CommandOutput& output, ll::command::EmptyParam const&) {
        auto* pl = playerOf(origin, output);
        if (!pl) return;
        forms::openMainMenu(*pl);
        output.success("Opened the LeviCreate menu.");
    });

    // ---- wand ------------------------------------------------------------
    cmd.overload().text("wand").execute([](CommandOrigin const&, CommandOutput& output, ll::command::EmptyParam const&) {
        output.success(
            "Wand tool: hold an iron axe (minecraft:iron_axe).\n"
            "§aSneak + right-click a block§r — set pos #1\n"
            "§bRight-click a block§r — set pos #2\n"
            "Or use /vc pos1 and /vc pos2 to select at your feet."
        );
    });

    // ---- pos1 / pos2 -----------------------------------------------------
    cmd.overload().text("pos1").execute([](CommandOrigin const& origin, CommandOutput& output, ll::command::EmptyParam const&) {
        auto* pl = playerOf(origin, output);
        if (!pl) return;
        auto& sel = LeviCreate::getInstance().states().get(pl->getRealName()).sel;
        sel.p1   = ops::feetBlock(*pl);
        sel.has1 = true;
        output.success("Pos #1 set at your feet.");
    });
    cmd.overload().text("pos2").execute([](CommandOrigin const& origin, CommandOutput& output, ll::command::EmptyParam const&) {
        auto* pl = playerOf(origin, output);
        if (!pl) return;
        auto& sel = LeviCreate::getInstance().states().get(pl->getRealName()).sel;
        sel.p2   = ops::feetBlock(*pl);
        sel.has2 = true;
        output.success("Pos #2 set at your feet.");
    });

    // ---- copy / cut ------------------------------------------------------
    cmd.overload().text("copy").execute([](CommandOrigin const& origin, CommandOutput& output, ll::command::EmptyParam const&) {
        auto* pl = playerOf(origin, output);
        if (!pl) return;
        int3 mn, mx;
        if (!requireSelectionOrErr(*pl, output, mn, mx)) return;
        std::string err;
        auto        m = WorldOps::captureRegion(pl->getDimensionBlockSource(), mn, mx, err);
        if (!m) {
            output.error(err);
            return;
        }
        std::uint64_t n = m->countNonAir();
        Clipboard     clip;
        clip.model      = std::move(*m);
        clip.sourceName = "selection";
        auto& data      = LeviCreate::getInstance().states().get(pl->getRealName());
        data.clip       = std::move(clip);
        output.success(
            "Copied " + formatVolume(n) + " blocks (" + formatVolume(data.clip->model.volume())
            + " region). Use /vc paste."
        );
    });
    cmd.overload().text("cut").execute([](CommandOrigin const& origin, CommandOutput& output, ll::command::EmptyParam const&) {
        auto* pl = playerOf(origin, output);
        if (!pl) return;
        int3 mn, mx;
        if (!requireSelectionOrErr(*pl, output, mn, mx)) return;
        std::string err;
        auto        m = WorldOps::captureRegion(pl->getDimensionBlockSource(), mn, mx, err);
        if (!m) {
            output.error(err);
            return;
        }
        Clipboard clip;
        clip.model      = std::move(*m);
        clip.sourceName = "cut";
        std::uint64_t n = clip.model.countNonAir();
        LeviCreate::getInstance().states().get(pl->getRealName()).clip = std::move(clip);
        std::uint64_t changed = 0;
        if (!ops::clearRegion(*pl, mn, mx, changed, err)) {
            output.error(err);
            return;
        }
        output.success("Cut " + formatVolume(n) + " blocks to clipboard (cleared " + formatVolume(changed) + ").");
    });

    // ---- paste -----------------------------------------------------------
    cmd.overload<PasteParams>()
        .text("paste")
        .optional("scale")
        .optional("yaw")
        .optional("pitch")
        .execute([](CommandOrigin const& origin, CommandOutput& output, PasteParams const& params) {
            auto* pl = playerOf(origin, output);
            if (!pl) return;
            Clipboard* clip = clipboardOrErr(*pl, output);
            if (!clip) return;

            ModelTransform t;
            t.scale        = params.scale > 0.0f ? params.scale : 1.0f;
            t.yawDegrees   = params.yaw;
            t.pitchDegrees = params.pitch;
            VoxelModel m   = transformModel(clip->model, t);
            if (m.empty()) {
                output.error("Paste failed: transformed model is empty or too large.");
                return;
            }
            int3 origin2 = ops::feetAnchor(*pl, m.sx, m.sy, m.sz);
            PlaceResult res;
            std::string err;
            if (!ops::pasteModel(*pl, m, origin2, false, res, err)) {
                output.error(err);
                return;
            }
            reportPlace(output, "Pasted", res, &origin2);
        });

    // ---- move ------------------------------------------------------------
    cmd.overload<MoveParams>()
        .text("move")
        .required("dx")
        .required("dy")
        .required("dz")
        .execute([](CommandOrigin const& origin, CommandOutput& output, MoveParams const& params) {
            auto* pl = playerOf(origin, output);
            if (!pl) return;
            int3 mn, mx;
            if (!requireSelectionOrErr(*pl, output, mn, mx)) return;
            auto& bs = pl->getDimensionBlockSource();
            std::string err;
            auto        m = WorldOps::captureRegion(bs, mn, mx, err);
            if (!m) {
                output.error(err);
                return;
            }
            ops::pushUndoSnapshot(*pl, mn, mx);
            std::uint64_t changed = 0;
            if (!ops::clearRegion(*pl, mn, mx, changed, err)) {
                output.error(err);
                return;
            }
            int3 dst{mn.x + params.dx, mn.y + params.dy, mn.z + params.dz};
            PlaceResult res;
            if (!ops::pasteModel(*pl, *m, dst, true, res, err)) {
                output.error(err);
                return;
            }
            LeviCreate::getInstance().states().get(pl->getRealName()).sel.reset();
            reportPlace(output, "Moved", res, &dst);
            output.success("§7Selection reset — reselect at the new location to continue.");
        });

    // ---- rotate (any degree) ----------------------------------------------
    cmd.overload<RotateParams>()
        .text("rotate")
        .required("degrees")
        .optional("axis")
        .execute([](CommandOrigin const& origin, CommandOutput& output, RotateParams const& params) {
            auto* pl = playerOf(origin, output);
            if (!pl) return;
            int3 mn, mx;
            if (!requireSelectionOrErr(*pl, output, mn, mx)) return;
            int axis = 1;
            if (params.axis == "x" || params.axis == "X") axis = 0;
            else if (params.axis == "z" || params.axis == "Z") axis = 2;
            std::string err;
            auto        m = WorldOps::captureRegion(pl->getDimensionBlockSource(), mn, mx, err);
            if (!m) {
                output.error(err);
                return;
            }
            VoxelModel out = rotateModel(*m, axis, static_cast<double>(params.degrees));
            if (out.empty()) {
                output.error("Rotation produced an empty model (region too large?).");
                return;
            }
            ops::pushUndoSnapshot(*pl, mn, mx);
            std::uint64_t changed = 0;
            if (!ops::clearRegion(*pl, mn, mx, changed, err)) {
                output.error(err);
                return;
            }
            PlaceResult res;
            if (!ops::pasteModel(*pl, out, mn, false, res, err)) {
                output.error(err);
                return;
            }
            LeviCreate::getInstance().states().get(pl->getRealName()).sel.reset();
            reportPlace(output, "Rotated " + std::to_string(params.degrees) + "°", res, &mn);
        });

    // ---- scale ------------------------------------------------------------
    cmd.overload<ScaleParams>()
        .text("scale")
        .required("factor")
        .execute([](CommandOrigin const& origin, CommandOutput& output, ScaleParams const& params) {
            auto* pl = playerOf(origin, output);
            if (!pl) return;
            if (params.factor <= 0.0f || params.factor > 16.0f) {
                output.error("Scale factor must be between 0.0625 and 16.");
                return;
            }
            int3 mn, mx;
            if (!requireSelectionOrErr(*pl, output, mn, mx)) return;
            std::string err;
            auto        m = WorldOps::captureRegion(pl->getDimensionBlockSource(), mn, mx, err);
            if (!m) {
                output.error(err);
                return;
            }
            VoxelModel out = scaleModel(*m, params.factor);
            if (out.empty()) {
                output.error("Scaling produced an empty model (result too large?).");
                return;
            }
            ops::pushUndoSnapshot(*pl, mn, mx);
            std::uint64_t changed = 0;
            if (!ops::clearRegion(*pl, mn, mx, changed, err)) {
                output.error(err);
                return;
            }
            PlaceResult res;
            if (!ops::pasteModel(*pl, out, mn, false, res, err)) {
                output.error(err);
                return;
            }
            LeviCreate::getInstance().states().get(pl->getRealName()).sel.reset();
            reportPlace(output, "Scaled x" + std::to_string(params.factor), res, &mn);
        });

    // ---- flip --------------------------------------------------------------
    cmd.overload<AxisParams>()
        .text("flip")
        .required("axis")
        .execute([](CommandOrigin const& origin, CommandOutput& output, AxisParams const& params) {
            auto* pl = playerOf(origin, output);
            if (!pl) return;
            int axis = -1;
            if (params.axis == "x" || params.axis == "X") axis = 0;
            else if (params.axis == "y" || params.axis == "Y") axis = 1;
            else if (params.axis == "z" || params.axis == "Z") axis = 2;
            if (axis < 0) {
                output.error("Axis must be x, y or z.");
                return;
            }
            int3 mn, mx;
            if (!requireSelectionOrErr(*pl, output, mn, mx)) return;
            std::string err;
            auto        m = WorldOps::captureRegion(pl->getDimensionBlockSource(), mn, mx, err);
            if (!m) {
                output.error(err);
                return;
            }
            VoxelModel out = flipModel(*m, axis);
            ops::pushUndoSnapshot(*pl, mn, mx);
            std::uint64_t changed = 0;
            if (!ops::clearRegion(*pl, mn, mx, changed, err)) {
                output.error(err);
                return;
            }
            PlaceResult res;
            if (!ops::pasteModel(*pl, out, mn, false, res, err)) {
                output.error(err);
                return;
            }
            reportPlace(output, "Flipped " + params.axis, res, &mn);
        });

    // ---- mini / expand -----------------------------------------------------
    cmd.overload<MiniParams>()
        .text("mini")
        .optional("factor")
        .execute([](CommandOrigin const& origin, CommandOutput& output, MiniParams const& params) {
            auto* pl = playerOf(origin, output);
            if (!pl) return;
            int  f   = params.factor > 0 ? params.factor : 4;
            std::string msg, err;
            if (!ops::miniOrExpand(*pl, f, false, msg, err)) {
                output.error(err);
                return;
            }
            output.success(msg);
        });
    cmd.overload<MiniParams>()
        .text("expand")
        .optional("factor")
        .execute([](CommandOrigin const& origin, CommandOutput& output, MiniParams const& params) {
            auto* pl = playerOf(origin, output);
            if (!pl) return;
            int  f   = params.factor > 0 ? params.factor : 4;
            std::string msg, err;
            if (!ops::miniOrExpand(*pl, f, true, msg, err)) {
                output.error(err);
                return;
            }
            output.success(msg);
        });

    // ---- fill / replace / clear --------------------------------------------
    cmd.overload<FillParams>()
        .text("fill")
        .required("block")
        .execute([](CommandOrigin const& origin, CommandOutput& output, FillParams const& params) {
            auto* pl = playerOf(origin, output);
            if (!pl) return;
            int3 mn, mx;
            if (!requireSelectionOrErr(*pl, output, mn, mx)) return;
            std::string name = normBlock(params.block);
            std::string err;
            std::uint64_t changed = 0;
            if (!WorldOps::fillRegion(pl->getDimensionBlockSource(), mn, mx, name, false, changed, err)) {
                output.error(err);
                return;
            }
            output.success("Filled " + formatVolume(changed) + " blocks with " + name + ".");
        });
    cmd.overload<ReplaceParams>()
        .text("replace")
        .required("from")
        .required("to")
        .execute([](CommandOrigin const& origin, CommandOutput& output, ReplaceParams const& params) {
            auto* pl = playerOf(origin, output);
            if (!pl) return;
            int3 mn, mx;
            if (!requireSelectionOrErr(*pl, output, mn, mx)) return;
            std::string from = normBlock(params.from);
            std::string to   = normBlock(params.to);
            ops::pushUndoSnapshot(*pl, mn, mx);
            std::string   err;
            std::uint64_t changed = 0;
            if (!WorldOps::replaceRegion(pl->getDimensionBlockSource(), mn, mx, from, to, changed, err)) {
                output.error(err);
                return;
            }
            output.success("Replaced " + formatVolume(changed) + " blocks: " + from + " → " + to + ".");
        });
    cmd.overload().text("clear").execute([](CommandOrigin const& origin, CommandOutput& output, ll::command::EmptyParam const&) {
        auto* pl = playerOf(origin, output);
        if (!pl) return;
        int3 mn, mx;
        if (!requireSelectionOrErr(*pl, output, mn, mx)) return;
        std::uint64_t changed = 0;
        std::string   err;
        if (!ops::clearRegion(*pl, mn, mx, changed, err)) {
            output.error(err);
            return;
        }
        output.success("Cleared " + formatVolume(changed) + " blocks. Use /vc undo to restore.");
    });

    // ---- undo ---------------------------------------------------------------
    cmd.overload().text("undo").execute([](CommandOrigin const& origin, CommandOutput& output, ll::command::EmptyParam const&) {
        auto* pl = playerOf(origin, output);
        if (!pl) return;
        auto& data = LeviCreate::getInstance().states().get(pl->getRealName());
        if (data.undo.empty()) {
            output.error("Nothing to undo.");
            return;
        }
        UndoEntry e = std::move(data.undo.back());
        data.undo.pop_back();
        PlaceResult res;
        std::string err;
        if (!ops::pasteModel(*pl, e.snapshot, e.minPos, true, res, err)) {
            output.error(err);
            return;
        }
        output.success("Undo: restored " + formatVolume(res.placed) + " blocks.");
    });

    // ---- info / count ---------------------------------------------------------
    cmd.overload().text("info").execute([](CommandOrigin const& origin, CommandOutput& output, ll::command::EmptyParam const&) {
        auto* pl = playerOf(origin, output);
        if (!pl) return;
        auto& data = LeviCreate::getInstance().states().get(pl->getRealName());
        std::string msg;
        if (data.sel.complete()) {
            int3 mn = data.sel.minPos();
            int3 mx = data.sel.maxPos();
            msg += "Selection: (" + std::to_string(mn.x) + ", " + std::to_string(mn.y) + ", " + std::to_string(mn.z)
                 + ") → (" + std::to_string(mx.x) + ", " + std::to_string(mx.y) + ", " + std::to_string(mx.z)
                 + ") = " + formatVolume(data.sel.volumeBlocks()) + " blocks\n";
        } else {
            msg += "Selection: incomplete (use the wand or /vc pos1, /vc pos2)\n";
        }
        if (data.clip && data.clip->valid()) {
            msg += "Clipboard: " + data.clip->sourceName + " — "
                 + std::to_string(data.clip->model.sx) + "×" + std::to_string(data.clip->model.sy) + "×"
                 + std::to_string(data.clip->model.sz) + ", " + formatVolume(data.clip->model.countNonAir())
                 + " blocks, palette " + std::to_string(data.clip->model.palette.size()) + "\n";
        } else {
            msg += "Clipboard: empty\n";
        }
        msg += "Undo stack: " + std::to_string(data.undo.size()) + " entries";
        output.success(msg);
    });
    cmd.overload().text("count").execute([](CommandOrigin const& origin, CommandOutput& output, ll::command::EmptyParam const&) {
        auto* pl = playerOf(origin, output);
        if (!pl) return;
        int3 mn, mx;
        if (!requireSelectionOrErr(*pl, output, mn, mx)) return;
        std::unordered_map<std::string, std::uint64_t> counts;
        std::string                                    err;
        if (!WorldOps::countRegion(pl->getDimensionBlockSource(), mn, mx, counts, err)) {
            output.error(err);
            return;
        }
        std::vector<std::pair<std::string, std::uint64_t>> sorted(counts.begin(), counts.end());
        std::sort(sorted.begin(), sorted.end(), [](auto const& a, auto const& b) {
            return a.second > b.second;
        });
        std::string msg = "Selection contains " + std::to_string(sorted.size()) + " block types:";
        std::size_t shown = std::min<std::size_t>(sorted.size(), 10);
        for (std::size_t i = 0; i < shown; ++i) {
            msg += "\n§f" + sorted[i].first + "§r × " + formatVolume(sorted[i].second);
        }
        if (sorted.size() > shown) msg += "\n… and " + std::to_string(sorted.size() - shown) + " more";
        output.success(msg);
    });

    // ---- model library ---------------------------------------------------------
    cmd.overload<SaveParams>()
        .text("save")
        .required("name")
        .execute([](CommandOrigin const& origin, CommandOutput& output, SaveParams const& params) {
            auto* pl = playerOf(origin, output);
            if (!pl) return;
            if (!validModelName(params.name)) {
                output.error("Invalid name (1..32 chars; letters, digits, _ or -).");
                return;
            }
            int3 mn, mx;
            std::string err;
            VoxelModel  m;
            auto*       clip = ops::clipboardOf(*pl);
            if (clip) {
                m = clip->model;
            } else if (requireSelectionOrErr(*pl, output, mn, mx)) {
                auto cap = WorldOps::captureRegion(pl->getDimensionBlockSource(), mn, mx, err);
                if (!cap) {
                    output.error(err);
                    return;
                }
                m = std::move(*cap);
            } else {
                return;
            }
            auto file = LeviCreate::getInstance().modelFile(params.name);
            std::error_code ec;
            std::filesystem::create_directories(file.parent_path(), ec);
            std::ofstream out(file, std::ios::binary | std::ios::trunc);
            if (!out) {
                output.error("Cannot write model file: " + file.string());
                return;
            }
            std::string json = m.toJson();
            out.write(json.data(), static_cast<std::streamsize>(json.size()));
            out.close();
            output.success(
                "Saved model §o" + params.name + "§r (" + formatVolume(m.countNonAir()) + " blocks). "
                "Load with /vc load " + params.name + "."
            );
        });
    cmd.overload<LoadParams>()
        .text("load")
        .required("name")
        .optional("paste")
        .execute([](CommandOrigin const& origin, CommandOutput& output, LoadParams const& params) {
            auto* pl = playerOf(origin, output);
            if (!pl) return;
            if (!validModelName(params.name)) {
                output.error("Invalid model name.");
                return;
            }
            auto file = LeviCreate::getInstance().modelFile(params.name);
            std::ifstream in(file, std::ios::binary);
            if (!in) {
                output.error("Model not found: " + params.name);
                return;
            }
            std::ostringstream ss;
            ss << in.rdbuf();
            auto m = VoxelModel::fromJson(ss.str());
            if (!m) {
                output.error("Failed to parse model " + params.name + ".");
                return;
            }
            Clipboard clip;
            clip.model      = std::move(*m);
            clip.sourceName = params.name;
            auto dims       = std::to_string(clip.model.sx) + "×" + std::to_string(clip.model.sy) + "×"
                            + std::to_string(clip.model.sz);
            auto& data = LeviCreate::getInstance().states().get(pl->getRealName());
            data.clip  = std::move(clip);
            output.success(
                "Loaded §o" + params.name + "§r (" + dims + ", " + formatVolume(data.clip->model.countNonAir())
                + " blocks)."
            );
            if (params.paste) {
                VoxelModel const& mdl = data.clip->model;
                int3              anc = ops::feetAnchor(*pl, mdl.sx, mdl.sy, mdl.sz);
                PlaceResult       res;
                std::string       err;
                if (!ops::pasteModel(*pl, mdl, anc, false, res, err)) {
                    output.error(err);
                    return;
                }
                reportPlace(output, "Pasted " + params.name, res, &anc);
            }
        });
    cmd.overload<DeleteParams>()
        .text("delete")
        .required("name")
        .execute([](CommandOrigin const& origin, CommandOutput& output, DeleteParams const& params) {
            if (!validModelName(params.name)) {
                output.error("Invalid model name.");
                return;
            }
            auto file = LeviCreate::getInstance().modelFile(params.name);
            std::error_code ec;
            if (!std::filesystem::remove(file, ec) || ec) {
                output.error("Model not found: " + params.name);
                return;
            }
            output.success("Deleted model §o" + params.name + "§r.");
        });
    cmd.overload().text("list").execute([](CommandOrigin const&, CommandOutput& output, ll::command::EmptyParam const&) {
        auto            dir = LeviCreate::getInstance().modelsDir();
        std::error_code ec;
        std::filesystem::create_directories(dir, ec);
        std::vector<std::string> names;
        for (auto const& entry : std::filesystem::directory_iterator(dir, ec)) {
            if (!entry.is_regular_file()) continue;
            if (entry.path().extension() != ".json") continue;
            names.push_back(entry.path().stem().string());
        }
        std::sort(names.begin(), names.end());
        if (names.empty()) {
            output.success("No saved models yet. Select something and use /vc save <name>.");
            return;
        }
        std::string msg = "Saved models (" + std::to_string(names.size()) + "):";
        for (auto const& n : names) msg += "\n§f" + n + "§r";
        output.success(msg);
    });
}

} // namespace lc::cmd
