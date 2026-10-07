#pragma once
#include "LabExport.h"

namespace Lab {

    class Entity;
    class Transform;

    /// @brief Base class for all scene entity components in Lab Engine
    class Component {
    public:
        Entity* entity{nullptr};
        bool enabled{true};

        virtual ~Component() = default;

        virtual void Awake() {}
        virtual void Start() {}
        virtual void Update(float dt) { (void)dt; }
        virtual void LateUpdate(float dt) { (void)dt; }
        virtual void OnDestroy() {}

        Transform* getTransform() const;
    };

} // namespace Lab
