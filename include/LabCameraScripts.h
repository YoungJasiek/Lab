#pragma once
#include "LabComponent.h"
#include "LabTransform.h"
#include "LabEntity.h"
#include "LabInput.h"
#include <algorithm>

namespace Lab {

    /// @brief Smooth camera follow script running in LateUpdate()
    class CameraFollow : public Component {
    public:
        Transform* target{nullptr};
        Vec3 offset{0.0f, 5.0f, -10.0f};
        float smoothSpeed{5.0f};

        void LateUpdate(float dt) override {
            if (!target || !entity || !entity->transform) return;
            Transform* transform = entity->transform.get();
            Vec3 desired = target->position + offset;
            float t = std::min(smoothSpeed * dt, 1.0f);
            transform->position = Vec3::Lerp(transform->position, desired, t);
            transform->LookAt(target->position);
        }
    };

    /// @brief First-person look script running in Update()
    class FPSLook : public Component {
    public:
        float mouseSensitivity{100.0f};
        float xRotation{0.0f};
        Transform* parent{nullptr};

        void Update(float dt) override {
            if (!entity || !entity->transform) return;
            float mouseX = Input::GetAxis("MouseX") * mouseSensitivity * dt;
            float mouseY = Input::GetAxis("MouseY") * mouseSensitivity * dt;

            xRotation -= mouseY;
            xRotation = std::clamp(xRotation, -90.0f, 90.0f);

            Transform* transform = entity->transform.get();
            transform->localRotation = Quat::FromEuler(xRotation, 0.0f, 0.0f);
            transform->rotation = transform->localRotation;
            if (parent) {
                parent->Rotate(Vec3(0.0f, 1.0f, 0.0f), mouseX);
            }
        }
    };

} // namespace Lab
