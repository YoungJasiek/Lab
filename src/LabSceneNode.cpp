#include "LabSceneNode.h"
#include "LabTransform.h"
#include <algorithm>

namespace Lab {

SceneNode::SceneNode(const std::string& name)
    : m_name(name) {
}

void SceneNode::syncFromTransform(const Transform& transform) {
    m_localPosition = transform.position;
    m_localRotation = transform.rotation;
    m_localScale = transform.scale;
    setDirty();
}

void SceneNode::syncToTransform(Transform& transform) const {
    transform.position = m_localPosition;
    transform.rotation = m_localRotation;
    transform.localRotation = m_localRotation;
    transform.scale = m_localScale;
}

void SceneNode::setParent(std::shared_ptr<SceneNode> parent) {
    auto currentParent = m_parent.lock();
    if (currentParent == parent) return;

    if (currentParent) {
        currentParent->removeChild(shared_from_this());
    }

    m_parent = parent;
    if (parent) {
        parent->addChild(shared_from_this());
    }
    setDirty();
}

std::shared_ptr<SceneNode> SceneNode::getParent() const {
    return m_parent.lock();
}

void SceneNode::addChild(std::shared_ptr<SceneNode> child) {
    if (!child || child.get() == this) return;

    for (const auto& c : m_children) {
        if (c == child) return;
    }

    m_children.push_back(child);
    child->m_parent = shared_from_this();
    child->setDirty();
}

void SceneNode::removeChild(std::shared_ptr<SceneNode> child) {
    if (!child) return;

    auto it = std::find(m_children.begin(), m_children.end(), child);
    if (it != m_children.end()) {
        (*it)->m_parent.reset();
        (*it)->setDirty();
        m_children.erase(it);
    }
}

void SceneNode::setLocalPosition(const Vec3& pos) {
    m_localPosition = pos;
    setDirty();
}

void SceneNode::setLocalRotation(const Quat& rot) {
    m_localRotation = rot;
    setDirty();
}

void SceneNode::setLocalScale(const Vec3& scale) {
    m_localScale = scale;
    setDirty();
}

void SceneNode::setDirty() {
    m_isDirty = true;
    for (auto& child : m_children) {
        if (child) {
            child->setDirty();
        }
    }
}

const Mat4& SceneNode::getWorldMatrix() {
    if (m_isDirty) {
        Mat4 localTransform = Mat4::translate(m_localPosition) *
                              m_localRotation.toMat4() *
                              Mat4::scale(m_localScale);

        auto parent = m_parent.lock();
        if (parent) {
            m_cachedWorldMatrix = parent->getWorldMatrix() * localTransform;
        } else {
            m_cachedWorldMatrix = localTransform;
        }
        m_isDirty = false;
    }
    return m_cachedWorldMatrix;
}

Vec3 SceneNode::getWorldPosition() {
    const Mat4& m = getWorldMatrix();
    return Vec3(m.m[12], m.m[13], m.m[14]);
}

std::shared_ptr<SceneNode> SceneNode::clone(bool recursive) const {
    auto copy = std::make_shared<SceneNode>(m_name);
    copy->m_localPosition = m_localPosition;
    copy->m_localRotation = m_localRotation;
    copy->m_localScale = m_localScale;
    copy->m_isDirty = true;

    if (recursive) {
        for (const auto& child : m_children) {
            if (child) {
                auto childClone = child->clone(true);
                copy->addChild(childClone);
            }
        }
    }
    return copy;
}

Prefab::Prefab(std::shared_ptr<SceneNode> rootTemplate)
    : m_rootTemplate(rootTemplate) {
}

std::shared_ptr<SceneNode> Prefab::instantiate() const {
    if (!m_rootTemplate) return nullptr;
    return m_rootTemplate->clone(true);
}

} // namespace Lab
