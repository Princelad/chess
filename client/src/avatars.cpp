#include "avatars.h"

#include <SFML/Graphics/Texture.hpp>

#include <algorithm>
#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

namespace chess::client::avatars {

AvatarPack loadAvatarPack(const std::string& dirName)
{
    AvatarPack pack;
    for (const std::string& root : assetRoots()) {
        const std::filesystem::path dir =
            std::filesystem::path(root) / dirName;
        if (!std::filesystem::exists(dir))
            continue; // next root; first hit wins once we find the dir

        // Sorted by filename so the pack index mapping is stable across
        // machines (ASCII byte order like the codebase's other sorts).
        std::vector<std::string> files;
        std::error_code ec;
        for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
            if (!entry.is_regular_file(ec)) continue;
            std::string ext = entry.path().extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(),
                [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            if (ext != ".png") continue; // README.md and friends live beside
            files.push_back(entry.path().string());
        }
        (void)ec;
        std::sort(files.begin(), files.end());

        for (const std::string& file : files) {
            sf::Texture tile;
            if (tile.loadFromFile(file)) {
                tile.setSmooth(true);
                pack.tiles.push_back(std::move(tile));
            }
        }
        // One dir wins even if its PNGs partially fail to load — a broken
        // file just isn't in the pool, no silent fall-through to another
        // tree (same reasoning as the piece loader's all-or-nothing root).
        break;
    }
    return pack;
}

} // namespace chess::client::avatars
