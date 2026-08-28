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

        Vertex(Vec3 p, Vec3 n, Vec2 t, Vec3 c = { 1, 1, 1 })
            : position(p), normal(n), texCoords(t), color(c) {}
    };

    class Shader {
    public:
        Shader(const char* vertexSource, const char* fragmentSource);
        ~Shader();

        void use() const;
        void setMat4(const std::string& name, const Mat4& mat) const;
        void setVec3(const std::string& name, const Vec3& vec) const;
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

        // Lighting configuration
        static void setSunLight(const Vec3& direction, const Vec3& color, const Vec3& ambient);

        // 3D Rendering
        static void drawCube(const Vec3& position, const Vec3& rotation, const Vec3& scale, const Vec3& color, const Texture* texture = nullptr, bool enableLighting = true);
        static void drawCube(const Vec3& position, const Vec3& size, const Vec3& color, bool enableLighting = true);
        static void drawMesh(const Mesh& mesh, const Vec3& position, const Vec3& rotation, const Vec3& scale, const Vec3& color = { 1, 1, 1 }, const Texture* texture = nullptr, bool enableLighting = true);
        static void drawBaseplate(float size, const Texture* texture = nullptr);

        // 2D/UI Rendering
        static void beginUI(int windowWidth, int windowHeight);
        static void endUI();
        static void drawRect(float x, float y, float w, float h, const Vec3& color);

    private:
        static Mat4 getTransform(const Vec3& pos, const Vec3& rot, const Vec3& scale);
        
        static Shader* _defaultShader;
        static Shader* _uiShader;
        static Mesh* _cubeMesh;
        static Mesh* _quadMesh; // For baseplate
        static unsigned int _uiVao, _uiVbo;
        
        static Mat4 _viewMatrix;
        static Mat4 _projMatrix;
        static Mat4 _uiProjMatrix;
        static Vec3 _cameraPos;
        static Vec3 _lightDir;
        static Vec3 _lightColor;
        static Vec3 _ambientColor;
    };
}
