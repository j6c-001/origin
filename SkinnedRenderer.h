#ifndef SKINNED_RENDERER_H
#define SKINNED_RENDERER_H

#include <vector>
#include <string>
#include <iostream>
#include <map>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "FBXStateMachine.h"
#include "stb_image.h"

#ifdef __EMSCRIPTEN__
#include <GLES3/gl3.h>
#else
#include <GL/glew.h>
#endif

class SkinnedRenderer {
public:
    struct MeshGL {
        GLuint vao, vbo, ebo;
        GLsizei count;
        GLuint textureID = 0;
    };

    void init(const std::vector<FBXStateMachine::MeshData>& meshes) {
#ifdef __EMSCRIPTEN__
        const char* glslVersion = "#version 300 es";
#else
        const char* glslVersion = "#version 330 core";
#endif
        std::string vShaderSrc = glslVersion;
        vShaderSrc += R"(
            layout(std140) uniform BoneMatrices { mat4 u_bones[256]; };
            uniform mat4 u_vp;
            layout(location=0) in vec3 a_pos;
            layout(location=1) in vec2 a_uv;
            layout(location=2) in ivec4 a_boneIds;
            layout(location=3) in vec4 a_weights;
            out vec2 v_uv;
            void main() {
                vec4 pos = vec4(0.0);
                float totalWeight = 0.0;
                for(int i=0; i<4; i++) {
                    if(a_boneIds[i] >= 0 && a_boneIds[i] < 256) {
                        pos += a_weights[i] * (u_bones[a_boneIds[i]] * vec4(a_pos, 1.0));
                        totalWeight += a_weights[i];
                    }
                }
                if (totalWeight < 0.01) pos = vec4(a_pos, 1.0);
                gl_Position = u_vp * vec4(pos.xyz, 1.0);
                v_uv = a_uv;
            }
        )";

        std::string fShaderSrc = glslVersion;
        fShaderSrc += R"(
            precision mediump float;
            in vec2 v_uv;
            out vec4 FragColor;
            uniform sampler2D u_texture;
            uniform int u_hasTexture;
            void main() {
                if (u_hasTexture != 0) FragColor = texture(u_texture, v_uv);
                else FragColor = vec4(v_uv, 0.5, 1.0);
            }
        )";

        program = compileShader(vShaderSrc.c_str(), fShaderSrc.c_str());
        if (program == 0) {
            std::cerr << "Failed to compile SkinnedRenderer shaders" << std::endl;
        }
        
        glGenBuffers(1, &uboBones);
        glBindBuffer(GL_UNIFORM_BUFFER, uboBones);
        glBufferData(GL_UNIFORM_BUFFER, 256 * sizeof(glm::mat4), nullptr, GL_DYNAMIC_DRAW);
        
        // On desktop GL, layout(binding=0) might not be enough without this call
        GLuint blockIdx = glGetUniformBlockIndex(program, "BoneMatrices");
        if (blockIdx != GL_INVALID_INDEX) glUniformBlockBinding(program, blockIdx, 0);
        glBindBufferBase(GL_UNIFORM_BUFFER, 0, uboBones);

        std::cout << "Initializing renderer with " << meshes.size() << " meshes." << std::endl;
        for (const auto& mData : meshes) {
            MeshGL m;
            m.textureID = 0;
            if (!mData.texturePath.empty()) {
                if (textureCache.find(mData.texturePath) == textureCache.end()) {
                    GLuint texID = loadTexture(mData.texturePath);
                    if (texID != 0) textureCache[mData.texturePath] = texID;
                }
                if (textureCache.count(mData.texturePath)) m.textureID = textureCache[mData.texturePath];
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

    void render(const std::vector<glm::mat4>& bones) {
        glUseProgram(program);
        
        // Setup simple camera
        glm::mat4 view = glm::lookAt(glm::vec3(0, 100, 300), glm::vec3(0, 100, 0), glm::vec3(0, 1, 0));
        glm::mat4 projection = glm::perspective(glm::radians(45.0f), 1280.0f/720.0f, 0.1f, 10000.0f);
        glm::mat4 vp = projection * view;
        glUniformMatrix4fv(glGetUniformLocation(program, "u_vp"), 1, GL_FALSE, glm::value_ptr(vp));

        glBindBuffer(GL_UNIFORM_BUFFER, uboBones);
        if (!bones.empty()) {
            glBufferSubData(GL_UNIFORM_BUFFER, 0, std::min(bones.size(), (size_t)256) * sizeof(glm::mat4), bones.data());
        }

        for (const auto& m : meshGLs) {
            if (m.textureID != 0) {
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, m.textureID);
                glUniform1i(glGetUniformLocation(program, "u_texture"), 0);
                glUniform1i(glGetUniformLocation(program, "u_hasTexture"), 1);
            } else {
                glUniform1i(glGetUniformLocation(program, "u_hasTexture"), 0);
            }
            glBindVertexArray(m.vao);
            glDrawElements(GL_TRIANGLES, m.count, GL_UNSIGNED_INT, 0);
        }
        glBindVertexArray(0);
    }

private:
    GLuint compileShader(const char* vSrc, const char* fSrc) {
        GLuint vShader = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vShader, 1, &vSrc, nullptr);
        glCompileShader(vShader);
        checkShader(vShader, "Vertex");
        
        GLuint fShader = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fShader, 1, &fSrc, nullptr);
        glCompileShader(fShader);
        checkShader(fShader, "Fragment");
        
        GLuint prog = glCreateProgram();
        glAttachShader(prog, vShader);
        glAttachShader(prog, fShader);
        glLinkProgram(prog);
        checkProgram(prog);
        return prog;
    }

    void checkShader(GLuint shader, std::string type) {
        GLint success;
        GLchar infoLog[1024];
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success) {
            glGetShaderInfoLog(shader, 1024, NULL, infoLog);
            std::cerr << "ERROR::SHADER_COMPILATION_ERROR of type: " << type << "\n" << infoLog << "\n -- --------------------------------------------------- -- " << std::endl;
        }
    }

    void checkProgram(GLuint program) {
        GLint success;
        GLchar infoLog[1024];
        glGetProgramiv(program, GL_LINK_STATUS, &success);
        if (!success) {
            glGetProgramInfoLog(program, 1024, NULL, infoLog);
            std::cerr << "ERROR::PROGRAM_LINKING_ERROR\n" << infoLog << "\n -- --------------------------------------------------- -- " << std::endl;
        }
    }

    GLuint loadTexture(const std::string& path) {
        if (path.empty()) return 0;
        
        int w, h, ch;
        stbi_set_flip_vertically_on_load(true);
        
        std::vector<std::string> pathsToTry;
        pathsToTry.push_back(path);
        
        // Extract filename
        size_t lastSlash = path.find_last_of("/\\");
        std::string filename = (lastSlash == std::string::npos) ? path : path.substr(lastSlash + 1);
        std::string dir = (lastSlash == std::string::npos) ? "" : path.substr(0, lastSlash + 1);
        
        if (lastSlash != std::string::npos) {
            pathsToTry.push_back(filename); // Try current dir
            pathsToTry.push_back(dir + "textures/" + filename);
            pathsToTry.push_back(dir + "Textures/" + filename);
        } else {
            pathsToTry.push_back("textures/" + filename);
            pathsToTry.push_back("Textures/" + filename);
        }

        unsigned char* data = nullptr;
        for (const auto& tryPath : pathsToTry) {
            data = stbi_load(tryPath.c_str(), &w, &h, &ch, 0);
            if (data) break;
        }

        if (!data) {
            std::cerr << "Texture failed to load from all paths: " << path << std::endl;
            return 0;
        }

        GLuint tex;
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        GLenum fmt = (ch == 4) ? GL_RGBA : GL_RGB;
        glTexImage2D(GL_TEXTURE_2D, 0, fmt, w, h, 0, fmt, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        stbi_image_free(data);
        return tex;
    }

    GLuint uboBones;
    GLuint program;
    std::vector<MeshGL> meshGLs;
    std::map<std::string, GLuint> textureCache;
};

#endif
