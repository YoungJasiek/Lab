#include "LabRagdoll.h"

namespace Lab {

    int Ragdoll::addBone(const std::string& name, int parentIndex, const Vec3& localPos, float mass, float length, float radius) {
        RagdollBone bone;
        bone.name = name;
        bone.parentIndex = parentIndex;
        bone.position = localPos;
        bone.mass = mass;
        bone.length = length;
        bone.radius = radius;
        bone.isKinematic = true;

        int idx = static_cast<int>(_bones.size());
        _bones.push_back(bone);

        if (parentIndex >= 0 && parentIndex < idx) {
            RagdollJoint joint;
            joint.parentBone = parentIndex;
            joint.childBone = idx;
            joint.targetDistance = (localPos - _bones[parentIndex].position).length();
            if (joint.targetDistance < 0.001f) joint.targetDistance = length;
            _joints.push_back(joint);
        }

        return idx;
    }

    void Ragdoll::activateRagdoll(const Vec3& initialImpulse) {
        _isActive = true;
        for (auto& bone : _bones) {
            bone.isKinematic = false;
            bone.velocity += initialImpulse * (1.0f / bone.mass);
        }
    }

    void Ragdoll::setKinematic(bool kinematic) {
        _isActive = !kinematic;
        for (auto& bone : _bones) {
            bone.isKinematic = kinematic;
        }
    }

    void Ragdoll::step(float dt, const Vec3& gravity) {
        if (!_isActive || dt <= 0.0f) return;

        // 1. Integrate bone positions & velocities
        for (auto& bone : _bones) {
            if (bone.isKinematic) continue;

            bone.velocity += gravity * dt;
            bone.velocity *= 0.995f; // Air damping
            bone.position += bone.velocity * dt;

            // Simple ground collision at Y = 0
            if (bone.position.y < bone.radius) {
                bone.position.y = bone.radius;
                if (bone.velocity.y < 0.0f) {
                    bone.velocity.y = -bone.velocity.y * 0.2f; // restitution
                    bone.velocity.x *= 0.8f; // friction
                    bone.velocity.z *= 0.8f;
                }
            }
        }

        // 2. Solve Distance Joint Constraints (Verlet relaxation)
        for (int iter = 0; iter < 4; ++iter) {
            for (const auto& joint : _joints) {
                auto& parent = _bones[joint.parentBone];
                auto& child = _bones[joint.childBone];

                Vec3 delta = child.position - parent.position;
                float dist = delta.length();
                if (dist > 0.0001f) {
                    float diff = (dist - joint.targetDistance) / dist;
                    Vec3 correction = delta * (0.5f * diff);

                    if (!parent.isKinematic) parent.position += correction;
                    if (!child.isKinematic) child.position -= correction;
                }
            }
        }
    }

} // namespace Lab
