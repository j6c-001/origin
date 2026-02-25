#ifndef CHARACTER_PHYSICS_H
#define CHARACTER_PHYSICS_H

#include <vector>
#include <string>
#include <glm/glm.hpp>
#include "FBXStateMachine.h"

struct Capsule {
    std::string boneName;
    float radius;
    float height;
    float damageMultiplier;
    glm::vec3 start; // world space
    glm::vec3 end;   // world space
};

struct HitResult {
    std::string boneName;
    glm::vec3 position;
    glm::vec3 normal;
    float damage;
};

class CharacterPhysics {
public:
    void setupColliders(const std::vector<Capsule>& config) {
        colliders = config;
    }

    void update(const std::vector<Bone>& bones, const glm::mat4& modelTransform) {
        for (auto& cap : colliders) {
            // Find bone transform
            for (const auto& bone : bones) {
                if (bone.name == cap.boneName) {
                    glm::mat4 boneWorld = modelTransform * bone.finalTransform;
                    // For simplicity, capsule centers follow bone
                    cap.start = glm::vec3(boneWorld * glm::vec4(0, 0, 0, 1));
                    cap.end = glm::vec3(boneWorld * glm::vec4(0, cap.height, 0, 1));
                    break;
                }
            }
        }
    }

    bool raycast(glm::vec3 rayOrigin, glm::vec3 rayDir, float maxDist, HitResult& hit) {
        float minDist = maxDist;
        bool found = false;

        for (const auto& cap : colliders) {
            float t;
            if (rayCapsuleIntersection(rayOrigin, rayDir, cap.start, cap.end, cap.radius, t)) {
                if (t < minDist) {
                    minDist = t;
                    hit.boneName = cap.boneName;
                    hit.position = rayOrigin + rayDir * t;
                    hit.normal = glm::normalize(hit.position - (cap.start + cap.end) * 0.5f); // Rough normal
                    hit.damage = cap.damageMultiplier;
                    found = true;
                }
            }
        }
        return found;
    }

private:
    bool rayCapsuleIntersection(glm::vec3 ro, glm::vec3 rd, glm::vec3 a, glm::vec3 b, float r, float& t) {
        // Implementation of Ray-Capsule intersection
        // For simplicity, treat as sphere for now or implement full check
        glm::vec3 ba = b - a;
        glm::vec3 oa = ro - a;
        float baba = glm::dot(ba, ba);
        float bard = glm::dot(ba, rd);
        float baoa = glm::dot(ba, oa);
        float rdoa = glm::dot(rd, oa);
        float oaoa = glm::dot(oa, oa);

        float a_coeff = baba - bard * bard;
        float b_coeff = baba * rdoa - baoa * bard;
        float c_coeff = baba * oaoa - baoa * baoa - r * r * baba;
        float h = b_coeff * b_coeff - a_coeff * c_coeff;
        if (h >= 0.0f) {
            t = (-b_coeff - sqrt(h)) / a_coeff;
            float y = baoa + t * bard;
            if (y > 0.0 && y < baba) return true;
            // Cap intersections
            glm::vec3 oc = (y <= 0.0) ? oa : ro - b;
            b_coeff = glm::dot(rd, oc);
            c_coeff = glm::dot(oc, oc) - r * r;
            h = b_coeff * b_coeff - c_coeff;
            if (h >= 0.0) {
                t = -b_coeff - sqrt(h);
                return true;
            }
        }
        return false;
    }

    std::vector<Capsule> colliders;
};

#endif
