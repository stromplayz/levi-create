#include "mod/Config.h"

namespace levicreate {

Tool toolFromIndex(int index, Tool fallback) {
    if (index < 0 || index > static_cast<int>(Tool::Move)) {
        return fallback;
    }
    return static_cast<Tool>(index);
}

int toolToIndex(Tool tool) { return static_cast<int>(tool); }

const char *toolLabel(Tool tool) {
    switch (tool) {
    case Tool::Build:
        return "Build";
    case Tool::Erase:
        return "Erase";
    case Tool::Paint:
        return "Paint";
    case Tool::Box:
        return "Box";
    case Tool::Sphere:
        return "Sphere";
    case Tool::Move:
        return "Move";
    }
    return "Build";
}

} // namespace levicreate
