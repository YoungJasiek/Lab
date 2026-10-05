#pragma once
#include <string>
#include <vector>
#include <cmath>
#include <algorithm>
#include "LabMath.h"

namespace Lab {

    struct RagdollBone {
        std::string name;
        int parentIndex{-1};
        Vec3 position{0, 0, 0};
        Vec3 velocity{0, 0, 0};
        Quat rotation{Quat::identity()};
        Vec3 angularVelocity{0, 0, 0};
        float mass{5.0f};
        float length{0.5f};
        float radius{0.1f};
        bool isKinematic{true};
    };

    struct RagdollJoint {
        int parentBone{-1};
        int childBone{-1};
        float targetDistance{0.5f};
        float minAngleRad{-1.0f};
        float maxAngleRad{1.0f};
    };

    class Ragdoll {
    public:
        Ragdoll() = default;

        int addBone(const std::string& name, int parentIndex, const Vec3& localPos, float mass = 5.0f, float length = 0.5f, float radius = 0.1f);
        void activateRagdoll(const Vec3& initialImpulse = Vec3(0, 0, 0));
        void setKinematic(bool kinematic);
        bool isActive() const { return _isActive; }

        void step(float dt, const Vec3& gravity = Vec3(0.0f, -9.81f, 0.0f));

        const std::vector<RagdollBone>& getBones() const { return _bones; }
        std::vector<RagdollBone>& getBones() { return _bones; }
        const std::vector<RagdollJoint>& getJoints() const { return _joints; }

    private:
        std::vector<RagdollBone> _bones;
        std::vector<RagdollJoint> _joints;
        bool _isActive{false};
    };

} // namespace Lab
