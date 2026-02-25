#include "FBXStateMachine.h"
#include <iostream>

void FBXStateMachine::loadFBX(std::string path) {
    fbxDirectory = "";
    size_t lastSlash = path.find_last_of("/\\");
    if (lastSlash != std::string::npos) {
        fbxDirectory = path.substr(0, lastSlash + 1);
    }

    scene = importer.ReadFile(path, aiProcess_Triangulate | aiProcess_GenSmoothNormals | aiProcess_FlipUVs | aiProcess_JoinIdenticalVertices | aiProcess_LimitBoneWeights);
    if (!scene) {
        std::cerr << "Assimp error: " << importer.GetErrorString() << std::endl;
        return;
    }
    
    bones.clear();
    boneMapping.clear();
    meshes.clear();
    
    processNode(scene->mRootNode, -1);

    for (unsigned int i = 0; i < scene->mNumMeshes; i++) {
        aiMesh* mesh = scene->mMeshes[i];
        MeshData meshData;

        if (mesh->mMaterialIndex >= 0) {
            aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];
            aiString path;
            bool found = false;
            if (material->GetTexture(aiTextureType_DIFFUSE, 0, &path) == AI_SUCCESS) found = true;
            else if (material->GetTexture(aiTextureType_BASE_COLOR, 0, &path) == AI_SUCCESS) found = true;
            else if (material->GetTexture(aiTextureType_EMISSIVE, 0, &path) == AI_SUCCESS) found = true;
            else if (material->GetTexture(aiTextureType_AMBIENT, 0, &path) == AI_SUCCESS) found = true;
            else if (material->GetTexture(aiTextureType_UNKNOWN, 0, &path) == AI_SUCCESS) found = true;

            if (found) {
                std::string texPath = path.C_Str();
                if (texPath.size() > 0 && texPath[0] == '*') {
                    meshData.texturePath = texPath;
                } else {
                    // Replace backslashes with forward slashes for cross-platform
                    for (auto& c : texPath) if (c == '\\') c = '/';
                    
                    // Extract filename only for potential absolute path issues
                    size_t lastSlash = texPath.find_last_of('/');
                    std::string filename = (lastSlash == std::string::npos) ? texPath : texPath.substr(lastSlash + 1);
                    
                    meshData.texturePath = fbxDirectory + filename;
                }
            }
        }
        
        // Extract vertices
        for (unsigned int j = 0; j < mesh->mNumVertices; j++) {
            Vertex v;
            v.position = glm::vec3(mesh->mVertices[j].x, mesh->mVertices[j].y, mesh->mVertices[j].z);
            v.uv = mesh->HasTextureCoords(0) ? glm::vec2(mesh->mTextureCoords[0][j].x, mesh->mTextureCoords[0][j].y) : glm::vec2(0.0f);
            v.boneIds = glm::ivec4(0);
            v.weights = glm::vec4(0.0f);
            meshData.vertices.push_back(v);
        }

        // Extract indices
        for (unsigned int j = 0; j < mesh->mNumFaces; j++) {
            aiFace face = mesh->mFaces[j];
            for (unsigned int k = 0; k < face.mNumIndices; k++) {
                meshData.indices.push_back(face.mIndices[k]);
            }
        }

        // Extract bone weights
        std::vector<int> boneCount(mesh->mNumVertices, 0);
        for (unsigned int j = 0; j < mesh->mNumBones; j++) {
            aiBone* aiBonePtr = mesh->mBones[j];
            std::string boneName = aiBonePtr->mName.C_Str();
            int boneIdx = -1;
            if (boneMapping.find(boneName) != boneMapping.end()) {
                boneIdx = boneMapping[boneName];
                bones[boneIdx].offsetMatrix = glm::transpose(glm::make_mat4(&aiBonePtr->mOffsetMatrix.a1));
            }

            for (unsigned int k = 0; k < aiBonePtr->mNumWeights; k++) {
                unsigned int vertexId = aiBonePtr->mWeights[k].mVertexId;
                float weight = aiBonePtr->mWeights[k].mWeight;
                if (boneCount[vertexId] < 4) {
                    meshData.vertices[vertexId].boneIds[boneCount[vertexId]] = boneIdx;
                    meshData.vertices[vertexId].weights[boneCount[vertexId]] = weight;
                    boneCount[vertexId]++;
                }
            }
        }
        meshes.push_back(meshData);
    }

    aiMatrix4x4 globalTransform = scene->mRootNode->mTransformation;
    globalTransform.Inverse();
    globalInverseTransform = glm::transpose(glm::make_mat4(&globalTransform.a1));

    // Initialize bone matrices to bind pose
    readNodeHierarchy(0.0f, scene->mRootNode, glm::mat4(1.0f), nullptr);
    finalBoneMatrices.resize(bones.size());
    for (size_t i = 0; i < bones.size(); i++) {
        finalBoneMatrices[i] = bones[i].finalTransform;
    }
}

void FBXStateMachine::processNode(const aiNode* node, int parentIdx) {
    Bone bone;
    bone.name = node->mName.C_Str();
    bone.parentIndex = parentIdx;
    bone.localTransform = glm::transpose(glm::make_mat4(&node->mTransformation.a1));
    bone.offsetMatrix = glm::mat4(1.0f); 
    
    int currentIdx = static_cast<int>(bones.size());
    bones.push_back(bone);
    boneMapping[bone.name] = currentIdx;
    
    if (parentIdx != -1) {
        bones[parentIdx].children.push_back(currentIdx);
    }
    
    for (unsigned int i = 0; i < node->mNumChildren; i++) {
        processNode(node->mChildren[i], currentIdx);
    }
}

void FBXStateMachine::setState(State state) {
    if (currentState == state) return;
    nextState = state;
    isCrossfading = true;
    crossfadeTime = 0.0f;
}

void FBXStateMachine::setAnimationMapping(State state, int clipIndex) {
    stateToClipIndex[state] = clipIndex;
}

const aiTexture* FBXStateMachine::getEmbeddedTexture(const std::string& path) const {
    if (!scene || path.empty() || path[0] != '*') return nullptr;
    return scene->GetEmbeddedTexture(path.c_str());
}

FBXStateMachine::Metadata FBXStateMachine::getMetadata() const {
    Metadata meta;
    if (scene) {
        meta.numAnimations = scene->mNumAnimations;
        meta.numMeshes = scene->mNumMeshes;
        meta.numBones = bones.size();
        for (unsigned int i = 0; i < scene->mNumAnimations; i++) {
            meta.animationNames.push_back(scene->mAnimations[i]->mName.C_Str());
        }
    }
    return meta;
}

void FBXStateMachine::update(float dt) {
    if (!scene || scene->mNumAnimations == 0) return;

    currentTime += dt;
    
    // Basic animation loop for current state
    const aiAnimation* pAnimation = scene->mAnimations[stateToClipIndex[currentState]];
    float ticksPerSecond = pAnimation->mTicksPerSecond != 0 ? pAnimation->mTicksPerSecond : 25.0f;
    float timeInTicks = currentTime * ticksPerSecond;
    float animationTime = fmod(timeInTicks, (float)pAnimation->mDuration);

    readNodeHierarchy(animationTime, scene->mRootNode, glm::mat4(1.0f), pAnimation);
    
    // Update final matrices for GPU
    finalBoneMatrices.resize(bones.size());
    for (size_t i = 0; i < bones.size(); i++) {
        finalBoneMatrices[i] = bones[i].finalTransform;
    }
    
    if (isCrossfading) {
        crossfadeTime += dt;
        if (crossfadeTime >= crossfadeDuration) {
            currentState = nextState;
            isCrossfading = false;
        }
        // Actually blend animations here in a real implementation
    }
}

void FBXStateMachine::readNodeHierarchy(float animationTime, const aiNode* pNode, const glm::mat4& parentTransform, const aiAnimation* pAnimation) {
    std::string nodeName(pNode->mName.data);
    glm::mat4 nodeTransform = glm::transpose(glm::make_mat4(&pNode->mTransformation.a1));

    const aiNodeAnim* pNodeAnim = findNodeAnim(pAnimation, nodeName);
    if (pNodeAnim) {
        // Interpolate scaling, rotation, translation
        aiVector3D scaling;
        calcInterpolatedScaling(scaling, animationTime, pNodeAnim);
        glm::mat4 scalingM = glm::scale(glm::mat4(1.0f), glm::vec3(scaling.x, scaling.y, scaling.z));

        aiQuaternion rotation;
        calcInterpolatedRotation(rotation, animationTime, pNodeAnim);
        aiMatrix4x4 rotationMAi = aiMatrix4x4(rotation.GetMatrix());
        glm::mat4 rotationM = glm::transpose(glm::make_mat4(&rotationMAi.a1));

        aiVector3D translation;
        calcInterpolatedPosition(translation, animationTime, pNodeAnim);
        glm::mat4 translationM = glm::translate(glm::mat4(1.0f), glm::vec3(translation.x, translation.y, translation.z));

        nodeTransform = translationM * rotationM * scalingM;
    }

    glm::mat4 globalTransform = parentTransform * nodeTransform;

    if (boneMapping.find(nodeName) != boneMapping.end()) {
        int boneIndex = boneMapping[nodeName];
        bones[boneIndex].worldTransform = globalTransform;
        bones[boneIndex].finalTransform = globalInverseTransform * globalTransform * bones[boneIndex].offsetMatrix;
    }

    for (unsigned int i = 0; i < pNode->mNumChildren; i++) {
        readNodeHierarchy(animationTime, pNode->mChildren[i], globalTransform, pAnimation);
    }
}

const aiNodeAnim* FBXStateMachine::findNodeAnim(const aiAnimation* pAnimation, const std::string& nodeName) {
    if (!pAnimation) return nullptr;
    for (unsigned int i = 0; i < pAnimation->mNumChannels; i++) {
        const aiNodeAnim* pNodeAnim = pAnimation->mChannels[i];
        if (std::string(pNodeAnim->mNodeName.data) == nodeName) {
            return pNodeAnim;
        }
    }
    return nullptr;
}

void FBXStateMachine::calcInterpolatedRotation(aiQuaternion& out, float animationTime, const aiNodeAnim* pNodeAnim) {
    if (pNodeAnim->mNumRotationKeys == 1) {
        out = pNodeAnim->mRotationKeys[0].mValue;
        return;
    }
    unsigned int rotationIndex = findRotation(animationTime, pNodeAnim);
    unsigned int nextRotationIndex = rotationIndex + 1;
    float deltaTime = (float)(pNodeAnim->mRotationKeys[nextRotationIndex].mTime - pNodeAnim->mRotationKeys[rotationIndex].mTime);
    float factor = (animationTime - (float)pNodeAnim->mRotationKeys[rotationIndex].mTime) / deltaTime;
    const aiQuaternion& startRotationQ = pNodeAnim->mRotationKeys[rotationIndex].mValue;
    const aiQuaternion& endRotationQ = pNodeAnim->mRotationKeys[nextRotationIndex].mValue;
    aiQuaternion::Interpolate(out, startRotationQ, endRotationQ, factor);
    out = out.Normalize();
}

void FBXStateMachine::calcInterpolatedPosition(aiVector3D& out, float animationTime, const aiNodeAnim* pNodeAnim) {
    if (pNodeAnim->mNumPositionKeys == 1) {
        out = pNodeAnim->mPositionKeys[0].mValue;
        return;
    }
    unsigned int positionIndex = findPosition(animationTime, pNodeAnim);
    unsigned int nextPositionIndex = positionIndex + 1;
    float deltaTime = (float)(pNodeAnim->mPositionKeys[nextPositionIndex].mTime - pNodeAnim->mPositionKeys[positionIndex].mTime);
    float factor = (animationTime - (float)pNodeAnim->mPositionKeys[positionIndex].mTime) / deltaTime;
    const aiVector3D& start = pNodeAnim->mPositionKeys[positionIndex].mValue;
    const aiVector3D& end = pNodeAnim->mPositionKeys[nextPositionIndex].mValue;
    aiVector3D delta = end - start;
    out = start + factor * delta;
}

void FBXStateMachine::calcInterpolatedScaling(aiVector3D& out, float animationTime, const aiNodeAnim* pNodeAnim) {
    if (pNodeAnim->mNumScalingKeys == 1) {
        out = pNodeAnim->mScalingKeys[0].mValue;
        return;
    }
    unsigned int scalingIndex = findScaling(animationTime, pNodeAnim);
    unsigned int nextScalingIndex = scalingIndex + 1;
    float deltaTime = (float)(pNodeAnim->mScalingKeys[nextScalingIndex].mTime - pNodeAnim->mScalingKeys[scalingIndex].mTime);
    float factor = (animationTime - (float)pNodeAnim->mScalingKeys[scalingIndex].mTime) / deltaTime;
    const aiVector3D& start = pNodeAnim->mScalingKeys[scalingIndex].mValue;
    const aiVector3D& end = pNodeAnim->mScalingKeys[nextScalingIndex].mValue;
    aiVector3D delta = end - start;
    out = start + factor * delta;
}

unsigned int FBXStateMachine::findRotation(float animationTime, const aiNodeAnim* pNodeAnim) {
    for (unsigned int i = 0; i < pNodeAnim->mNumRotationKeys - 1; i++) {
        if (animationTime < (float)pNodeAnim->mRotationKeys[i + 1].mTime) return i;
    }
    return 0;
}

unsigned int FBXStateMachine::findPosition(float animationTime, const aiNodeAnim* pNodeAnim) {
    for (unsigned int i = 0; i < pNodeAnim->mNumPositionKeys - 1; i++) {
        if (animationTime < (float)pNodeAnim->mPositionKeys[i + 1].mTime) return i;
    }
    return 0;
}

unsigned int FBXStateMachine::findScaling(float animationTime, const aiNodeAnim* pNodeAnim) {
    for (unsigned int i = 0; i < pNodeAnim->mNumScalingKeys - 1; i++) {
        if (animationTime < (float)pNodeAnim->mScalingKeys[i + 1].mTime) return i;
    }
    return 0;
}
