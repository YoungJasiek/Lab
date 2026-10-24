#include "LFM/LFMDualViewport.h"
#include "LabRenderer.h"
#include "LabInput.h"
#include "LabSkeletal.h"
#include <GLFW/glfw3.h>
#include <cmath>
#include <algorithm>

namespace Lab::LFM {

    DualViewport::DualViewport() {
        _cinematicCamera.fieldOfView = 75.0f;
        _cinematicCamera.setPosition(Vec3(-12.0f, 3.5f, 8.0f));
        _cinematicCamera.setPitch(-12.0f);
        _cinematicCamera.setYaw(-40.0f);

        _workCamera.fieldOfView = 70.0f;
        _workCamera.setPosition(_workPos);
        _workCamera.setPitch(_workPitch);
        _workCamera.setYaw(_workYaw);
    }

    void DualViewport::updateLayout(float totalX, float totalY, float totalW, float totalH) {
        float gap = 4.0f;
        float halfW = (totalW - gap) * 0.5f;

        // Primary (Left) Viewport
        _primaryRect.x = totalX;
        _primaryRect.y = totalY;
        _primaryRect.w = halfW;
        _primaryRect.h = totalH;

        // Secondary (Right) Viewport
        _secondaryRect.x = totalX + halfW + gap;
        _secondaryRect.y = totalY;
        _secondaryRect.w = halfW;
        _secondaryRect.h = totalH;
    }

    void DualViewport::setCinematicCameraTransform(const Vec3& pos, float pitch, float yaw, float fov) {
        _cinematicCamera.setPosition(pos);
        _cinematicCamera.setPitch(pitch);
        _cinematicCamera.setYaw(yaw);
        _cinematicCamera.fieldOfView = fov;
    }

    void DualViewport::updateWorkCameraFlight(float dt, double mx, double my, bool rmb) {
        if (rmb) {
            float camSpeed = 10.0f * dt;
            if (Input::isKeyPressed(GLFW_KEY_LEFT_SHIFT)) camSpeed *= 2.5f;

            float radYaw = _workYaw * (3.14159265f / 180.0f);
            Vec3 forward(std::cos(radYaw), 0.0f, std::sin(radYaw));
            Vec3 right(-std::sin(radYaw), 0.0f, std::cos(radYaw));

            if (Input::isKeyPressed(GLFW_KEY_W)) _workPos += forward * camSpeed;
            if (Input::isKeyPressed(GLFW_KEY_S)) _workPos -= forward * camSpeed;
            if (Input::isKeyPressed(GLFW_KEY_D)) _workPos += right * camSpeed;
            if (Input::isKeyPressed(GLFW_KEY_A)) _workPos -= right * camSpeed;
            if (Input::isKeyPressed(GLFW_KEY_E)) _workPos.y += camSpeed;
            if (Input::isKeyPressed(GLFW_KEY_Q)) _workPos.y -= camSpeed;

            float dmx = static_cast<float>(mx - _lastMouseX);
            float dmy = static_cast<float>(my - _lastMouseY);
            _workYaw += dmx * 0.22f;
            _workPitch -= dmy * 0.22f;
            _workPitch = std::clamp(_workPitch, -88.0f, 88.0f);

            _workCamera.setPosition(_workPos);
            _workCamera.setPitch(_workPitch);
            _workCamera.setYaw(_workYaw);
        }
        _lastMouseX = mx;
        _lastMouseY = my;
    }

    void DualViewport::drawCameraFrustum(const Camera& targetCam, const Vec3& color) {
        Vec3 camPos = targetCam.getPosition();
        Vec3 camFront = targetCam.getFront();
        Vec3 camRight = targetCam.getRight();
        Vec3 camUp = targetCam.getUp();

        // Draw camera body box
        Renderer::drawCube(camPos, Vec3(0.4f, 0.3f, 0.5f), color, nullptr, true);

        // Calculate 4 corners of frustum pyramid at distance 2.5m
        float dist = 2.5f;
        float fovRad = targetCam.fieldOfView * (3.14159265f / 180.0f);
        float hHeight = std::tan(fovRad * 0.5f) * dist;
        float hWidth = hHeight * (16.0f / 9.0f);

        Vec3 center = camPos + camFront * dist;
        Vec3 topLeft  = center - camRight * hWidth + camUp * hHeight;
        Vec3 topRight = center + camRight * hWidth + camUp * hHeight;
        Vec3 botLeft  = center - camRight * hWidth - camUp * hHeight;
        Vec3 botRight = center + camRight * hWidth - camUp * hHeight;

        // Draw pyramid corner markers
        Renderer::drawCube(topLeft, Vec3(0.08f, 0.08f, 0.08f), color, nullptr, true);
        Renderer::drawCube(topRight, Vec3(0.08f, 0.08f, 0.08f), color, nullptr, true);
        Renderer::drawCube(botLeft, Vec3(0.08f, 0.08f, 0.08f), color, nullptr, true);
        Renderer::drawCube(botRight, Vec3(0.08f, 0.08f, 0.08f), color, nullptr, true);

        // Draw pyramid edges from camera position
        Renderer::drawAdditiveTracer(camPos, topLeft, color, 0.016f, 0.85f);
        Renderer::drawAdditiveTracer(camPos, topRight, color, 0.016f, 0.85f);
        Renderer::drawAdditiveTracer(camPos, botLeft, color, 0.016f, 0.85f);
        Renderer::drawAdditiveTracer(camPos, botRight, color, 0.016f, 0.85f);

        // Draw far frame rectangle
        Renderer::drawAdditiveTracer(topLeft, topRight, color, 0.016f, 0.85f);
        Renderer::drawAdditiveTracer(topRight, botRight, color, 0.016f, 0.85f);
        Renderer::drawAdditiveTracer(botRight, botLeft, color, 0.016f, 0.85f);
        Renderer::drawAdditiveTracer(botLeft, topLeft, color, 0.016f, 0.85f);
    }

    void DualViewport::drawMotionPath(const std::vector<Vec3>& points, const Vec3& color) {
        if (points.empty()) return;

        for (size_t i = 0; i < points.size(); ++i) {
            // Draw keyframe node marker
            Renderer::drawCube(points[i], Vec3(0.12f, 0.12f, 0.12f), Vec3(0.95f, 0.85f, 0.20f), nullptr, true);

            // Draw line connecting to next keyframe
            if (i + 1 < points.size()) {
                Renderer::drawAdditiveTracer(points[i], points[i + 1], color, 0.024f, 0.90f);
            }
        }
    }

    void DualViewport::drawTransformGizmo(const Vec3& pos, float scale) {
        // Center core
        Renderer::drawCube(pos, Vec3(0.04f, 0.04f, 0.04f), Vec3(0.95f, 0.95f, 0.95f), nullptr, false);

        // X Axis (Red)
        Renderer::drawCube(pos + Vec3(scale * 0.5f, 0.0f, 0.0f), Vec3(scale, 0.02f, 0.02f), Vec3(0.95f, 0.22f, 0.22f), nullptr, false);
        Renderer::drawCube(pos + Vec3(scale, 0.0f, 0.0f), Vec3(0.06f, 0.06f, 0.06f), Vec3(1.0f, 0.15f, 0.15f), nullptr, false);

        // Y Axis (Green)
        Renderer::drawCube(pos + Vec3(0.0f, scale * 0.5f, 0.0f), Vec3(0.02f, scale, 0.02f), Vec3(0.22f, 0.95f, 0.22f), nullptr, false);
        Renderer::drawCube(pos + Vec3(0.0f, scale, 0.0f), Vec3(0.06f, 0.06f, 0.06f), Vec3(0.15f, 1.0f, 0.15f), nullptr, false);

        // Z Axis (Blue)
        Renderer::drawCube(pos + Vec3(0.0f, 0.0f, scale * 0.5f), Vec3(0.02f, 0.02f, scale), Vec3(0.25f, 0.45f, 0.98f), nullptr, false);
        Renderer::drawCube(pos + Vec3(0.0f, 0.0f, scale), Vec3(0.06f, 0.06f, 0.06f), Vec3(0.25f, 0.45f, 1.0f), nullptr, false);
    }

    void DualViewport::drawSkeleton(const Skeleton& skeleton,
                                    const std::vector<Mat4>& globalTransforms,
                                    const Mat4& modelTransform,
                                    int selectedBoneIndex) {
        if (globalTransforms.size() != skeleton.getBoneCount()) return;

        for (size_t i = 0; i < skeleton.getBoneCount(); ++i) {
            const Bone* bone = skeleton.getBone(static_cast<int>(i));
            if (!bone) continue;

            Mat4 boneWorld = modelTransform * globalTransforms[i];
            Vec3 bonePos(boneWorld.m[12], boneWorld.m[13], boneWorld.m[14]);
            bool isSelected = (static_cast<int>(i) == selectedBoneIndex);

            // Bone link to parent
            if (bone->parentIndex >= 0 && static_cast<size_t>(bone->parentIndex) < globalTransforms.size()) {
                Mat4 parentWorld = modelTransform * globalTransforms[bone->parentIndex];
                Vec3 parentPos(parentWorld.m[12], parentWorld.m[13], parentWorld.m[14]);

                Vec3 linkColor = isSelected ? Vec3(1.0f, 0.85f, 0.20f) : Vec3(0.35f, 0.65f, 0.85f);
                float thickness = isSelected ? 0.020f : 0.008f;
                Renderer::drawAdditiveTracer(parentPos, bonePos, linkColor, thickness, 0.9f);
            }

            // Joint node
            if (isSelected) {
                Renderer::drawCube(bonePos, Vec3(0.045f, 0.045f, 0.045f), Vec3(1.0f, 0.85f, 0.20f), nullptr, false);
                drawTransformGizmo(bonePos, 0.38f);
            } else {
                Renderer::drawCube(bonePos, Vec3(0.020f, 0.020f, 0.020f), Vec3(0.40f, 0.75f, 0.95f), nullptr, false);
            }
        }
    }

} // namespace Lab::LFM
