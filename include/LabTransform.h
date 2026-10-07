#pragma once
#include "LabComponent.h"
#include "LabMath.h"
#include <vector>
#include <algorithm>

namespace Lab {

    /// @brief Transform component managing position, orientation, scale and hierarchy
    class Transform : public Component {
    public:
        Vec3 position{0.0f, 0.0f, 0.0f};
        Quat rotation{0.0f, 0.0f, 0.0f, 1.0f};
        Quat localRotation{0.0f, 0.0f, 0.0f, 1.0f};
        Vec3 scale{1.0f, 1.0f, 1.0f};

        Transform* parent{nullptr};
        std::vector<Transform*> children;

        Transform() = default;
        virtual ~Transform() {
            if (parent) {
                parent->removeChild(this);
            }
            for (auto* child : children) {
                if (child) child->parent = nullptr;
            }
        }

        void setParent(Transform* newParent) {
            if (parent) parent->removeChild(this);
            parent = newParent;
            if (parent) parent->children.push_back(this);
        }

        void removeChild(Transform* child) {
            auto it = std::find(children.begin(), children.end(), child);
            if (it != children.end()) children.erase(it);
        }

        Vec3 forward() const {
            return rotation.rotateVector(Vec3(0.0f, 0.0f, -1.0f)).normalized();
        }

        Vec3 right() const {
            return rotation.rotateVector(Vec3(1.0f, 0.0f, 0.0f)).normalized();
        }

        Vec3 up() const {
            return rotation.rotateVector(Vec3(0.0f, 1.0f, 0.0f)).normalized();
        }

        void LookAt(const Vec3& target, const Vec3& worldUp = Vec3(0.0f, 1.0f, 0.0f)) {
            (void)worldUp;
            Vec3 dir = target - position;
            if (dir.lengthSq() < 1e-6f) return;
            dir = dir.normalized();

            float pitch = std::asin(std::clamp(dir.y, -1.0f, 1.0f));
            float yaw = std::atan2(-dir.x, -dir.z);
            rotation = Quat::fromEuler(pitch, yaw, 0.0f);
            localRotation = rotation;
        }

        void Rotate(const Vec3& axis, float angleDegrees) {
            float rad = angleDegrees * (3.14159265f / 180.0f);
            Quat delta = Quat::fromAxisAngle(axis, rad);
            rotation = (delta * rotation).normalized();
            localRotation = rotation;
        }

        Mat4 getLocalMatrix() const {
            return makeTransform(position, rotation, scale);
        }

        Mat4 getWorldMatrix() const {
            if (parent) {
                return parent->getWorldMatrix() * getLocalMatrix();
            }
            return getLocalMatrix();
        }

        Vec3 getWorldPosition() const {
            if (parent) {
                Mat4 wm = getWorldMatrix();
                return Vec3(wm.m[12], wm.m[13], wm.m[14]);
            }
            return position;
        }
    };

} // namespace Lab
