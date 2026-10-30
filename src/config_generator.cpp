#include <filesystem>
#include <iostream>

#include "mod/Config.h"

#include <pl/Config.hpp>

// Host-side generator: writes config/config.json and config/config.schema.json
// into the mod package directory before packaging (.levipack).
int main(int argc, char **argv) {
    if (argc != 2) {
        std::cerr << "usage: levi_config_generator <mod-package-dir>\n";
        return 2;
    }

    const std::filesystem::path packageDir(argv[1]);
    const auto configDir = packageDir / "config";
    pl::config::ConfigFile<levicreate::ModConfig> config(
        levicreate::ModConfig{}, configDir / "config.json",
        configDir / "config.schema.json");

    if (!config.save()) {
        std::cerr << "failed to write " << config.configPath() << '\n';
        return 1;
    }
    if (!config.writeSchema()) {
        std::cerr << "failed to write " << config.schemaPath() << '\n';
        return 1;
    }

    std::cout << "generated " << config.configPath() << '\n';
    std::cout << "generated " << config.schemaPath() << '\n';
    return 0;
}
