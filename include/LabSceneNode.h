#pragma once
#include "LabCore.h"
#include "LabMath.h"
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>

namespace Lab {

class Entity;
class Transform;

class LAB_CORE_API SceneNode : public std::enable_shared_from_this<SceneNode> {
public:
    SceneNode(const std::string& name = "Entity");
    virtual ~SceneNode() = default;

    const std::string& getName() const { return m_name; }
    void setName(const std::string& name) { m_name = name; }

    // Entity link
    void setEntity(Entity* entity) { m_entity = entity; }
    Entity* getEntity() const { return m_entity; }

    // Bidirectional Transform synchronization
    void syncFromTransform(const Transform& transform);
    void syncToTransform(Transform& transform) const;

    // Hierarchy management
    void setParent(std::shared_ptr<SceneNode> parent);
    std::shared_ptr<SceneNode> getParent() const;
    void addChild(std::shared_ptr<SceneNode> child);
    void removeChild(std::shared_ptr<SceneNode> child);
    const std::vector<std::shared_ptr<SceneNode>>& getChildren() const { return m_children; }

    // Local transformations
    void setLocalPosition(const Vec3& pos);
    void setLocalRotation(const Quat& rot);
    void setLocalScale(const Vec3& scale);

    const Vec3& getLocalPosition() const { return m_localPosition; }
    const Quat& getLocalRotation() const { return m_localRotation; }
    const Vec3& getLocalScale() const { return m_localScale; }

    // World transformations (cached with dirty flag)
    const Mat4& getWorldMatrix();
    Vec3 getWorldPosition();

    // Mark dirty recursively
    void setDirty();

    // Cloning / Prefab support
    virtual std::shared_ptr<SceneNode> clone(bool recursive = true) const;

protected:
    std::string m_name;
    Entity* m_entity = nullptr;
    std::weak_ptr<SceneNode> m_parent;
    std::vector<std::shared_ptr<SceneNode>> m_children;

    Vec3 m_localPosition{0.0f, 0.0f, 0.0f};
    Quat m_localRotation{0.0f, 0.0f, 0.0f, 1.0f}; // Identity
    Vec3 m_localScale{1.0f, 1.0f, 1.0f};

    Mat4 m_cachedWorldMatrix = Mat4::identity();
    bool m_isDirty = true;
};

/// @brief Prefab template allowing saving and multi-instantiation of node hierarchies.
class LAB_CORE_API Prefab {
public:
    Prefab(std::shared_ptr<SceneNode> rootTemplate);
    ~Prefab() = default;

    /// @brief Instantiates a brand new hierarchy clone from this prefab template.
    std::shared_ptr<SceneNode> instantiate() const;

private:
    std::shared_ptr<SceneNode> m_rootTemplate;
};

} // namespace Lab
