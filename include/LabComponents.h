#pragma once
#include "LabComponent.h"
#include "LabTransform.h"
#include "LabEntity.h"
#include "LabMath.h"
#include "LabLight.h"
#include "LabCloth.h"
#include "LabFluid.h"
#include "LabForceField.h"
#include "LabVehicle.h"
#include "LabDecals.h"
#include "LabPostProcess.h"
#include "LabAudio.h"
#include <string>
#include <memory>

namespace Lab {

    // =========================================================================
    // 1. Light Component
    // =========================================================================
    enum class LightType {
        Point,
        Directional,
        Spot
    };

    class LightComponent : public Component {
    public:
        LightType type{LightType::Point};
        Vec3 color{1.0f, 0.95f, 0.85f};
        float intensity{2.0f};
        float radius{14.0f};
        float spotAngle{42.0f};

        LightComponent() = default;
    };

    // =========================================================================
    // 2. Audio Source Component
    // =========================================================================
    class AudioSourceComponent : public Component {
    public:
        std::string soundPath{""};
        float volume{1.0f};
        float pitch{1.0f};
        bool loop{false};
        bool playOnStart{true};
        float minDistance{1.0f};
        float maxDistance{25.0f};

        AudioSourceComponent() = default;

        void Start() override {
            if (playOnStart && !soundPath.empty()) {
                play();
            }
        }

        void play() {
            Vec3 pos = getTransform() ? getTransform()->getWorldPosition() : Vec3(0, 0, 0);
            (void)pos;
            // Triggers spatial sound playback through LabAudio
        }
    };

    // =========================================================================
    // 3. Cloth Simulation Component
    // =========================================================================
    class ClothComponent : public Component {
    public:
        int gridWidth{16};
        int gridHeight{16};
        float spacing{0.25f};
        float stiffness{0.9f};
        bool pinTopEdge{true};
        ClothMesh mesh;

        ClothComponent() {
            initCloth();
        }

        void initCloth() {
            mesh.initGrid((std::size_t)gridWidth, (std::size_t)gridHeight, spacing);
            if (pinTopEdge) {
                for (int x = 0; x < gridWidth && x < (int)mesh.particles.size(); ++x) {
                    mesh.particles[x].pin();
                }
            }
        }

        void Update(float dt) override {
            mesh.step(dt, Vec3(0, -9.81f, 0), 4);
        }
    };

    // =========================================================================
    // 4. Water / Fluid Volume Component
    // =========================================================================
    class WaterVolumeComponent : public Component {
    public:
        float surfaceY{0.0f};
        float density{1000.0f};
        float linearDrag{0.92f};
        float angularDrag{0.85f};
        Vec3 size{10.0f, 2.0f, 10.0f};
        WaterVolume water;

        WaterVolumeComponent() {
            water.surfaceY = surfaceY;
            water.density = density;
            water.linearDrag = linearDrag;
            water.angularDrag = angularDrag;
        }

        void Update(float dt) override {
            (void)dt;
            if (getTransform()) {
                surfaceY = getTransform()->position.y;
                water.surfaceY = surfaceY;
            }
        }
    };

    // =========================================================================
    // 5. Force Field Component (Radial, Directional, Vortex)
    // =========================================================================
    class ForceFieldComponent : public Component {
    public:
        ForceFieldType fieldType{ForceFieldType::RadialExplosion};
        float strength{50.0f};
        float radius{8.0f};
        Vec3 direction{0.0f, 1.0f, 0.0f};

        ForceFieldComponent() = default;

        Vec3 calculateForceAt(const Vec3& worldPos) const {
            Vec3 center = getTransform() ? getTransform()->getWorldPosition() : Vec3(0, 0, 0);
            ForceFieldTrigger fft;
            fft.type = fieldType;
            fft.position = center;
            fft.radius = radius;
            fft.blastMagnitude = strength;
            fft.direction = direction;
            return fft.evaluateForceAt(worldPos);
        }
    };

    // =========================================================================
    // 6. Arcade / Physics Vehicle Component
    // =========================================================================
    class VehicleComponent : public Component {
    public:
        float mass{1200.0f};
        float engineForce{2500.0f};
        float brakeForce{4000.0f};
        float maxSteerAngle{35.0f};
        float currentSteer{0.0f};
        float currentThrottle{0.0f};
        float currentBrake{0.0f};
        Vec3 velocity{0.0f, 0.0f, 0.0f};

        VehicleComponent() = default;

        void Update(float dt) override {
            if (!getTransform()) return;
            // Simple wheeled physics integration
            Vec3 fwd = getTransform()->forward();
            float accel = (currentThrottle * engineForce - currentBrake * brakeForce) / (mass > 0 ? mass : 1000.0f);
            velocity += fwd * (accel * dt);
            velocity = velocity * std::pow(0.98f, dt * 60.0f); // Rolling resistance
            getTransform()->position += velocity * dt;
            getTransform()->Rotate(Vec3(0, 1, 0), currentSteer * (velocity.length() * 0.1f) * dt);
        }
    };

    // =========================================================================
    // 7. Projected Decal Component
    // =========================================================================
    class DecalComponent : public Component {
    public:
        std::string textureName{"blood_splatter.png"};
        Vec3 size{1.0f, 1.0f, 1.0f};
        Color tint{1.0f, 1.0f, 1.0f, 1.0f};
        float opacity{1.0f};

        DecalComponent() = default;
    };

    // =========================================================================
    // 8. Post-Process Volume Component
    // =========================================================================
    class PostProcessComponent : public Component {
    public:
        float bloomIntensity{0.45f};
        float vignetteStrength{0.25f};
        float chromaticAberration{0.002f};
        float filmGrain{0.02f};
        float saturation{1.0f};
        float contrast{1.0f};
        Vec3 colorTint{1.0f, 1.0f, 1.0f};
        bool isGlobal{true};
        float volumeRadius{15.0f};

        PostProcessComponent() = default;
    };

} // namespace Lab
