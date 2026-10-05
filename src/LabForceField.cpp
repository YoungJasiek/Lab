#include "LabForceField.h"

namespace Lab {

    Vec3 ForceFieldTrigger::evaluateForceAt(const Vec3& targetPos, float targetMass) const {
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

    bool ForceFieldTrigger::containsPoint(const Vec3& pt) const {
        return (pt - position).length() <= radius;
    }

    void ForceFieldTrigger::update(float dt) {
        elapsedTime += dt;
    }

    void ForceFieldManager::addField(const ForceFieldTrigger& field) {
        fields.push_back(field);
    }

    void ForceFieldManager::clear() {
        fields.clear();
    }

    void ForceFieldManager::update(float dt) {
        for (auto& f : fields) {
            f.update(dt);
        }
    }

    Vec3 ForceFieldManager::sampleTotalForceAt(const Vec3& targetPos, float targetMass) const {
        Vec3 totalForce(0, 0, 0);
        for (const auto& f : fields) {
            totalForce += f.evaluateForceAt(targetPos, targetMass);
        }
        return totalForce;
    }

    float ForceFieldManager::sampleDampingMultiplierAt(const Vec3& targetPos) const {
        float multiplier = 1.0f;
        for (const auto& f : fields) {
            if (f.type == ForceFieldType::FrictionVolume && f.containsPoint(targetPos)) {
                multiplier *= f.linearDampingMultiplier;
            }
        }
        return multiplier;
    }

} // namespace Lab
