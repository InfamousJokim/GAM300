#include "Core.h"
#include "Graphics/Models/Model.h"
#include "Auxiliaries/AssetLoadContext.h"

namespace Boom {

// ======================== StaticModel Implementation ========================

void StaticModel::LoadFromDiskCPU(const std::string& filename, StaticModelLoadContext& outContext) {
    outContext.filePath = filename;
    outContext.loadSuccess = false;

    uint32_t flags = aiProcess_Triangulate |
        aiProcess_GenSmoothNormals | aiProcess_CalcTangentSpace |
        aiProcess_OptimizeMeshes |
        aiProcess_OptimizeGraph | aiProcess_ValidateDataStructure |
        aiProcess_ImproveCacheLocality |
        aiProcess_FixInfacingNormals |
        aiProcess_GenUVCoords | aiProcess_FlipUVs;

    Assimp::Importer importer;
    const aiScene* ai_scene = importer.ReadFile(filename, flags);

    if (!ai_scene || ai_scene->mFlags == AI_SCENE_FLAGS_INCOMPLETE || !ai_scene->mRootNode) {
        std::string err = (importer.GetErrorString() && importer.GetErrorString()[0])
            ? importer.GetErrorString()
            : "incomplete model";
        outContext.errorMessage = "Failed to load model: " + err;
        return;
    }

    // Parse all meshes (CPU only - no OpenGL)
    std::function<void(const aiScene*, aiNode*)> parseNode = [&](const aiScene* scene, aiNode* node) {
        for (uint32_t i = 0; i < node->mNumMeshes; ++i) {
            aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
            MeshData<ShadedVert> meshData;

            // Vertices
            for (uint32_t v = 0; v < mesh->mNumVertices; ++v) {
                ShadedVert vert;
                vert.pos = AssimpToVec3(mesh->mVertices[v]);
                vert.norm = AssimpToVec3(mesh->mNormals[v]);
                if (mesh->HasTextureCoords(0)) {
                    vert.uv = { mesh->mTextureCoords[0][v].x, mesh->mTextureCoords[0][v].y };
                }
                if (mesh->HasTangentsAndBitangents()) {
                    vert.biTangent = glm::normalize(AssimpToVec3(mesh->mBitangents[v]));
                    vert.tangent = glm::normalize(AssimpToVec3(mesh->mTangents[v]));
                }
                meshData.vtx.push_back(vert);
            }

            // Indices
            for (uint32_t f = 0; f < mesh->mNumFaces; ++f) {
                for (uint32_t j = 0; j < mesh->mFaces[f].mNumIndices; ++j) {
                    meshData.idx.push_back(mesh->mFaces[f].mIndices[j]);
                }
            }

            outContext.meshes.push_back(std::move(meshData));
        }

        // Recurse children
        for (uint32_t i = 0; i < node->mNumChildren; ++i) {
            parseNode(scene, node->mChildren[i]);
        }
    };

    parseNode(ai_scene, ai_scene->mRootNode);
    outContext.loadSuccess = true;
}

StaticModel::StaticModel(const StaticModelLoadContext& context) {
    if (!context.loadSuccess) {
        BOOM_ERROR("StaticModel: Cannot create from failed load context: {}", context.errorMessage);
        return;
    }

    // Create GPU meshes from pre-loaded data (main thread only)
    for (const auto& meshData : context.meshes) {
        // Make two copies: one for physics mesh data, one for GPU upload
        MeshData<ShadedVert> physicsCopy = meshData;
        MeshData<ShadedVert> gpuCopy = meshData;

        m_PhysicsMeshData.push_back(physicsCopy);
        auto mesh = std::make_unique<ShadedMesh>(std::move(gpuCopy));
        meshes.push_back(std::move(mesh));
    }
}

// ======================== SkeletalModel Implementation ========================

void SkeletalModel::LoadFromDiskCPU(const std::string& filename, SkeletalModelLoadContext& outContext) {
    outContext.filePath = filename;
    outContext.loadSuccess = false;

    uint32_t flags = aiProcess_Triangulate |
        aiProcess_GenSmoothNormals | aiProcess_CalcTangentSpace |
        aiProcess_OptimizeMeshes |
        aiProcess_OptimizeGraph | aiProcess_ValidateDataStructure |
        aiProcess_ImproveCacheLocality |
        aiProcess_FixInfacingNormals |
        aiProcess_SortByPType |
        aiProcess_JoinIdenticalVertices |
        aiProcess_FlipUVs | aiProcess_GenUVCoords |
        aiProcess_LimitBoneWeights;

    Assimp::Importer importer;
    const aiScene* ai_scene = importer.ReadFile(filename, flags);

    if (!ai_scene || ai_scene->mFlags == AI_SCENE_FLAGS_INCOMPLETE || !ai_scene->mRootNode) {
        outContext.errorMessage = "Failed to load skeletal model: " + std::string(importer.GetErrorString());
        return;
    }

    outContext.globalTransform = glm::inverse(AssimpToMat4(ai_scene->mRootNode->mTransformation));

    // Joint map for tracking joints across meshes
    JointMap jointMap;

    // Parse all meshes
    std::function<void(const aiScene*, aiNode*)> parseNode = [&](const aiScene* scene, aiNode* node) {
        for (uint32_t i = 0; i < node->mNumMeshes; ++i) {
            aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
            MeshData<SkeletalVertex> meshData;

            // Vertices
            for (uint32_t v = 0; v < mesh->mNumVertices; ++v) {
                SkeletalVertex vert;
                vert.pos = AssimpToVec3(mesh->mVertices[v]);
                vert.norm = AssimpToVec3(mesh->mNormals[v]);
                if (mesh->HasTextureCoords(0)) {
                    vert.uv.x = mesh->mTextureCoords[0][v].x;
                    vert.uv.y = mesh->mTextureCoords[0][v].y;
                }
                meshData.vtx.push_back(vert);
            }

            // Indices
            for (uint32_t f = 0; f < mesh->mNumFaces; ++f) {
                for (uint32_t j = 0; j < mesh->mFaces[f].mNumIndices; ++j) {
                    meshData.idx.push_back(mesh->mFaces[f].mIndices[j]);
                }
            }

            // Joints (bones)
            for (uint32_t b = 0; b < mesh->mNumBones; ++b) {
                aiBone* ai_bone = mesh->mBones[b];
                std::string jointName(ai_bone->mName.C_Str());

                if (jointMap.count(jointName) == 0) {
                    jointMap[jointName].offset = AssimpToMat4(ai_bone->mOffsetMatrix);
                    jointMap[jointName].index = outContext.jointCount++;
                    jointMap[jointName].name = jointName;
                }

                // Set vertex joint weights
                for (uint32_t w = 0; w < ai_bone->mNumWeights; ++w) {
                    uint32_t vertexId = ai_bone->mWeights[w].mVertexId;
                    float weight = ai_bone->mWeights[w].mWeight;

                    // Find first available slot
                    for (uint32_t slot = 0; slot < 4; ++slot) {
                        if (meshData.vtx[vertexId].joints[slot] < 0) {
                            meshData.vtx[vertexId].joints[slot] = jointMap[jointName].index;
                            meshData.vtx[vertexId].weights[slot] = weight;
                            break;
                        }
                    }
                }
            }

            outContext.meshes.push_back(std::move(meshData));
        }

        // Recurse children
        for (uint32_t i = 0; i < node->mNumChildren; ++i) {
            parseNode(scene, node->mChildren[i]);
        }
    };

    parseNode(ai_scene, ai_scene->mRootNode);

    // Parse animations
    for (uint32_t i = 0; i < ai_scene->mNumAnimations; ++i) {
        aiAnimation* ai_anim = ai_scene->mAnimations[i];
        auto clip = std::make_shared<AnimationClip>();
        clip->name = ai_anim->mName.C_Str();
        clip->duration = (float)ai_anim->mDuration;
        clip->ticksPerSecond = (float)ai_anim->mTicksPerSecond;

        for (uint32_t j = 0; j < ai_anim->mNumChannels; ++j) {
            aiNodeAnim* ai_channel = ai_anim->mChannels[j];
            std::string jointName(ai_channel->mNodeName.C_Str());

            std::vector<KeyFrame>& track = clip->tracks[jointName];
            uint32_t maxKeys = std::max({
                ai_channel->mNumPositionKeys,
                ai_channel->mNumRotationKeys,
                ai_channel->mNumScalingKeys
            });

            track.reserve(maxKeys);
            for (uint32_t k = 0; k < maxKeys; ++k) {
                KeyFrame key;
                if (k < ai_channel->mNumPositionKeys) {
                    key.position = AssimpToVec3(ai_channel->mPositionKeys[k].mValue);
                    key.timeStamp = (float)ai_channel->mPositionKeys[k].mTime;
                }
                if (k < ai_channel->mNumRotationKeys) {
                    key.rotation = AssimpToQuat(ai_channel->mRotationKeys[k].mValue);
                }
                if (k < ai_channel->mNumScalingKeys) {
                    key.scale = AssimpToVec3(ai_channel->mScalingKeys[k].mValue);
                }
                track.push_back(key);
            }
        }

        outContext.clips.push_back(clip);
    }

    // Build joint hierarchy
    std::function<void(aiNode*, Joint&)> parseHierarchy = [&](aiNode* ai_node, Joint& joint) {
        std::string jointName(ai_node->mName.C_Str());

        if (jointMap.count(jointName)) {
            joint = jointMap[jointName];
            for (uint32_t i = 0; i < ai_node->mNumChildren; ++i) {
                Joint child;
                parseHierarchy(ai_node->mChildren[i], child);
                joint.children.push_back(std::move(child));
            }
        }
        else {
            for (uint32_t i = 0; i < ai_node->mNumChildren; ++i) {
                parseHierarchy(ai_node->mChildren[i], joint);
            }
        }
    };

    outContext.rootJoint = std::make_shared<Joint>();
    parseHierarchy(ai_scene->mRootNode, *outContext.rootJoint);
    outContext.loadSuccess = true;
}

SkeletalModel::SkeletalModel(const SkeletalModelLoadContext& context) {
    if (!context.loadSuccess) {
        BOOM_ERROR("SkeletalModel: Cannot create from failed load context: {}", context.errorMessage);
        return;
    }

    m_JointCount = context.jointCount;

    // Create animator
    m_Animator = std::make_shared<Animator>();
    m_Animator->m_GlobalTransform = context.globalTransform;
    m_Animator->m_Root = *context.rootJoint;
    m_Animator->m_Clips = context.clips;
    m_Animator->m_Transforms.resize(m_JointCount);

    // Create GPU meshes from pre-loaded data (main thread only)
    for (const auto& meshData : context.meshes) {
        MeshData<SkeletalVertex> copy = meshData;
        auto mesh = std::make_unique<SkeletalMesh>(std::move(copy));
        meshes.push_back(std::move(mesh));
    }
}

} // namespace Boom
