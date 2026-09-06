#pragma once
#include <vector>
#include <string>
#include <memory>
#include "LabMath.h"
#include "LabRenderer.h"
#include "LabMap.h"

namespace Lab {

    enum class DecalType {
        BulletHoleConcrete = 0,
        BulletHoleMetal    = 1,
        BloodSplatter      = 2,
        ExplosiveScorch    = 3
    };

    struct DecalInstance {
        DecalType type = DecalType::BulletHoleConcrete;
        Vec3 position{ 0.0f, 0.0f, 0.0f };
        Vec3 normal{ 0.0f, 1.0f, 0.0f };
        float size = 0.25f;
        float lifetime = 0.0f;
        float maxLifetime = 60.0f;
        float alpha = 1.0f;

        std::vector<Vertex> vertices;
        std::vector<unsigned int> indices;
    };

    class DecalSystem {
    public:
        DecalSystem();
        ~DecalSystem();

        void init();
        void shutdown();

        // Spawns a decal at world position facing normal
        void spawnDecal(DecalType type, const Vec3& position, const Vec3& normal, float size = 0.25f, float maxLifetime = 60.0f);

        // Projects an explosion scorch decal onto nearby surfaces
        void spawnExplosionScorch(const Vec3& center, float radius = 2.4f, const std::vector<MapBrush>& brushes = {});

        // Updates decal lifetimes and alpha fadeouts
        void update(float dt);

        // Renders all active decals using batched VAO/VBO in OpenGL 4.5 Core Profile
        void render(const Camera& cam);

        // Clears all decals
        void clear();

        size_t getDecalCount() const { return _decals.size(); }
        const std::vector<DecalInstance>& getDecals() const { return _decals; }

    private:
        std::vector<DecalInstance> _decals;
        size_t _maxDecals = 256;

        std::unique_ptr<Texture> _texConcreteHole;
        std::unique_ptr<Texture> _texMetalHole;
        std::unique_ptr<Texture> _texBlood;
        std::unique_ptr<Texture> _texScorch;

        unsigned int _vao = 0;
        unsigned int _vbo = 0;
        unsigned int _ebo = 0;
        std::unique_ptr<Shader> _decalShader;

        bool _initialized = false;

        void buildDecalGeometry(DecalInstance& decal);
        void initGPU();
        void initTextures();
    };

} // namespace Lab
