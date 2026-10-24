#pragma once

#include "LabMath.h"
#include "LabCamera.h"
#include <string>
#include <vector>

namespace Lab {
    class Skeleton;
}

namespace Lab::LFM {

    struct ViewportRect {
        float x = 0.0f;
        float y = 0.0f;
        float w = 0.0f;
        float h = 0.0f;
    };

    class DualViewport {
    public:
        DualViewport();

        void updateLayout(float totalX, float totalY, float totalW, float totalH);

        const ViewportRect& getPrimaryRect() const { return _primaryRect; }
        const ViewportRect& getSecondaryRect() const { return _secondaryRect; }

        Camera& getWorkCamera() { return _workCamera; }
        const Camera& getWorkCamera() const { return _workCamera; }

        Camera& getCinematicCamera() { return _cinematicCamera; }
        const Camera& getCinematicCamera() const { return _cinematicCamera; }

        void setCinematicCameraTransform(const Vec3& pos, float pitch, float yaw, float fov);
        void updateWorkCameraFlight(float dt, double mx, double my, bool rmb);

        // Visualizer helpers
        static void drawCameraFrustum(const Camera& targetCam, const Vec3& color = {0.95f, 0.85f, 0.20f});
        static void drawMotionPath(const std::vector<Vec3>& points, const Vec3& color = {0.20f, 0.90f, 0.45f});
        static void drawTransformGizmo(const Vec3& pos, float scale = 0.5f);
        static void drawSkeleton(const Skeleton& skeleton,
                                 const std::vector<Mat4>& globalTransforms,
                                 const Mat4& modelTransform,
                                 int selectedBoneIndex = -1);

    private:
        ViewportRect _primaryRect;
        ViewportRect _secondaryRect;

        Camera _workCamera;
        Camera _cinematicCamera;

        float _workPitch = -15.0f;
        float _workYaw = -45.0f;
        Vec3 _workPos{0.0f, 3.5f, 6.0f};

        double _lastMouseX = 0.0;
        double _lastMouseY = 0.0;
    };

} // namespace Lab::LFM
