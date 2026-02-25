#ifndef CHARACTER_EDITOR_H
#define CHARACTER_EDITOR_H

#include <imgui.h>
#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <cstring>
#include <cstddef>
#include <algorithm>
#include <cctype>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <GL/glew.h>
#include "stb_image.h"
#include "FBXStateMachine.h"
#include "AssetBaking.h"

class CharacterEditor {
public:
    CharacterEditor() {
        // Compile simple line shader
        GLuint vShader = glCreateShader(GL_VERTEX_SHADER);
        const char* vSrc = R"(#version 330 core
            layout(location = 0) in vec3 aPos;
            uniform mat4 uVP;
            void main() { gl_Position = uVP * vec4(aPos, 1.0); }
        )";
        glShaderSource(vShader, 1, &vSrc, nullptr);
        glCompileShader(vShader);

        GLuint fShader = glCreateShader(GL_FRAGMENT_SHADER);
        const char* fSrc = R"(#version 330 core
            out vec4 FragColor;
            uniform vec3 uColor;
            void main() { FragColor = vec4(uColor, 1.0); }
        )";
        glShaderSource(fShader, 1, &fSrc, nullptr);
        glCompileShader(fShader);

        lineShader = glCreateProgram();
        glAttachShader(lineShader, vShader);
        glAttachShader(lineShader, fShader);
        glLinkProgram(lineShader);
        glDeleteShader(vShader);
        glDeleteShader(fShader);

        glGenVertexArrays(1, &lineVAO);
        glGenBuffers(1, &lineVBO);

        // Compile skinned mesh shader
        vShader = glCreateShader(GL_VERTEX_SHADER);
        const char* svSrc = R"(#version 330 core
            layout(location = 0) in vec3 aPos;
            layout(location = 1) in vec2 aUV;
            layout(location = 2) in ivec4 aBoneIds;
            layout(location = 3) in vec4 aWeights;
            uniform mat4 uVP;
            uniform mat4 uBones[256];
            out vec2 vUV;
            void main() {
                vec4 pos = vec4(0.0);
                float totalWeight = 0.0;
                for(int i=0; i<4; i++) {
                    if(aBoneIds[i] >= 0 && aBoneIds[i] < 256) {
                        pos += aWeights[i] * (uBones[aBoneIds[i]] * vec4(aPos, 1.0));
                        totalWeight += aWeights[i];
                    }
                }
                if (totalWeight < 0.01) pos = vec4(aPos, 1.0);
                gl_Position = uVP * vec4(pos.xyz, 1.0);
                vUV = aUV;
            }
        )";
        glShaderSource(vShader, 1, &svSrc, nullptr);
        glCompileShader(vShader);

        fShader = glCreateShader(GL_FRAGMENT_SHADER);
        const char* sfSrc = R"(#version 330 core
            in vec2 vUV;
            out vec4 FragColor;
            uniform sampler2D uTexture;
            uniform bool uHasTexture;
            void main() { 
                if (uHasTexture)
                    FragColor = texture(uTexture, vUV);
                else
                    FragColor = vec4(vUV, 0.5, 1.0); 
            }
        )";
        glShaderSource(fShader, 1, &sfSrc, nullptr);
        glCompileShader(fShader);

        skinnedShader = glCreateProgram();
        glAttachShader(skinnedShader, vShader);
        glAttachShader(skinnedShader, fShader);
        glLinkProgram(skinnedShader);
        glDeleteShader(vShader);
        glDeleteShader(fShader);
    }

    void setupMeshGL() {
        for(auto& m : meshGLs) {
            glDeleteVertexArrays(1, &m.vao);
            glDeleteBuffers(1, &m.vbo);
            glDeleteBuffers(1, &m.ebo);
        }
        meshGLs.clear();
        
        // Clear cached textures
        for (auto& pair : textureCache) {
            glDeleteTextures(1, &pair.second);
        }
        textureCache.clear();

        currentAsset.textures.clear();
        const auto& meshes = sm.getMeshes();
        for (const auto& mData : meshes) {
            MeshGL m;
            m.textureID = 0;
            if (!mData.texturePath.empty()) {
                if (textureCache.find(mData.texturePath) == textureCache.end()) {
                    GLuint texID = loadTexture(mData.texturePath);
                    if (texID != 0) {
                        textureCache[mData.texturePath] = texID;
                        currentAsset.textures.push_back(mData.texturePath);
                    }
                }
                if (textureCache.count(mData.texturePath)) {
                    m.textureID = textureCache[mData.texturePath];
                }
            }
            
            glGenVertexArrays(1, &m.vao);
            glGenBuffers(1, &m.vbo);
            glGenBuffers(1, &m.ebo);

            glBindVertexArray(m.vao);
            glBindBuffer(GL_ARRAY_BUFFER, m.vbo);
            glBufferData(GL_ARRAY_BUFFER, mData.vertices.size() * sizeof(FBXStateMachine::Vertex), mData.vertices.data(), GL_STATIC_DRAW);

            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m.ebo);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, mData.indices.size() * sizeof(unsigned int), mData.indices.data(), GL_STATIC_DRAW);

            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(FBXStateMachine::Vertex), (void*)offsetof(FBXStateMachine::Vertex, position));
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(FBXStateMachine::Vertex), (void*)offsetof(FBXStateMachine::Vertex, uv));
            glEnableVertexAttribArray(1);
            glVertexAttribIPointer(2, 4, GL_INT, sizeof(FBXStateMachine::Vertex), (void*)offsetof(FBXStateMachine::Vertex, boneIds));
            glEnableVertexAttribArray(2);
            glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(FBXStateMachine::Vertex), (void*)offsetof(FBXStateMachine::Vertex, weights));
            glEnableVertexAttribArray(3);

            m.count = (GLsizei)mData.indices.size();
            meshGLs.push_back(m);
        }
        glBindVertexArray(0);
    }

    void update(float dt) {
        sm.update(dt);
    }

    void render(int width, int height) {
        if (sm.getBones().empty()) return;

        glm::vec3 eye;
        eye.x = cameraDist * cos(glm::radians(cameraPitch)) * sin(glm::radians(cameraYaw));
        eye.y = cameraDist * sin(glm::radians(cameraPitch)) + cameraHeight;
        eye.z = cameraDist * cos(glm::radians(cameraPitch)) * cos(glm::radians(cameraYaw));

        glm::mat4 view = glm::lookAt(eye, glm::vec3(0, cameraHeight, 0), glm::vec3(0, 1, 0));
        glm::mat4 projection = glm::perspective(glm::radians(45.0f), (float)width / (float)height, 0.1f, 10000.0f);
        glm::mat4 vp = projection * view;

        if (showSkinnedMesh && !meshGLs.empty()) {
            glUseProgram(skinnedShader);
            glUniformMatrix4fv(glGetUniformLocation(skinnedShader, "uVP"), 1, GL_FALSE, glm::value_ptr(vp));
            
            auto finalMatrices = sm.getFinalBoneMatrices();
            if (!finalMatrices.empty()) {
                glUniformMatrix4fv(glGetUniformLocation(skinnedShader, "uBones"), (GLsizei)finalMatrices.size(), GL_FALSE, glm::value_ptr(finalMatrices[0]));
            }

            for (const auto& m : meshGLs) {
                if (m.textureID != 0) {
                    glActiveTexture(GL_TEXTURE0);
                    glBindTexture(GL_TEXTURE_2D, m.textureID);
                    glUniform1i(glGetUniformLocation(skinnedShader, "uTexture"), 0);
                    glUniform1i(glGetUniformLocation(skinnedShader, "uHasTexture"), 1);
                } else {
                    glUniform1i(glGetUniformLocation(skinnedShader, "uHasTexture"), 0);
                }
                glBindVertexArray(m.vao);
                glDrawElements(GL_TRIANGLES, m.count, GL_UNSIGNED_INT, 0);
            }
            glBindVertexArray(0);
        }

        glUseProgram(lineShader);
        glUniformMatrix4fv(glGetUniformLocation(lineShader, "uVP"), 1, GL_FALSE, glm::value_ptr(vp));
        glUniform3f(glGetUniformLocation(lineShader, "uColor"), 1.0f, 1.0f, 0.0f);

        std::vector<float> lineVertices;
        const auto& bones = sm.getBones();
        for (const auto& bone : bones) {
            if (bone.parentIndex != -1) {
                glm::vec3 pos = glm::vec3(bone.worldTransform[3]);
                glm::vec3 parentPos = glm::vec3(bones[bone.parentIndex].worldTransform[3]);
                lineVertices.push_back(parentPos.x); lineVertices.push_back(parentPos.y); lineVertices.push_back(parentPos.z);
                lineVertices.push_back(pos.x); lineVertices.push_back(pos.y); lineVertices.push_back(pos.z);
            }
        }

        if (!lineVertices.empty()) {
            glBindVertexArray(lineVAO);
            glBindBuffer(GL_ARRAY_BUFFER, lineVBO);
            glBufferData(GL_ARRAY_BUFFER, lineVertices.size() * sizeof(float), lineVertices.data(), GL_STREAM_DRAW);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
            glEnableVertexAttribArray(0);
            glDrawArrays(GL_LINES, 0, (GLsizei)(lineVertices.size() / 3));
        }

        if (showBoneLabels) {
            for (const auto& bone : bones) {
                glm::vec4 worldPos = bone.worldTransform[3];
                glm::vec4 clipPos = vp * worldPos;
                if (clipPos.w > 0) {
                    glm::vec3 ndcPos = glm::vec3(clipPos) / clipPos.w;
                    float screenX = (ndcPos.x + 1.0f) * 0.5f * width;
                    float screenY = (1.0f - ndcPos.y) * 0.5f * height;
                    ImGui::GetBackgroundDrawList()->AddText(ImVec2(screenX, screenY), IM_COL32(255, 255, 255, 255), bone.name.c_str());
                }
            }
        }
    }

    void ui() {
        // Mouse controls for camera
        if (!ImGui::GetIO().WantCaptureMouse) {
            if (ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
                ImVec2 delta = ImGui::GetIO().MouseDelta;
                cameraYaw -= delta.x * 0.5f;
                cameraPitch += delta.y * 0.5f;
                cameraPitch = glm::clamp(cameraPitch, -89.0f, 89.0f);
            }
            cameraDist -= ImGui::GetIO().MouseWheel * 10.0f;
            cameraDist = glm::max(cameraDist, 1.0f);
        }

        ImGui::Begin("Soldier Editor");
        
        static char fbxPath[256] = "soldier.fbx";
        ImGui::InputText("FBX Path", fbxPath, 256);
        if (ImGui::Button("Load FBX")) {
            currentAsset.skeleton = fbxPath;
            sm.loadFBX(fbxPath);
            setupMeshGL();
        }

        auto meta = sm.getMetadata();
        if (meta.numMeshes > 0 || meta.numAnimations > 0) {
            ImGui::Text("FBX Metadata:");
            ImGui::BulletText("Meshes: %d", meta.numMeshes);
            ImGui::BulletText("Animations: %d", meta.numAnimations);
            ImGui::BulletText("Bones: %d", meta.numBones);
            if (ImGui::TreeNode("Animation Names")) {
                for (const auto& name : meta.animationNames) {
                    ImGui::Text("- %s", name.c_str());
                }
                ImGui::TreePop();
            }

            if (ImGui::TreeNode("Bone Hierarchy")) {
                if (meta.numBones > 0) {
                    renderBoneHierarchy(0); // Start from root
                }
                ImGui::TreePop();
            }

            ImGui::Checkbox("Show Skinned Bones (Textured Skeleton)", &showSkinnedMesh);
            ImGui::Checkbox("Show Bone Names", &showBoneLabels);
            ImGui::DragFloat("Camera Distance", &cameraDist, 1.0f, 1.0f, 5000.0f);
            ImGui::DragFloat("Camera Height", &cameraHeight, 1.0f, -1000.0f, 1000.0f);
            ImGui::DragFloat("Camera Yaw", &cameraYaw, 1.0f);
            ImGui::DragFloat("Camera Pitch", &cameraPitch, 1.0f, -89.0f, 89.0f);

            if (ImGui::TreeNode("Texture Debug")) {
                for (auto const& [path, id] : textureCache) {
                    ImGui::Text("Path: %s", path.c_str());
                    ImGui::Image((void*)(intptr_t)id, ImVec2(256, 256));
                    ImGui::Separator();
                }
                if (textureCache.empty()) {
                    ImGui::Text("No textures loaded.");
                }
                ImGui::TreePop();
            }
        }

        ImGui::Separator();
        ImGui::Text("Animation State Mapping");
        const char* states[] = {"IDLE", "RUN", "JUMP"};
        for (int i = 0; i < 3; i++) {
            int clipIdx = currentAsset.states[states[i]];
            if (ImGui::InputInt(states[i], &clipIdx)) {
                currentAsset.states[states[i]] = clipIdx;
            }
        }

        ImGui::Separator();
        ImGui::Text("Physics Setup (Capsule Colliders)");
        if (ImGui::Button("Add Collider")) {
            currentAsset.colliders.push_back({"Head", 0.1f, 0.2f, 50.0f});
        }
        for (size_t i = 0; i < currentAsset.colliders.size(); i++) {
            auto& c = currentAsset.colliders[i];
            std::string label = "Collider " + std::to_string(i);
            if (ImGui::TreeNode(label.c_str())) {
                char boneName[64];
                strncpy(boneName, c.bone.c_str(), 64);
                if (ImGui::InputText("Bone", boneName, 64)) c.bone = boneName;
                ImGui::DragFloat("Radius", &c.radius, 0.01f);
                ImGui::DragFloat("Height", &c.height, 0.01f);
                ImGui::DragFloat("Damage Multiplier", &c.damage, 1.0f);
                if (ImGui::Button("Remove")) {
                    currentAsset.colliders.erase(currentAsset.colliders.begin() + i);
                }
                ImGui::TreePop();
            }
        }

        ImGui::Separator();
        if (ImGui::Button("Save Baked Asset")) {
            AssetBaking::save("soldier.asset.json", currentAsset);
        }

        ImGui::End();
    }

private:
    void renderBoneHierarchy(int boneIdx) {
        const auto& bones = sm.getBones();
        if (boneIdx < 0 || boneIdx >= (int)bones.size()) return;

        const auto& bone = bones[boneIdx];
        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;
        if (bone.children.empty()) flags |= ImGuiTreeNodeFlags_Leaf;

        bool nodeOpen = ImGui::TreeNodeEx((void*)(intptr_t)boneIdx, flags, "%s", bone.name.c_str());
        if (nodeOpen) {
            for (int childIdx : bone.children) {
                renderBoneHierarchy(childIdx);
            }
            ImGui::TreePop();
        }
    }

    FBXStateMachine sm;
    BakedAsset currentAsset;
    GLuint lineShader, lineVAO, lineVBO;
    GLuint skinnedShader;
    struct MeshGL {
        GLuint vao, vbo, ebo;
        GLsizei count;
        GLuint textureID = 0;
    };

    GLuint loadTexture(const std::string& path) {
        if (path.empty()) return 0;
        
        unsigned char *data = nullptr;
        int width, height, nrChannels;
        stbi_set_flip_vertically_on_load(true);

        std::vector<std::string> pathsToTry;
        if (path[0] == '*') {
            const aiTexture* embedded = sm.getEmbeddedTexture(path);
            if (embedded) {
                if (embedded->mHeight == 0) {
                    data = stbi_load_from_memory((unsigned char*)embedded->pcData, embedded->mWidth, &width, &height, &nrChannels, 0);
                } else {
                    // Raw ARGB8888 data
                    data = (unsigned char*)malloc(embedded->mWidth * embedded->mHeight * 4);
                    memcpy(data, embedded->pcData, embedded->mWidth * embedded->mHeight * 4);
                    width = embedded->mWidth;
                    height = embedded->mHeight;
                    nrChannels = 4;
                }
            }
        } else {
            std::string filename = path;
            std::string dir = "";
            size_t lastSlash = path.find_last_of("/\\");
            if (lastSlash != std::string::npos) {
                dir = path.substr(0, lastSlash + 1);
                filename = path.substr(lastSlash + 1);
            }

            pathsToTry.push_back(path);
            pathsToTry.push_back(dir + "textures/" + filename);
            pathsToTry.push_back(dir + "Textures/" + filename);

            std::string fbxBase = currentAsset.skeleton;
            size_t lastFbxSlash = fbxBase.find_last_of("/\\");
            if (lastFbxSlash != std::string::npos) fbxBase = fbxBase.substr(lastFbxSlash + 1);
            size_t lastFbxDot = fbxBase.find_last_of('.');
            if (lastFbxDot != std::string::npos) fbxBase = fbxBase.substr(0, lastFbxDot);
            
            if (!fbxBase.empty()) {
                pathsToTry.push_back(dir + fbxBase + ".fbm/" + filename);
            }

            size_t lastDot = filename.find_last_of('.');
            if (lastDot != std::string::npos) {
                std::string base = filename.substr(0, lastDot);
                std::string ext = filename.substr(lastDot);
                for (auto& c : ext) c = (char)std::tolower((unsigned char)c);
                
                std::vector<std::string> altExts = {".png", ".jpg", ".jpeg", ".tga", ".bmp"};
                for (const auto& alt : altExts) {
                    if (alt != ext) {
                        pathsToTry.push_back(dir + base + alt);
                        pathsToTry.push_back(dir + "textures/" + base + alt);
                        if (!fbxBase.empty()) {
                            pathsToTry.push_back(dir + fbxBase + ".fbm/" + base + alt);
                        }
                    }
                }
            }

            if (!dir.empty()) {
                pathsToTry.push_back(filename);
            }

            for (const auto& tryPath : pathsToTry) {
                data = stbi_load(tryPath.c_str(), &width, &height, &nrChannels, 0);
                if (data) break;
            }
        }

        if (data) {
            GLuint textureID;
            glGenTextures(1, &textureID);
            glBindTexture(GL_TEXTURE_2D, textureID);
            GLenum format = (nrChannels == 4) ? GL_RGBA : GL_RGB;
            glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
            glGenerateMipmap(GL_TEXTURE_2D);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            stbi_image_free(data);
            return textureID;
        } else {
            std::cerr << "Texture failed to load: " << path << std::endl;
            if (!pathsToTry.empty()) {
                std::cerr << "Tried paths:" << std::endl;
                for (const auto& t : pathsToTry) std::cerr << "  " << t << std::endl;
            }
            return 0;
        }
    }
    std::vector<MeshGL> meshGLs;
    std::map<std::string, GLuint> textureCache;
    bool showSkinnedMesh = false;
    bool showBoneLabels = false;
    float cameraDist = 300.0f;
    float cameraHeight = 100.0f;
    float cameraYaw = 0.0f;
    float cameraPitch = 20.0f;
};

#endif // CHARACTER_EDITOR_H
