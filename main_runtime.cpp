#include <iostream>
#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#undef STB_IMAGE_IMPLEMENTATION
#include "FBXStateMachine.h"
#include "CharacterPhysics.h"
#include "SkinnedRenderer.h"
#include "AssetBaking.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/html5.h>
#include <GLFW/glfw3.h>
#else
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#endif

FBXStateMachine stateMachine;
CharacterPhysics physics;
SkinnedRenderer renderer;
BakedAsset asset;
GLFWwindow* window = nullptr;

void update() {
    static float lastTime = (float)glfwGetTime();
    float currentTime = (float)glfwGetTime();
    float dt = currentTime - lastTime;
    lastTime = currentTime;

    stateMachine.update(dt);
    physics.update(stateMachine.getBones(), glm::mat4(1.0f));
    
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    glViewport(0, 0, width, height);
    glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    renderer.render(stateMachine.getFinalBoneMatrices());

#ifndef __EMSCRIPTEN__
    glfwSwapBuffers(window);
    glfwPollEvents();
#endif
}

extern "C" {
    void setState(int state) {
        stateMachine.setState(static_cast<State>(state));
    }
    
    bool shoot(float x, float y, float z, float dx, float dy, float dz) {
        HitResult hit;
        if (physics.raycast(glm::vec3(x,y,z), glm::vec3(dx,dy,dz), 100.0f, hit)) {
            std::cout << "Hit bone: " << hit.boneName << " damage: " << hit.damage << std::endl;
            return true;
        }
        return false;
    }
}

int main() {
    if (!glfwInit()) return -1;
    
#ifdef __EMSCRIPTEN__
    window = glfwCreateWindow(1280, 720, "Character Runtime Player", NULL, NULL);
#else
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    window = glfwCreateWindow(1280, 720, "Character Runtime Player", NULL, NULL);
    if (!window) {
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    if (glewInit() != GLEW_OK) return -1;
#endif

    glEnable(GL_DEPTH_TEST);

    const char* asset_path = "assets/soldier.asset.json";
#ifdef __EMSCRIPTEN__
    if (FILE *file = fopen(asset_path, "r")) {
        fclose(file);
    } else {
        std::cerr << "Warning: " << asset_path << " not found, trying local path." << std::endl;
        asset_path = "soldier.asset.json";
    }
#endif

    asset = AssetBaking::load(asset_path);
    std::cout << "Loaded asset: " << asset.skeleton << " from " << asset_path << std::endl;
    
    std::string fbx_path = asset.skeleton;
#ifdef __EMSCRIPTEN__
    if (fbx_path.find("assets/") != 0) {
        fbx_path = "assets/" + fbx_path;
    }
#endif
    stateMachine.loadFBX(fbx_path);
    std::cout << "Loaded FBX: " << stateMachine.getMeshes().size() << " meshes, " 
              << stateMachine.getBones().size() << " bones." << std::endl;

    for (auto const& [name, index] : asset.states) {
        if (name == "IDLE") stateMachine.setAnimationMapping(IDLE, index);
        else if (name == "RUN") stateMachine.setAnimationMapping(RUN, index);
        else if (name == "JUMP") stateMachine.setAnimationMapping(JUMP, index);
    }
    
    std::vector<Capsule> capsules;
    for(auto& c : asset.colliders) {
        capsules.push_back({c.bone, c.radius, c.height, c.damage});
    }
    physics.setupColliders(capsules);
    
    renderer.init(stateMachine.getMeshes());
    if (stateMachine.getMeshes().empty()) {
        std::cerr << "Warning: No meshes found in the FBX file!" << std::endl;
    }

#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop(update, 0, 1);
#else
    while (!glfwWindowShouldClose(window)) { update(); }
    glfwTerminate();
#endif
    return 0;
}
