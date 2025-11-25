#pragma once
#include "GlobalConstants.h"
#include "Helper.h"
#include "Animator.h"
#include "BoomProperties.h"
#include "Graphics/Utilities/Data.h"

#include <assimp/postprocess.h>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>

namespace Boom {
	// Forward declarations for two-phase loading
	struct StaticModelLoadContext;
	struct SkeletalModelLoadContext;

	struct Model
	{
		BOOM_INLINE Model() = default;
		BOOM_INLINE Model(std::string const&) {};
		BOOM_INLINE virtual bool HasJoint() { return false; }
		BOOM_INLINE virtual void Draw(uint32_t = GL_TRIANGLES) {};

		static BOOM_INLINE bool CheckForJoints(std::string const& filename) {
			Assimp::Importer importer; //will auto free
			return ModelHasJoints(importer.ReadFile(filename, aiProcess_Triangulate | aiProcess_GenNormals));
		}

	private:
		static BOOM_INLINE bool ModelHasJoints(const aiScene* scene) {
			if (!scene) return false;

			for (unsigned int i = 0; i < scene->mNumMeshes; ++i) {
				aiMesh* mesh = scene->mMeshes[i];
				if (mesh->mNumBones > 0) {
					return true;
				}
			}
			return false;
		}

	public:
		Transform3D modelTransform{};
		XPROPERTY_DEF(
			"Model", Model,
			obj_member<"transform", &Model::modelTransform>
		)
	};

	//---------------------------Static Model------------------------------
	struct StaticModel : Model
	{
		BOOM_INLINE StaticModel() = default;
		/**
		 * @brief Loads meshes from a static (non-skeletal) model file via Assimp.
		 * @param filename File name relative to CONSTANTS::MODELS_LOCATION.
		 * @details Applies a set of Assimp post-process flags for real-time rendering.
		 *          On failure, logs an error and leaves the model empty.
		 */
		BOOM_INLINE StaticModel(std::string const& filename)
		{
			uint32_t flags = aiProcess_Triangulate |
				aiProcess_GenSmoothNormals | aiProcess_CalcTangentSpace |
				aiProcess_OptimizeMeshes |
				aiProcess_OptimizeGraph | aiProcess_ValidateDataStructure |
				aiProcess_ImproveCacheLocality |
				aiProcess_FixInfacingNormals |
				aiProcess_GenUVCoords | aiProcess_FlipUVs;

			Assimp::Importer importer;
			const aiScene* ai_scene = importer.ReadFile(filename, flags);

			if (!ai_scene || ai_scene->mFlags == AI_SCENE_FLAGS_INCOMPLETE || !ai_scene->mRootNode)
			{
				std::string tmp{ std::filesystem::path(filename).filename().string() };
				std::string err{ strcmp(importer.GetErrorString(), "") == 0 ? "incomplete model" : importer.GetErrorString()};
				BOOM_ERROR("failed to load model({})-{}", tmp, err);
				return;
			}

			// parse all meshes
			ParseNode(ai_scene, ai_scene->mRootNode);
		}

		/**
		 * @brief NEW: Two-phase loading constructor - uploads pre-loaded model data to GPU
		 * @param context Pre-loaded model data from worker thread
		 */
		StaticModel(const StaticModelLoadContext& context);

		/**
		 * @brief NEW: Static CPU-side loading (no OpenGL calls - can run on worker thread)
		 * @param filename File path to load
		 * @param outContext Output context to fill with mesh data
		 */
		static void LoadFromDiskCPU(const std::string& filename, StaticModelLoadContext& outContext);

		BOOM_INLINE const std::vector<MeshData<ShadedVert>>& GetMeshData() const {
			return m_PhysicsMeshData;
		}

		BOOM_INLINE void Draw(uint32_t mode = GL_TRIANGLES) override
		{
			for (auto& mesh : meshes) {
				mesh->Draw(mode);
			}
		}

	private:
		/**
		* @brief Recursively parse a scene node and its children to collect meshes.
		* @param scene Assimp scene.
		* @param node Current Assimp node to parse.
		*/
		BOOM_INLINE void ParseNode(aiScene const* scene, aiNode* node)
		{
			for (uint32_t i{}; i < node->mNumMeshes; ++i) {
				ParseMesh(scene->mMeshes[node->mMeshes[i]]);
			}

			for (uint32_t i{}; i < node->mNumChildren; ++i) {
				ParseNode(scene, node->mChildren[i]);
			}
		}

		/**
		* @brief Convert an Assimp mesh to an engine mesh and append it to the list.
		* @param mesh Assimp mesh pointer.
		*/
		BOOM_INLINE void ParseMesh(aiMesh* mesh)
		{
			MeshData<ShadedVert> meshData{};

			//vertex data
			for (uint32_t i{}; i < mesh->mNumVertices; ++i) {
				ShadedVert vert;
				vert.pos = AssimpToVec3(mesh->mVertices[i]);
				vert.norm = AssimpToVec3(mesh->mNormals[i]);
				if (mesh->HasTextureCoords(0)) vert.uv = {mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y};
				if (mesh->HasTangentsAndBitangents()) {
					vert.biTangent = glm::normalize(AssimpToVec3(mesh->mBitangents[i]));
					vert.tangent = glm::normalize(AssimpToVec3(mesh->mTangents[i]));
				}

				meshData.vtx.push_back(vert);
			}

			//indicies data
			for (uint32_t i{}; i < mesh->mNumFaces; ++i) {
				for (uint32_t j{}; j < mesh->mFaces[i].mNumIndices; ++j) {
					meshData.idx.push_back(mesh->mFaces[i].mIndices[j]);
				}
			}

			m_PhysicsMeshData.push_back(meshData);

			auto ret{ std::make_unique<ShadedMesh>(std::move(meshData)) };
			meshes.push_back(std::move(ret));
		}

	private:
		std::vector<Mesh3D> meshes;
		std::vector<MeshData<ShadedVert>> m_PhysicsMeshData;
	};

	//---------------------------Skeletal Model------------------------------

	struct SkeletalModel : Model
	{
		using JointMap = std::unordered_map<std::string, Joint>;

		BOOM_INLINE SkeletalModel() = default;
		/**
		* @brief Load meshes, skeleton, and animation clips via Assimp.
		* @param path File path relative to CONSTANTS::MODELS_LOCATION.
		* @details Builds mesh data with per-vertex joint weights, constructs the joint hierarchy,
		*          and parses available animation channels into the Animator.
		*          On failure, logs an error and leaves the model empty.
		*/
		BOOM_INLINE SkeletalModel(std::string const& filename)
		{
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
			if (!ai_scene || ai_scene->mFlags == AI_SCENE_FLAGS_INCOMPLETE || !ai_scene->mRootNode)
			{
				BOOM_ERROR("failed to load model: �{}�", importer.GetErrorString());
				return;
			}

			m_Animator = std::make_shared<Animator>();
			m_Animator->m_GlobalTransform = glm::inverse(AssimpToMat4(ai_scene->mRootNode->mTransformation));

			// temp joints map
			JointMap jointMap{};

			// parse all meshes
			ParseNode(ai_scene, ai_scene->mRootNode, jointMap);

			// parse animations
			ParseAnimations(ai_scene, jointMap);
		}

		/**
		 * @brief NEW: Two-phase loading constructor - uploads pre-loaded skeletal model data to GPU
		 * @param context Pre-loaded skeletal model data from worker thread
		 */
		SkeletalModel(const SkeletalModelLoadContext& context);

		/**
		 * @brief NEW: Static CPU-side loading (no OpenGL calls - can run on worker thread)
		 * @param filename File path to load
		 * @param outContext Output context to fill with skeletal mesh and animation data
		 */
		static void LoadFromDiskCPU(const std::string& filename, SkeletalModelLoadContext& outContext);

		BOOM_INLINE void Draw(uint32_t mode = GL_TRIANGLES) override final
		{
			for (auto& mesh : meshes)
			{
				mesh->Draw(mode);
			}
		}

		/**
		* @brief Access the animator controlling this model's skeleton and clips.
		* @return Shared pointer to the Animator (non-null after successful Load()).
		*/
		BOOM_INLINE auto GetAnimator() { return m_Animator; }

		BOOM_INLINE bool HasJoint() override final { return m_JointCount > 0; }
	private:

		/**
		* @brief Recursively parse a scene node, converting its meshes and accumulating joints into the map.
		* @param ai_scene Assimp scene.
		* @param ai_node Current node.
		* @param jointMap Map of joint name -> Joint data (filled as meshes are parsed).
		*/
		BOOM_INLINE void ParseNode(const aiScene* ai_scene, aiNode* ai_node, JointMap& jointMap)
		{
			for (uint32_t i = 0; i < ai_node->mNumMeshes; i++)
			{
				ParseMesh(ai_scene->mMeshes[ai_node->mMeshes[i]], jointMap);
			}

			for (uint32_t i = 0; i < ai_node->mNumChildren; i++)
			{
				ParseNode(ai_scene, ai_node->mChildren[i], jointMap);
			}
		}

		/**
		 * @brief Assign a joint influence to a vertex in the first available slot.
		 * @param vertex Target vertex to modify.
		 * @param id Joint palette index.
		 * @param weight Weight in [0,1].
		 * @note At most 4 weights are stored; any additional influences are ignored.
		 */
		BOOM_INLINE void SetVertexJoint(SkeletalVertex& vertex, int32_t id, float weight)
		{
			for (uint32_t i = 0; i < 4; i++)
			{
				if (vertex.joints[i] < 0)
				{
					vertex.weights[i] = weight;
					vertex.joints[i] = id;
					return;
				}
			}
		}

		/**
		* @brief Build the joint hierarchy starting from an Assimp node and a pre-filled joint map.
		* @param ai_node Current Assimp node.
		* @param joint Output joint node (filled and expanded recursively).
		* @param jointMap Lookup of previously discovered joints (from mesh/bone parsing).
		* @details If the node name exists in jointMap, attaches it and constructs children.
		*          Otherwise, continues searching down the tree until matching joints are found.
		*/
		BOOM_INLINE void ParseHierarchy(aiNode* ai_node, Joint& joint, JointMap& jointMap)
		{
			std::string jointName(ai_node->mName.C_Str());

			if (jointMap.count(jointName))
			{
				joint = jointMap[jointName];

				for (uint32_t i = 0; i < ai_node->mNumChildren; i++)
				{
					Joint child;
					ParseHierarchy(ai_node->mChildren[i], child, jointMap);
					joint.children.push_back(std::move(child));
				}
			}
			else
			{
				for (uint32_t i = 0; i < ai_node->mNumChildren; i++)
				{
					ParseHierarchy(ai_node->mChildren[i], joint, jointMap);
				}
			}
		}

		/**
		 * @brief Parse animation clips and fill per-joint key tracks, then finalize hierarchy and animator buffers.
		 * @param ai_scene Assimp scene containing animations.
		 * @param jointMap Joint lookup built during mesh parsing.
		 * @note Animation.duration uses Assimp's duration; Animation.speed uses ticks-per-second (tps).
		 *       Callers may convert timestamps to seconds via (time / tps) during sampling.
		 */
		BOOM_INLINE void ParseAnimations(const aiScene* ai_scene, JointMap& jointMap)
		{
			m_Animator->m_Clips.reserve(ai_scene->mNumAnimations);

			for (uint32_t i = 0; i < ai_scene->mNumAnimations; i++)
			{
				auto ai_anim = ai_scene->mAnimations[i];

				// Create a new animation clip
				auto clip = std::make_shared<AnimationClip>();
				clip->name = ai_anim->mName.C_Str();
				clip->duration = (float)ai_anim->mDuration;
				clip->ticksPerSecond = (float)ai_anim->mTicksPerSecond;

				// Parse each animation channel (one per animated joint)
				for (uint32_t j = 0; j < ai_anim->mNumChannels; j++)
				{
					aiNodeAnim* ai_channel = ai_anim->mChannels[j];
					std::string jointName(ai_channel->mNodeName.C_Str());

					// Create track for this joint
					std::vector<KeyFrame>& track = clip->tracks[jointName];

					uint32_t maxKeys = std::max({
						ai_channel->mNumPositionKeys,
						ai_channel->mNumRotationKeys,
						ai_channel->mNumScalingKeys
						});

					track.reserve(maxKeys);

					for (uint32_t k = 0; k < maxKeys; k++)
					{
						KeyFrame key;

						if (k < ai_channel->mNumPositionKeys)
						{
							key.position = AssimpToVec3(ai_channel->mPositionKeys[k].mValue);
							key.timeStamp = (float)ai_channel->mPositionKeys[k].mTime;
						}
						if (k < ai_channel->mNumRotationKeys)
						{
							key.rotation = AssimpToQuat(ai_channel->mRotationKeys[k].mValue);
						}
						if (k < ai_channel->mNumScalingKeys)
						{
							key.scale = AssimpToVec3(ai_channel->mScalingKeys[k].mValue);
						}

						track.push_back(key);
					}
				}

				m_Animator->m_Clips.push_back(clip);
			}

			// Build joint hierarchy (unchanged)
			ParseHierarchy(ai_scene->mRootNode, m_Animator->m_Root, jointMap);
			m_Animator->m_Transforms.resize(m_JointCount);
		}

		/**
		* @brief Convert an Assimp mesh into a skinned mesh, collecting vertices, indices, and bone weights.
		* @param ai_mesh Assimp mesh pointer.
		* @param jointMap Joint lookup to populate (creates new joints as needed).
		*/
		BOOM_INLINE void ParseMesh(const aiMesh* ai_mesh, JointMap& jointMap)
		{
			// mesh data
			MeshData<SkeletalVertex> data;

			// vertices
			for (uint32_t i = 0; i < ai_mesh->mNumVertices; i++)
			{
				SkeletalVertex vertex;
				// positions
				vertex.pos = AssimpToVec3(ai_mesh->mVertices[i]);
				// normals
				vertex.norm = AssimpToVec3(ai_mesh->mNormals[i]);
				// texcoords
				if (ai_mesh->HasTextureCoords(0)) {
					vertex.uv.x = ai_mesh->mTextureCoords[0][i].x;
					vertex.uv.y = ai_mesh->mTextureCoords[0][i].y;
				}
				
				// push vertex
				data.vtx.push_back(std::move(vertex));
			}

			// indices   
			for (uint32_t i = 0; i < ai_mesh->mNumFaces; i++)
			{
				for (uint32_t k = 0; k < ai_mesh->mFaces[i].mNumIndices; k++)
				{
					data.idx.push_back(ai_mesh->mFaces[i].mIndices[k]);
				}
			}

			// joints
			for (uint32_t i = 0; i < ai_mesh->mNumBones; i++)
			{
				aiBone* ai_bone = ai_mesh->mBones[i];

				// get joint name
				std::string jointName(ai_bone->mName.C_Str());

				// add joint if not found
				if (jointMap.count(jointName) == 0)
				{
					jointMap[jointName].offset = AssimpToMat4(ai_bone->mOffsetMatrix);
					jointMap[jointName].index = m_JointCount++;
					jointMap[jointName].name = jointName;
				}

				// set vertex joint weights
				for (uint32_t j = 0; j < ai_bone->mNumWeights; j++)
				{
					SetVertexJoint(data.vtx[ai_bone->mWeights[j].mVertexId], jointMap[jointName].index, ai_bone->mWeights[j].mWeight);
				}
			}

			// create new mesh instance
			meshes.push_back(std::make_unique<SkeletalMesh>(std::move(data)));

		}

	private:
		std::vector<std::unique_ptr<SkeletalMesh>> meshes;
		std::shared_ptr<Animator> m_Animator;
		uint32_t m_JointCount = 0;
	};

	// helper type definitions
	using Animator3D = std::shared_ptr<Animator>;
	using Model3D = std::shared_ptr<Model>;
}
