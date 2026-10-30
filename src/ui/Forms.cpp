#include "ui/Forms.h"

#include "core/PlayerOps.h"
#include "mod/LeviCreate.h"

#include "ll/api/form/CustomForm.h"
#include "ll/api/form/ModalForm.h"
#include "ll/api/form/SimpleForm.h"

#include "mc/world/actor/player/Player.h"
#include "mc/world/level/BlockSource.h"
#include "mc/deps/core/math/Vec3.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>
#include <cctype>

namespace lc::forms {

namespace {

std::string posStr(int3 const& p) {
    return std::to_string(p.x) + ", " + std::to_string(p.y) + ", " + std::to_string(p.z);
}

bool validModelName(std::string const& n) {
    if (n.empty() || n.size() > 32) return false;
    for (char c : n) {
        if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '-')) return false;
    }
    return true;
}

// --- CustomForm result helpers -------------------------------------------

std::uint64_t const* getUint(ll::form::CustomFormResult const& res, std::string const& name) {
    if (!res) return nullptr;
    auto it = res->find(name);
    if (it == res->end()) return nullptr;
    return std::get_if<std::uint64_t>(&it->second);
}

double const* getDouble(ll::form::CustomFormResult const& res, std::string const& name) {
    if (!res) return nullptr;
    auto it = res->find(name);
    if (it == res->end()) return nullptr;
    return std::get_if<double>(&it->second);
}

std::string const* getString(ll::form::CustomFormResult const& res, std::string const& name) {
    if (!res) return nullptr;
    auto it = res->find(name);
    if (it == res->end()) return nullptr;
    return std::get_if<std::string>(&it->second);
}

bool getBool(ll::form::CustomFormResult const& res, std::string const& name) {
    if (auto* u = getUint(res, name)) return *u != 0;
    if (auto* d = getDouble(res, name)) return *d != 0.0;
    return false;
}

std::optional<double> getNumber(ll::form::CustomFormResult const& res, std::string const& name) {
    if (auto* d = getDouble(res, name)) return *d;
    if (auto* u = getUint(res, name)) return static_cast<double>(*u);
    return std::nullopt;
}

} // namespace

std::string const& helpText() {
    static const std::string kHelp =
        "§l§bLeviCreate§r §7— voxel building toolkit (Chisel & Bits + Axiom style, server-side)§r\n"
        "\n"
        "§eSelection§r\n"
        "§f/vc wand§r — wand help (item: §oiron axe§r)\n"
        "§f/vc pos1§r / §f/vc pos2§r — set corners at your feet\n"
        "§f/vc count§r — block statistics of the selection\n"
        "§f/vc info§r — selection & clipboard info\n"
        "\n"
        "§eClipboard§r\n"
        "§f/vc copy§r — copy the selection\n"
        "§f/vc cut§r — copy + clear the selection\n"
        "§f/vc paste §7[scale] [yaw] [pitch]§r — paste around you, e.g. §o/vc paste 0.5 90§r\n"
        "§f/vc move <dx> <dy> <dz>§r — move the selection\n"
        "\n"
        "§eTransform (in place)§r\n"
        "§f/vc rotate <degrees> §7[axis]§r — §oany degree§r, e.g. §o/vc rotate 45 y§r\n"
        "§f/vc scale <factor>§r — e.g. §o/vc scale 2§r or §o/vc scale 0.25§r\n"
        "§f/vc flip <x|y|z>§r — mirror the selection\n"
        "\n"
        "§eTiny builds (bits-style miniatures)§r\n"
        "§f/vc mini §7[factor]§r — build a 1/f miniature beside the original (2..16)\n"
        "§f/vc expand §7[factor]§r — rebuild a miniature x f full-size\n"
        "\n"
        "§eFill§r\n"
        "§f/vc fill <block>§r — fill selection, e.g. §o/vc fill glass§r\n"
        "§f/vc replace <from> <to>§r — e.g. §o/vc replace stone obsidian§r\n"
        "§f/vc clear§r — set the selection to air\n"
        "§f/vc undo§r — undo the last destructive operation (up to 8)\n"
        "\n"
        "§eModel library§r (saved in the mod data folder)\n"
        "§f/vc save <name>§r / §f/vc load <name> §7[paste]§r / §f/vc delete <name>§r / §f/vc list§r\n"
        "\n"
        "§eUI§r\n"
        "§f/vc menu§r (or §f/vc ui§r) — open the interactive form menu\n"
        "\n"
        "§7Model data uses a 1/16-precision voxel grid internally; minis are true scale replicas.\n"
        "§7Note: current LeviLamina releases run mods on Windows servers; the mod library is levi-create.dll.";
    return kHelp;
}

void openHelpMenu(Player& player) {
    ll::form::SimpleForm form("§l§bLeviCreate§r §7Help", helpText());
    form.appendButton("§lBack", [](Player& p) { openMainMenu(p); });
    form.sendTo(player);
}

void openMainMenu(Player& player) {
    ll::form::SimpleForm form(
        "§l§bLeviCreate§r",
        "§7Voxel building toolkit — build anything, any shape, any scale.\n§7Select with the wand (§oiron axe§r) "
        "or §f/vc pos1§r§7, §f/vc pos2§r§7."
    );
    form.appendButton("§l§2Selection", "textures/ui/icon_recipe_nature", "path", [](Player& p) {
        openSelectionMenu(p);
    });
    form.appendButton("§l§6Transform", "textures/blocks/anvil_side", "path", [](Player& p) {
        openTransformMenu(p);
    });
    form.appendButton("§l§9Clipboard && Paste", "textures/ui/icon_recipe_equipment", "path", [](Player& p) {
        openPasteMenu(p);
    });
    form.appendButton("§l§dMiniature / Bits", "textures/items/emerald", "path", [](Player& p) { openMiniMenu(p); });
    form.appendButton("§l§5Model Library", "textures/ui/icon_recipe_item", "path", [](Player& p) {
        openLibraryMenu(p);
    });
    form.appendButton("§l§7Help", "textures/ui/question_mark_icon", "path", [](Player& p) { openHelpMenu(p); });
    form.sendTo(player);
}

void openSelectionMenu(Player& player) {
    auto& lc    = LeviCreate::getInstance();
    auto& sel   = lc.states().get(player.getRealName()).sel;
    int3  feet  = ops::feetBlock(player);

    std::string content = "Wand: hold an §oiron axe§r.\n§aSneak + right-click§r = pos #1\n§bRight-click§r = pos #2\n\n";
    content += "Pos #1: " + std::string(sel.has1 ? posStr(sel.p1) : "§7not set§r") + "\n";
    content += "Pos #2: " + std::string(sel.has2 ? posStr(sel.p2) : "§7not set§r") + "\n";
    if (sel.complete()) {
        content += "Region: " + formatVolume(sel.volumeBlocks()) + " blocks\n";
        int3 mn = sel.minPos();
        int3 mx = sel.maxPos();
        content += "Bounds: " + posStr(mn) + " → " + posStr(mx) + "\n";
    }

    ll::form::SimpleForm form("§l§2Selection§r", content);
    form.appendButton("Set Pos #1 at my feet", [feet](Player& p) {
        auto& s = LeviCreate::getInstance().states().get(p.getRealName()).sel;
        s.p1   = feet;
        s.has1 = true;
        p.sendMessage("§a[LeviCreate]§r Pos #1 set: " + posStr(feet));
        openSelectionMenu(p);
    });
    form.appendButton("Set Pos #2 at my feet", [feet](Player& p) {
        auto& s = LeviCreate::getInstance().states().get(p.getRealName()).sel;
        s.p2   = feet;
        s.has2 = true;
        p.sendMessage("§b[LeviCreate]§r Pos #2 set: " + posStr(feet));
        openSelectionMenu(p);
    });
    form.appendButton("Swap corners", [](Player& p) {
        auto& s = LeviCreate::getInstance().states().get(p.getRealName()).sel;
        std::swap(s.p1, s.p2);
        openSelectionMenu(p);
    });
    form.appendButton("Reset selection", [](Player& p) {
        LeviCreate::getInstance().states().get(p.getRealName()).sel.reset();
        p.sendMessage("§7[LeviCreate]§r Selection cleared.");
        openSelectionMenu(p);
    });
    form.appendButton("§lBack", [](Player& p) { openMainMenu(p); });
    form.sendTo(player);
}

void openTransformMenu(Player& player) {
    ll::form::CustomForm form("§l§6Transform selection§r");
    form.appendDropdown("op", "Operation", {"Rotate", "Scale", "Flip"});
    form.appendInput("degrees", "Rotate degrees (any angle)", "e.g. 45 or 137.5");
    form.appendDropdown("axis", "Axis", {"X", "Y", "Z"});
    form.appendSlider("scale", "Scale factor (Scale op)", 0.125, 8.0, 0.125, 1.0);
    form.appendLabel(
        "§7Rotate: exact at 90/180/270, rasterized at other angles.\n"
        "§7Scale: 0.5 = half size, 2 = double size (nearest sampling).\n"
        "§7These are in-place transforms; the original area is cleared. §o/vc undo§r§7 restores."
    );
    form.sendTo(player, [](Player& p, ll::form::CustomFormResult const& res, FormCancelReason) {
        if (!res) return;
        auto const* op = getUint(res, "op");
        if (!op) return;

        int3 mn, mx;
        std::string err;
        if (!ops::requireSelection(p, mn, mx, err)) {
            p.sendMessage("§c[LeviCreate]§r " + err);
            return;
        }
        auto& bs   = p.getDimensionBlockSource();
        auto  src  = WorldOps::captureRegion(bs, mn, mx, err);
        if (!src) {
            p.sendMessage("§c[LeviCreate]§r " + err);
            return;
        }

        int axis = 1;
        if (auto const* a = getUint(res, "axis")) axis = static_cast<int>(*a);

        VoxelModel out;
        if (*op == 0) { // Rotate
            double deg = 0.0;
            if (auto v = getNumber(res, "degrees")) deg = *v;
            else if (auto const* s = getString(res, "degrees"); s && !s->empty()) {
                try {
                    deg = std::stod(*s);
                } catch (...) {
                    p.sendMessage("§c[LeviCreate]§r Invalid degrees: " + *s);
                    return;
                }
            }
            out = rotateModel(*src, axis, deg);
        } else if (*op == 1) { // Scale
            double f = 1.0;
            if (auto v = getNumber(res, "scale")) f = *v;
            out = scaleModel(*src, static_cast<float>(f));
        } else { // Flip
            out = flipModel(*src, axis);
        }
        if (out.empty()) {
            p.sendMessage("§c[LeviCreate]§r Transform produced an empty model (region too large?).");
            return;
        }

        ops::pushUndoSnapshot(p, mn, mx);
        std::uint64_t changed = 0;
        if (!ops::clearRegion(p, mn, mx, changed, err)) {
            p.sendMessage("§c[LeviCreate]§r " + err);
            return;
        }
        PlaceResult pr;
        if (!ops::pasteModel(p, out, mn, false, pr, err)) {
            p.sendMessage("§c[LeviCreate]§r " + err);
            return;
        }
        // Selection footprint may have changed; reset it.
        LeviCreate::getInstance().states().get(p.getRealName()).sel.reset();
        p.sendMessage(
            "§a[LeviCreate]§r Transform done: placed " + formatVolume(pr.placed) + " blocks"
            + (pr.failed ? (", failed " + formatVolume(pr.failed)) : "")
            + ". §7Selection reset — reselect to continue."
        );
    });
}

void openPasteMenu(Player& player) {
    Clipboard* clip = ops::clipboardOf(player);
    std::string content = clip ? ("Clipboard: §a" + clip->sourceName + "§r — "
        + formatVolume(clip->model.countNonAir()) + " blocks, " + std::to_string(clip->model.sx) + "×"
        + std::to_string(clip->model.sy) + "×" + std::to_string(clip->model.sz))
                              : "Clipboard: §7empty§r — copy something first (§o/vc copy§r§7).";

    ll::form::SimpleForm form("§l§9Clipboard && Paste§r", content);
    form.appendButton("Paste with options…", [](Player& p) {
        if (!ops::clipboardOf(p)) {
            p.sendMessage("§c[LeviCreate]§r Clipboard is empty. Use /vc copy first.");
            return;
        }
        ll::form::CustomForm f("§l§9Paste§r");
        f.appendSlider("scale", "Scale", 0.125, 8.0, 0.125, 1.0);
        f.appendSlider("yaw", "Yaw rotation (any angle)", 0.0, 345.0, 15.0, 0.0);
        f.appendDropdown("pitch", "Pitch", {"0", "90", "-90", "180"});
        f.appendToggle("includeAir", "Also paste air (carve out)", false);
        f.appendLabel("§7Pasted centered on you, at your feet level.");
        f.sendTo(p, [](Player& pl, ll::form::CustomFormResult const& res, FormCancelReason) {
            if (!res) return;
            Clipboard* cl = ops::clipboardOf(pl);
            if (!cl) return;
            ModelTransform t;
            if (auto v = getNumber(res, "scale")) t.scale = static_cast<float>(*v);
            if (auto v = getNumber(res, "yaw")) t.yawDegrees = static_cast<float>(*v);
            if (auto const* idx = getUint(res, "pitch")) {
                static const float pitches[] = {0.0f, 90.0f, -90.0f, 180.0f};
                t.pitchDegrees = pitches[std::min<std::size_t>(*idx, 3)];
            }
            VoxelModel m = transformModel(cl->model, t);
            if (m.empty()) {
                pl.sendMessage("§c[LeviCreate]§r Paste failed: model too large.");
                return;
            }
            int3 origin = ops::feetAnchor(pl, m.sx, m.sy, m.sz);
            PlaceResult pr;
            std::string err;
            bool includeAir = getBool(res, "includeAir");
            if (!ops::pasteModel(pl, m, origin, includeAir, pr, err)) {
                pl.sendMessage("§c[LeviCreate]§r " + err);
                return;
            }
            pl.sendMessage(
                "§a[LeviCreate]§r Pasted " + formatVolume(pr.placed) + " blocks"
                + (pr.airSkipped ? (", skipped " + formatVolume(pr.airSkipped) + " air") : "")
                + (pr.failed ? (", failed " + formatVolume(pr.failed)) : "") + " at " + posStr(origin) + "."
            );
        });
    });
    form.appendButton("Copy selection", [](Player& p) {
        int3 mn, mx;
        std::string err;
        if (!ops::requireSelection(p, mn, mx, err)) {
            p.sendMessage("§c[LeviCreate]§r " + err);
            return;
        }
        auto m = WorldOps::captureRegion(p.getDimensionBlockSource(), mn, mx, err);
        if (!m) {
            p.sendMessage("§c[LeviCreate]§r " + err);
            return;
        }
        Clipboard clip;
        clip.model      = std::move(*m);
        clip.sourceName = "selection";
        std::uint64_t n = clip.model.countNonAir();
        LeviCreate::getInstance().states().get(p.getRealName()).clip = std::move(clip);
        p.sendMessage("§a[LeviCreate]§r Copied " + formatVolume(n) + " blocks.");
    });
    form.appendButton("Cut selection", [](Player& p) {
        int3 mn, mx;
        std::string err;
        if (!ops::requireSelection(p, mn, mx, err)) {
            p.sendMessage("§c[LeviCreate]§r " + err);
            return;
        }
        auto m = WorldOps::captureRegion(p.getDimensionBlockSource(), mn, mx, err);
        if (!m) {
            p.sendMessage("§c[LeviCreate]§r " + err);
            return;
        }
        Clipboard clip;
        clip.model      = std::move(*m);
        clip.sourceName = "cut";
        LeviCreate::getInstance().states().get(p.getRealName()).clip = std::move(clip);
        std::uint64_t changed = 0;
        if (!ops::clearRegion(p, mn, mx, changed, err)) {
            p.sendMessage("§c[LeviCreate]§r " + err);
            return;
        }
        p.sendMessage("§a[LeviCreate]§r Cut " + formatVolume(changed) + " blocks to clipboard.");
    });
    form.appendButton("Clear clipboard", [](Player& p) {
        LeviCreate::getInstance().states().get(p.getRealName()).clip.reset();
        p.sendMessage("§7[LeviCreate]§r Clipboard cleared.");
    });
    form.appendButton("§lBack", [](Player& p) { openMainMenu(p); });
    form.sendTo(player);
}

void openMiniMenu(Player& player) {
    ll::form::SimpleForm form(
        "§l§dMiniature / Bits§r",
        "Build tiny replicas of anything — the Chisel & Bits style miniature workflow.\n\n"
        "§7§oMini§r§7 shrinks the selection by a factor and places the tiny copy beside it.\n"
        "§7§oExpand§r§7 takes a tiny build and rebuilds it full-size x factor.\n"
        "§7Both results are also stored in the clipboard for §o/vc paste§r§7."
    );
    form.appendButton("Mini x2", [](Player& p) { miniAction(p, 2, false); });
    form.appendButton("Mini x4", [](Player& p) { miniAction(p, 4, false); });
    form.appendButton("Mini x8", [](Player& p) { miniAction(p, 8, false); });
    form.appendButton("Mini x16 (1/16 bits scale)", [](Player& p) { miniAction(p, 16, false); });
    form.appendButton("Expand x2", [](Player& p) { miniAction(p, 2, true); });
    form.appendButton("Expand x4", [](Player& p) { miniAction(p, 4, true); });
    form.appendButton("Expand x8", [](Player& p) { miniAction(p, 8, true); });
    form.appendButton("§lBack", [](Player& p) { openMainMenu(p); });
    form.sendTo(player);
}

void openLibraryMenu(Player& player) {
    auto        dir = LeviCreate::getInstance().modelsDir();
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);

    std::string content = "Saved models:\n";
    std::vector<std::string> names;
    for (auto const& entry : std::filesystem::directory_iterator(dir, ec)) {
        if (!entry.is_regular_file()) continue;
        auto p = entry.path();
        if (p.extension() != ".json") continue;
        names.push_back(p.stem().string());
    }
    std::sort(names.begin(), names.end());
    if (names.empty()) {
        content += "§7(none yet — select something and use §oSave to library§r§7)";
    } else {
        for (auto const& n : names) content += "§f• " + n + "§r\n";
    }

    ll::form::SimpleForm form("§l§5Model Library§r", content);
    form.appendButton("Save selection to library…", [](Player& p) {
        ll::form::CustomForm f("§l§5Save model§r");
        f.appendInput("name", "Model name", "letters, digits, _ or -");
        f.sendTo(p, [](Player& pl, ll::form::CustomFormResult const& res, FormCancelReason) {
            if (!res) return;
            auto const* name = getString(res, "name");
            if (!name || !validModelName(*name)) {
                pl.sendMessage("§c[LeviCreate]§r Invalid name (1..32 chars; letters, digits, _ or -).");
                return;
            }
            int3 mn, mx;
            std::string err;
            if (!ops::requireSelection(pl, mn, mx, err)) {
                pl.sendMessage("§c[LeviCreate]§r " + err);
                return;
            }
            auto m = WorldOps::captureRegion(pl.getDimensionBlockSource(), mn, mx, err);
            if (!m) {
                pl.sendMessage("§c[LeviCreate]§r " + err);
                return;
            }
            auto file = LeviCreate::getInstance().modelFile(*name);
            std::ofstream out(file, std::ios::binary | std::ios::trunc);
            if (!out) {
                pl.sendMessage("§c[LeviCreate]§r Cannot write model file.");
                return;
            }
            std::string json = m->toJson();
            out.write(json.data(), static_cast<std::streamsize>(json.size()));
            out.close();
            pl.sendMessage("§a[LeviCreate]§r Saved model §o" + *name + "§r (" + formatVolume(m->countNonAir())
                           + " blocks). Load with §o/vc load " + *name + "§r.");
        });
    });
    for (auto const& n : names) {
        std::string name = n;
        form.appendButton("§f" + name, [name](Player& p) {
            ll::form::SimpleForm f("§l§5" + name, "What do you want to do with §o" + name + "§r?");
            f.appendButton("Load to clipboard", [name](Player& pl) { loadModel(pl, name, false); });
            f.appendButton("Load && paste here", [name](Player& pl) { loadModel(pl, name, true); });
            f.appendButton("§cDelete model", [name](Player& pl) {
                ll::form::ModalForm confirm(
                    "§cDelete " + name + "?", "This permanently deletes the saved model.", "§cDelete", "Cancel"
                );
                confirm.sendTo(pl, [name](Player& p2, ll::form::ModalFormResult r, FormCancelReason) {
                    if (r && *r == ll::form::ModalFormSelectedButton::Upper) {
                        std::error_code ec2;
                        auto file = LeviCreate::getInstance().modelFile(name);
                        if (std::filesystem::remove(file, ec2)) {
                            p2.sendMessage("§a[LeviCreate]§r Deleted §o" + name + "§r.");
                        } else {
                            p2.sendMessage("§c[LeviCreate]§r Could not delete §o" + name + "§r.");
                        }
                    }
                    openLibraryMenu(p2);
                });
            });
            f.appendButton("§lBack", [](Player& pl) { openLibraryMenu(pl); });
            f.sendTo(p);
        });
    }
    form.appendButton("§lBack", [](Player& p) { openMainMenu(p); });
    form.sendTo(player);
}

void loadModel(Player& player, std::string const& name, bool pasteNow) {
    if (!validModelName(name)) {
        player.sendMessage("§c[LeviCreate]§r Invalid model name.");
        return;
    }
    auto file = LeviCreate::getInstance().modelFile(name);
    std::ifstream in(file, std::ios::binary);
    if (!in) {
        player.sendMessage("§c[LeviCreate]§r Model not found: §o" + name + "§r.");
        return;
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    auto m = VoxelModel::fromJson(ss.str());
    if (!m) {
        player.sendMessage("§c[LeviCreate]§r Failed to parse model §o" + name + "§r.");
        return;
    }
    Clipboard clip;
    clip.model      = std::move(*m);
    clip.sourceName = name;
    auto dims = std::to_string(clip.model.sx) + "×" + std::to_string(clip.model.sy) + "×"
              + std::to_string(clip.model.sz);
    auto& data = LeviCreate::getInstance().states().get(player.getRealName());
    data.clip  = std::move(clip);
    player.sendMessage(
        "§a[LeviCreate]§r Loaded §o" + name + "§r (" + dims + ", " + formatVolume(data.clip->model.countNonAir())
        + " blocks)."
    );
    if (pasteNow) {
        VoxelModel const& mdl = data.clip->model;
        int3              origin = ops::feetAnchor(player, mdl.sx, mdl.sy, mdl.sz);
        PlaceResult       pr;
        std::string       err;
        if (!ops::pasteModel(player, mdl, origin, false, pr, err)) {
            player.sendMessage("§c[LeviCreate]§r " + err);
            return;
        }
        player.sendMessage("§a[LeviCreate]§r Pasted " + formatVolume(pr.placed) + " blocks at " + posStr(origin) + ".");
    }
}

void miniAction(Player& player, int factor, bool expand) {
    std::string msg, err;
    if (ops::miniOrExpand(player, factor, expand, msg, err)) {
        player.sendMessage("§a[LeviCreate]§r " + msg);
    } else {
        player.sendMessage("§c[LeviCreate]§r " + err);
    }
}

} // namespace lc::forms
