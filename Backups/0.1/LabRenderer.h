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

    class Texture {
    public:
        Texture(const std::string& path);
        Texture(const unsigned char* data, int width, int height, int channels);
        ~Texture();

        void bind() const;
        void unbind() const;

    private:
        unsigned int _id;
        int _width, _height, _channels;
    };

    class Mesh {
    public:
        Mesh(const std::vector<Vertex>& vertices, const std::vector<unsigned int>& indices)
            : _vertices(vertices), _indices(indices) {}

        void draw() const;

    private:
        std::vector<Vertex> _vertices;
        std::vector<unsigned int> _indices;
    };

    class Skybox {
    public:
        Skybox(const std::vector<std::string>& faces);
        ~Skybox();

        void draw(const Camera& camera) const;

    private:
        unsigned int _id;
        unsigned int _vao, _vbo;
    };

    class Renderer {
    public:
        static void init();
        static void beginFrame(const Camera& camera);
        static void endFrame();

        // 3D Rendering
        static void drawCube(const Vec3& position, const Vec3& size, const Vec3& color);
        static void drawMesh(const Mesh& mesh, const Vec3& position, const Vec3& rotation, const Vec3& scale);
        static void drawBaseplate(float size, const Texture* texture = nullptr);

        // 2D/UI Rendering
        static void beginUI(int windowWidth, int windowHeight);
        static void endUI();
        static void drawRect(float x, float y, float w, float h, const Vec3& color);

    private:
        static void applyTransform(const Vec3& pos, const Vec3& rot, const Vec3& scale);
    };
}
