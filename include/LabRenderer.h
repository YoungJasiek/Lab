#pragma once
#include <vector>
#include <string>
#include "LabMath.h"
#include "LabCamera.h"

namespace Lab {

    struct Vertex {
        Vec3 position;
        Vec3 normal;
        Vec2 texCoords;
        Vec3 color;

        Vertex() : position{0, 0, 0}, normal{0, 1, 0}, texCoords{0, 0}, color{1, 1, 1} {}
        Vertex(Vec3 p, Vec3 n, Vec2 t, Vec3 c = { 1, 1, 1 })
            : position(p), normal(n), texCoords(t), color(c) {}
    };

    class SkinnedMesh;
    class FacialMesh;

    class Shader {
    public:
        Shader(const char* vertexSource, const char* fragmentSource);
        ~Shader();

        void use() const;
        void setMat4(const std::string& name, const Mat4& mat) const;
        void setMat4Array(const std::string& name, const Mat4* mats, int count) const;
        void setVec3(const std::string& name, const Vec3& vec) const;
        void setVec2(const std::string& name, const Vec2& vec) const;
        void setInt(const std::string& name, int value) const;
        void setFloat(const std::string& name, float value) const;

    private:
        unsigned int _id;
        void checkCompileErrors(unsigned int shader, std::string type);
    };

    class Texture {
    public:
        Texture(const std::string& path);
        Texture(const unsigned char* data, int width, int height, int channels);
        ~Texture();

        void bind(unsigned int slot = 0) const;
        void unbind() const;
        void updateData(const unsigned char* data, int width, int height, int channels);
        unsigned int getId() const { return _id; }

    private:
        unsigned int _id;
        int _width, _height, _channels;
    };

    class Mesh {
    public:
        Mesh(const std::vector<Vertex>& vertices, const std::vector<unsigned int>& indices);
        ~Mesh();

        static Mesh* loadSTL(const std::string& path);
        void draw() const;
        int getIndexCount() const { return _indexCount; }

    private:
        unsigned int _vao, _vbo, _ebo;
        int _indexCount;
    };

    class Skybox {
    public:
        Skybox(const std::vector<std::string>& faces);
        ~Skybox();

        void draw(const Camera& camera, const Mat4& projection) const;

    private:
        unsigned int _id;
        unsigned int _vao, _vbo;
        Shader* _shader;
    };

    class Renderer {
    public:
        static void init();
        static void shutdown();
        static void beginFrame(const Camera& camera);
        static void endFrame();
        
        static void beginViewModel();
        static void endViewModel(const Camera& camera);

        // Lighting & Shadows configuration
        static void setSunLight(const Vec3& direction, const Vec3& color, const Vec3& ambient);
        static void setFlashlight(const Vec3& pos, const Vec3& dir, const Vec3& color,
                                  float innerCone, float outerCone, float range, float intensity);
        static void disableFlashlight();
        static void setShadowMap(const Mat4& lightSpaceMatrix, unsigned int depthTexture);
        static void disableShadowMap();

        // Shadow depth pass rendering
        static void beginShadowDepthPass(const Mat4& lightSpaceMatrix);
        static void endShadowDepthPass();
        static void drawShadowCube(const Vec3& position, const Vec3& rotation, const Vec3& scale);
        static void drawShadowCube(const Vec3& position, const Vec3& size);
        static void drawShadowCube(const Mat4& modelTransform);
        static void drawShadowMesh(const Mesh& mesh, const Vec3& position, const Vec3& rotation, const Vec3& scale);
        static void drawShadowMesh(const Mesh& mesh, const Mat4& modelTransform);
        static void drawShadowSkinnedMesh(const SkinnedMesh& mesh, const Mat4& modelTransform, const std::vector<Mat4>& boneMatrices);

        // 3D Rendering
        static void drawCube(const Vec3& position, const Vec3& rotation, const Vec3& scale, const Vec3& color, const Texture* texture = nullptr, bool enableLighting = true, const Vec2& uvTiling = { 0.25f, 0.25f }, int uvMode = 1);
        static void drawCube(const Vec3& position, const Vec3& size, const Vec3& color, const Texture* texture, bool enableLighting = true, const Vec2& uvTiling = { 0.25f, 0.25f }, int uvMode = 1);
        static void drawCube(const Vec3& position, const Vec3& size, const Vec3& color, bool enableLighting = true);
        static void drawCube(const Mat4& modelTransform, const Vec3& color = { 1, 1, 1 }, const Texture* texture = nullptr, bool enableLighting = true, const Vec2& uvTiling = { 1.0f, 1.0f }, int uvMode = 1);
        static void drawWireCube(const Vec3& position, const Vec3& size, const Vec3& color);
        static void drawBoundingBox(const Vec3& min, const Vec3& max, const Vec3& color);
        static void drawMesh(const Mesh& mesh, const Vec3& position, const Vec3& rotation, const Vec3& scale, const Vec3& color = { 1, 1, 1 }, const Texture* texture = nullptr, bool enableLighting = true);
        static void drawMesh(const Mesh& mesh, const Mat4& modelTransform, const Vec3& color = { 1, 1, 1 }, const Texture* texture = nullptr, bool enableLighting = true);
        static void drawSkinnedMesh(const SkinnedMesh& mesh, const Mat4& modelTransform, const std::vector<Mat4>& boneMatrices, const Vec3& color = { 1, 1, 1 }, const Texture* texture = nullptr, bool enableLighting = true);
        static void drawFacialMesh(const FacialMesh& mesh, const Mat4& modelTransform, const Vec3& color = { 1, 1, 1 }, const Texture* texture = nullptr, bool enableLighting = true);
        static void drawBaseplate(float size, const Texture* texture = nullptr);
        static std::string resolveModelTexture(const std::string& modelPath, const std::string& fallbackTexture = "");

        // 2D/UI Rendering
        static void beginUI(int windowWidth, int windowHeight);
        static void endUI();
        static void drawRect(float x, float y, float w, float h, const Vec3& color);
        static void drawTextureRect(float x, float y, float w, float h, const Texture& texture, const Vec3& tint = { 1, 1, 1 });
        static void drawTextureRect(float x, float y, float w, float h, const Texture& texture, float u0, float v0, float u1, float v1, const Vec3& tint = { 1, 1, 1 });

    private:
        static Mat4 getTransform(const Vec3& pos, const Vec3& rot, const Vec3& scale);
        static void applyLightingAndShadowUniforms(Shader* shader);
        
        static Shader* _defaultShader;
        static Shader* _shadowDepthShader;
        static Shader* _uiShader;
        static Mesh* _cubeMesh;
        static Mesh* _quadMesh; // For baseplate
        static unsigned int _wireCubeVao, _wireCubeVbo, _wireCubeEbo;
        static unsigned int _uiVao, _uiVbo;
        
        static Mat4 _viewMatrix;
        static Mat4 _projMatrix;
        static Mat4 _uiProjMatrix;
        static Vec3 _cameraPos;
        static Vec3 _lightDir;
        static Vec3 _lightColor;
        static Vec3 _ambientColor;

        // Dynamic Spotlight & Shadows state
        static bool _enableSpotlight;
        static Vec3 _spotLightPos;
        static Vec3 _spotLightDir;
        static Vec3 _spotLightColor;
        static float _spotLightInnerCone;
        static float _spotLightOuterCone;
        static float _spotLightRange;
        static float _spotLightIntensity;

        static bool _enableShadows;
        static Mat4 _lightSpaceMatrix;
        static unsigned int _shadowDepthTexture;
    };
}
