#pragma once

#include "LabMath.h"
#include <vector>
#include <string>
#include <cmath>
#include <algorithm>

namespace Lab {

    enum class ForceFieldType {
        RadialExplosion,  // Shockwave blast decaying with distance squared
        DirectionalWind,  // Linear directional air current with turbulence
        PointGravity,     // Spherical planetary attractor (Newtonian Gm/r^2)
        FrictionVolume    // Surface modifier (e.g. ice, mud) changing friction/damping
    };

    struct ForceFieldTrigger {
        std::string name{"ForceField"};
        ForceFieldType type{ForceFieldType::RadialExplosion};
        Vec3 position{0, 0, 0};
        float radius{10.0f};

        // Radial explosion parameters
        float blastMagnitude{1000.0f}; // Peak impulse / force
        float shockwaveSpeed{50.0f};    // Expanding blast front (m/s)
        float elapsedTime{0.0f};

        // Directional wind parameters
        Vec3 direction{0, 0, 1};
        float windSpeed{15.0f};
        float turbulence{0.0f}; // Random variation

        // Planetary / point gravity parameters
        float gravityStrength{9.81f}; // Acceleration at unit distance or GM

        // Friction / viscosity parameters
        float linearDampingMultiplier{1.0f};

        // Evaluate acceleration / force at target point
        Vec3 evaluateForceAt(const Vec3& targetPos, float targetMass = 1.0f) const {
            Vec3 diff = targetPos - position;
            float dist = diff.length();

            switch (type) {
                case ForceFieldType::RadialExplosion: {
                    if (dist > radius || dist < 1e-4f) return Vec3(0, 0, 0);

                    // Expanding shockwave ring
                    float currentWaveDist = shockwaveSpeed * elapsedTime;
                    float waveDistDiff = std::abs(dist - currentWaveDist);
                    float waveFactor = (waveDistDiff < 2.0f) ? 2.5f : 1.0f;

                    // Falloff 1 / (1 + r^2)
                    float falloff = 1.0f / (1.0f + (dist / radius) * (dist / radius));
                    Vec3 dir = diff / dist;
                    float forceMag = blastMagnitude * falloff * waveFactor;
                    return dir * forceMag;
                }
                case ForceFieldType::DirectionalWind: {
                    if (dist > radius) return Vec3(0, 0, 0);
                    Vec3 baseDir = direction.normalized();
                    return baseDir * (windSpeed * targetMass);
                }
                case ForceFieldType::PointGravity: {
                    if (dist > radius || dist < 1e-3f) return Vec3(0, 0, 0);
                    // Newtonian gravity towards center: a = -G / r^2 * dir
                    Vec3 dir = (position - targetPos) / dist;
                    float falloff = gravityStrength / (1.0f + dist * dist);
                    return dir * (falloff * targetMass);
                }
                case ForceFieldType::FrictionVolume: {
                    return Vec3(0, 0, 0); // Modifies damping rather than adding push force
                }
            }
            return Vec3(0, 0, 0);
        }

        bool containsPoint(const Vec3& pt) const {
            return (pt - position).length() <= radius;
        }

        void update(float dt) {
            elapsedTime += dt;
        }
    };

    class ForceFieldManager {
    public:
        std::vector<ForceFieldTrigger> fields;

        void addField(const ForceFieldTrigger& field) {
            fields.push_back(field);
        }

        void clear() {
            fields.clear();
        }

        void update(float dt) {
            for (auto& f : fields) {
                f.update(dt);
            }
        }

        Vec3 sampleTotalForceAt(const Vec3& targetPos, float targetMass = 1.0f) const {
            Vec3 totalForce(0, 0, 0);
            for (const auto& f : fields) {
                totalForce += f.evaluateForceAt(targetPos, targetMass);
            }
            return totalForce;
        }

        float sampleDampingMultiplierAt(const Vec3& targetPos) const {
            float multiplier = 1.0f;
            for (const auto& f : fields) {
                if (f.type == ForceFieldType::FrictionVolume && f.containsPoint(targetPos)) {
                    multiplier *= f.linearDampingMultiplier;
                }
            }
            return multiplier;
        }
    };

} // namespace Lab
