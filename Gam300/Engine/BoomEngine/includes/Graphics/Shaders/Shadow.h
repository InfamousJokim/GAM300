#pragma once
#include "Shader.h"
#include "Graphics/Models/Model.h"
#include "Graphics/Utilities/Data.h"
namespace Boom {
    struct ShadowShader : Shader
    {
        BOOM_INLINE ShadowShader(const std::string& path) : Shader(path)
        {
            u_LightSpace = glGetUniformLocation(shaderId, "u_lightSpace");
            u_Model = glGetUniformLocation(shaderId, "u_model");
            
            // create frame buffer
            glGenFramebuffers(1, &m_FrameBuffer);

            // create depth texture
            glGenTextures(1, &m_DepthMap);
            glBindTexture(GL_TEXTURE_2D, m_DepthMap);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, MapSize, MapSize, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

            // attach to frame buffer
            glBindFramebuffer(GL_FRAMEBUFFER, m_FrameBuffer);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, m_DepthMap, 0);

            // no drawing nor reading 
            glDrawBuffer(GL_NONE);
            glReadBuffer(GL_NONE);

            // check frame buffer
            if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            {
                BOOM_ERROR("CreateDepthBuffer() Failed!");
            }

            // unbind frame buffer
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
        }
        BOOM_INLINE void Draw(Model3D& model, Transform3D& transform)
        {
            SetUniform(jointsLoc, model->HasJoint());
            SetUniform(u_Model, transform.Matrix() * model->modelTransform.Matrix());
            //glCullFace(GL_FRONT);
            model->Draw();
            //glCullFace(GL_BACK);
        }

        BOOM_INLINE void BeginFrame(const glm::mat4& lightSpaceMtx)
        {
            // bind shadow shader
            Use();

            // set view projection matrix
            SetUniform(u_LightSpace, lightSpaceMtx);

            // set viewport a clear buffer
            glEnable(GL_DEPTH_TEST);
            glViewport(0, 0, MapSize, MapSize);
            glBindFramebuffer(GL_FRAMEBUFFER, m_FrameBuffer);
            glClear(GL_DEPTH_BUFFER_BIT);
        }

        BOOM_INLINE uint32_t GetDepthMap()
        {
            return m_DepthMap;
        }

        BOOM_INLINE void EndFrame()
        {
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glDisable(GL_DEPTH_TEST);
            UnUse();
        }

        BOOM_INLINE ~ShadowShader()
        {
            glDeleteFramebuffers(1, &m_FrameBuffer);
            glDeleteTextures(1, &m_DepthMap);
        }

        //Animation 
        BOOM_INLINE void SetJoints(std::vector<glm::mat4>& transforms)
        {
            for (size_t i = 0; i < transforms.size() && i < 100; ++i)
            {
                std::string uniform = "jointsMat[" + std::to_string(i) + "]";
                SetUniform(GetUniformVar(uniform.c_str()), transforms[i]);
            }
        }

    private:
        uint32_t m_FrameBuffer = 0u;
        uint32_t m_DepthMap = 0u;
        int32_t MapSize = 1024;

        int32_t jointsLoc;
        uint32_t u_LightSpace = 0u;
        uint32_t u_Model = 0u;
    };
}