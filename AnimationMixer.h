#ifndef ANIMATION_MIXER_H
#define ANIMATION_MIXER_H

#include <vector>
#include <glm/glm.hpp>

class AnimationMixer {
public:
    static std::vector<glm::mat4> blend(const std::vector<glm::mat4>& a, const std::vector<glm::mat4>& b, float alpha) {
        if (alpha <= 0.0f) return a;
        if (alpha >= 1.0f) return b;
        
        std::vector<glm::mat4> result(a.size());
        for (size_t i = 0; i < a.size(); i++) {
            // Simplified matrix LERP (Should decompose to translation/rotation/scale for true results)
            result[i] = a[i] * (1.0f - alpha) + b[i] * alpha;
        }
        return result;
    }
};

#endif
