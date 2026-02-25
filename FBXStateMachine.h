#ifndef FBX_STATE_MACHINE_H
#define FBX_STATE_MACHINE_H

#include <string>
#include <vector>
#include <map>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

enum State { IDLE, RUN, JUMP };

struct Bone {
    std::string name;
    glm::mat4 offsetMatrix;
    glm::mat4 localTransform;
    glm::mat4 finalTransform;
    int parentIndex = -1;
    std::vector<int> children;
    glm::mat4 worldTransform = glm::mat4(1.0f);
};

class FBXStateMachine {
public:
    void loadFBX(std::string path);
    void setState(State state);
    void setAnimationMapping(State state, int clipIndex);
    void update(float dt);
    
    std::vector<glm::mat4> getFinalBoneMatrices() { return finalBoneMatrices; }
    const std::vector<Bone>& getBones() const { return bones; }

    struct Vertex {
        glm::vec3 position;
        glm::vec2 uv;
        glm::ivec4 boneIds;
        glm::vec4 weights;
    };

    struct MeshData {
        std::vector<Vertex> vertices;
        std::vector<unsigned int> indices;
        std::string texturePath;
    };

    const std::vector<MeshData>& getMeshes() const { return meshes; }
    
    // Get embedded texture data
    const aiTexture* getEmbeddedTexture(const std::string& path) const;

    struct Metadata {
        int numAnimations = 0;
        int numMeshes = 0;
        int numBones = 0;
        std::vector<std::string> animationNames;
    };
    Metadata getMetadata() const;

private:
    void processNode(const aiNode* node, int parentIdx);
    void readNodeHierarchy(float animationTime, const aiNode* pNode, const glm::mat4& parentTransform, const aiAnimation* pAnimation);
    const aiNodeAnim* findNodeAnim(const aiAnimation* pAnimation, const std::string& nodeName);
    void calcInterpolatedRotation(aiQuaternion& out, float animationTime, const aiNodeAnim* pNodeAnim);
    void calcInterpolatedPosition(aiVector3D& out, float animationTime, const aiNodeAnim* pNodeAnim);
    void calcInterpolatedScaling(aiVector3D& out, float animationTime, const aiNodeAnim* pNodeAnim);
    unsigned int findRotation(float animationTime, const aiNodeAnim* pNodeAnim);
    unsigned int findPosition(float animationTime, const aiNodeAnim* pNodeAnim);
    unsigned int findScaling(float animationTime, const aiNodeAnim* pNodeAnim);

    std::string fbxDirectory;
    Assimp::Importer importer;
    const aiScene* scene = nullptr;
    std::vector<Bone> bones;
    std::map<std::string, int> boneMapping;
    std::vector<MeshData> meshes;
    std::vector<glm::mat4> finalBoneMatrices;
    glm::mat4 globalInverseTransform;
    
    State currentState = IDLE;
    float currentTime = 0.0f;
    std::map<State, int> stateToClipIndex;

    // Crossfade support
    State nextState = IDLE;
    float crossfadeTime = 0.0f;
    float crossfadeDuration = 0.2f;
    bool isCrossfading = false;
};

#endif
