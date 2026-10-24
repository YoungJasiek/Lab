#pragma once

#include "LabMath.h"
#include <string>
#include <vector>
#include <memory>

namespace Lab {
    class Skeleton;
}

namespace Lab::LFM {

    enum class ElementType {
        Actor,
        Bone,
        Camera,
        Light,
        Group
    };

    struct ElementNode {
        std::string name;
        std::string displayName;
        ElementType type = ElementType::Group;
        bool expanded = false;
        bool selected = false;
        int boneIndex = -1;
        Vec3 localPosition{0.0f, 0.0f, 0.0f};
        Vec3 localRotation{0.0f, 0.0f, 0.0f};
        std::vector<ElementNode> children;

        void addChild(ElementNode child) {
            children.push_back(std::move(child));
        }
    };

    class AnimationSet {
    public:
        AnimationSet();

        void populateDefault();
        void buildFromSkeleton(const std::string& actorName, const Skeleton& skeleton, const std::string& modelAsset);

        void addCamera(const std::string& name, const Vec3& pos = {0, 0, 0});
        void addLight(const std::string& name, const Vec3& pos = {0, 0, 0}, const Vec3& color = {1.0f, 0.95f, 0.85f});
        void clearCameras();
        void clearLights();

        std::vector<ElementNode>& getRoots() { return _roots; }
        const std::vector<ElementNode>& getRoots() const { return _roots; }

        ElementNode* findElement(const std::string& name);
        ElementNode* getSelectedElement() { return _selectedElement; }
        const ElementNode* getSelectedElement() const { return _selectedElement; }
        int getSelectedBoneIndex() const { return _selectedElement ? _selectedElement->boneIndex : -1; }
        void selectElement(const std::string& name);

    private:
        ElementNode* findRecursive(ElementNode& node, const std::string& name);
        std::vector<ElementNode> _roots;
        ElementNode* _selectedElement = nullptr;
    };

} // namespace Lab::LFM
