#pragma once
#include "Shader.h"
#include <vector>

namespace Boom {

    // CPU layout: 2 vertices per line (position + color)
    struct LineVert {
        glm::vec3 pos;
        glm::vec4 col;
    };

    class DebugLinesShader : public Shader {
    public:
        BOOM_INLINE DebugLinesShader(const std::string& path = "debug_lines.glsl")
            : Shader(path)
        {
            u_View = GetUniformVar("u_View");
            u_Proj = GetUniformVar("u_Proj");

            glGenVertexArrays(1, &m_VAO);
            glGenBuffers(1, &m_VBO);

            glBindVertexArray(m_VAO);
            glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
            glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_DYNAMIC_DRAW);

            // layout(location=0) vec3 aPos
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(LineVert), (void*)offsetof(LineVert, pos));

            // layout(location=1) vec4 aCol
            glEnableVertexAttribArray(1);
            glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(LineVert), (void*)offsetof(LineVert, col));

            glBindVertexArray(0);
            glBindBuffer(GL_ARRAY_BUFFER, 0);
        }

        BOOM_INLINE ~DebugLinesShader()
        {
            glDeleteBuffers(1, &m_VBO);
            glDeleteVertexArrays(1, &m_VAO);
        }

        // Draw world-space colored lines
        BOOM_INLINE void Draw(const glm::mat4& view, const glm::mat4& proj,
            const std::vector<LineVert>& verts,
            float lineWidth = 1.5f)
        {
            if (verts.empty()) return;

            Use();
            SetUniform(u_View, view);
            SetUniform(u_Proj, proj);

            glBindVertexArray(m_VAO);
            glBindBuffer(GL_ARRAY_BUFFER, m_VBO);

            const GLsizeiptr bytes = static_cast<GLsizeiptr>(verts.size() * sizeof(LineVert));
            if (bytes > m_CapacityBytes) {
                // grow
                m_CapacityBytes = bytes;
                glBufferData(GL_ARRAY_BUFFER, m_CapacityBytes, verts.data(), GL_DYNAMIC_DRAW);
            }
            else {
                glBufferSubData(GL_ARRAY_BUFFER, 0, bytes, verts.data());
            }

            // Lines render fine with the scene depth. You can disable depth for overlay effect.
            // glDisable(GL_DEPTH_TEST);
            glDisable(GL_CULL_FACE);
            glLineWidth(lineWidth);
            glDrawArrays(GL_LINES, 0, static_cast<GLint>(verts.size()));
            glLineWidth(1.0f);
            glEnable(GL_CULL_FACE);
            // glEnable(GL_DEPTH_TEST);

            glBindBuffer(GL_ARRAY_BUFFER, 0);
            glBindVertexArray(0);
            UnUse();
        }

    private:
        GLuint m_VAO = 0;
        GLuint m_VBO = 0;
        GLsizeiptr m_CapacityBytes = 0;

        GLint u_View = -1;
        GLint u_Proj = -1;
    };

} // namespace Boom