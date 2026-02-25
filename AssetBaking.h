#ifndef ASSET_BAKING_H
#define ASSET_BAKING_H

#include <string>
#include <vector>
#include <map>
#include <fstream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

struct BakedAsset {
    std::string skeleton;
    std::map<std::string, int> states;
    std::vector<std::string> textures;
    struct PhysicsConfig {
        std::string bone;
        float radius;
        float height;
        float damage;
    };
    std::vector<PhysicsConfig> colliders;
};

class AssetBaking {
public:
    static void save(const std::string& path, const BakedAsset& asset) {
        json j;
        j["skeleton"] = asset.skeleton;
        j["states"] = asset.states;
        j["textures"] = asset.textures;
        for (const auto& c : asset.colliders) {
            j["physics"]["colliders"].push_back({
                {"bone", c.bone}, {"radius", c.radius}, {"height", c.height}, {"damage", c.damage}
            });
        }
        std::ofstream file(path);
        file << j.dump(4);
    }

    static BakedAsset load(const std::string& path) {
        std::ifstream file(path);
        json j;
        file >> j;
        BakedAsset asset;
        asset.skeleton = j["skeleton"];
        asset.states = j["states"].get<std::map<std::string, int>>();
        asset.textures = j["textures"].get<std::vector<std::string>>();
        for (auto& item : j["physics"]["colliders"]) {
            asset.colliders.push_back({
                item["bone"], item["radius"], item["height"], item["damage"]
            });
        }
        return asset;
    }
};

#endif
