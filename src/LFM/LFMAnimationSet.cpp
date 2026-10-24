#include "LFM/LFMAnimationSet.h"
#include "LabSkeletal.h"
#include <algorithm>

namespace Lab::LFM {

    AnimationSet::AnimationSet() {
        populateDefault();
    }

    void AnimationSet::populateDefault() {
        _roots.clear();

        // Fallback default structure if no skeleton is loaded yet
        ElementNode guard;
        guard.name = "guard_01";
        guard.displayName = "guard_01 [Actor: guard.glb]";
        guard.type = ElementType::Actor;
        guard.expanded = true;

        ElementNode body;
        body.name = "guard_01_body";
        body.displayName = "Body";
        body.type = ElementType::Group;
        body.expanded = true;

        ElementNode pelvis;
        pelvis.name = "mixamorig:Hips";
        pelvis.displayName = "Hips";
        pelvis.type = ElementType::Bone;
        pelvis.boneIndex = 0;

        ElementNode spine0;
        spine0.name = "mixamorig:Spine";
        spine0.displayName = "Spine";
        spine0.type = ElementType::Bone;
        spine0.boneIndex = 11;

        ElementNode head;
        head.name = "mixamorig:Head";
        head.displayName = "Head";
        head.type = ElementType::Bone;
        head.boneIndex = 40;

        spine0.addChild(std::move(head));
        pelvis.addChild(std::move(spine0));
        body.addChild(std::move(pelvis));
        guard.addChild(std::move(body));

        _roots.push_back(std::move(guard));

        // 2. Cinematic Cameras
        addCamera("camera1", Vec3(-12.0f, 3.5f, 8.0f));
        addCamera("camera2", Vec3(0.0f, 2.0f, 1.5f));

        // 3. Stage Lights
        addLight("key_light", Vec3(-4.0f, 5.0f, 3.0f), Vec3(1.0f, 0.95f, 0.85f));
        addLight("rim_light", Vec3(3.0f, 4.0f, -4.0f), Vec3(0.4f, 0.85f, 1.0f));

        selectElement("mixamorig:Hips");
    }

    void AnimationSet::buildFromSkeleton(const std::string& actorName, const Skeleton& skeleton, const std::string& modelAsset) {
        // Remove existing actor with the same name if present
        _roots.erase(std::remove_if(_roots.begin(), _roots.end(), [&](const ElementNode& n) {
            return n.type == ElementType::Actor && n.name == actorName;
        }), _roots.end());

        ElementNode actor;
        actor.name = actorName;
        actor.displayName = actorName + " [Actor: " + modelAsset + "]";
        actor.type = ElementType::Actor;
        actor.expanded = true;

        auto buildBoneNode = [&](auto& self, int boneIdx) -> ElementNode {
            const Bone* b = skeleton.getBone(boneIdx);
            ElementNode node;
            node.name = b ? b->name : ("Bone_" + std::to_string(boneIdx));

            // Strip mixamorig: prefix for clean display name in tree
            std::string disp = node.name;
            if (disp.rfind("mixamorig:", 0) == 0) {
                disp = disp.substr(10);
            }
            node.displayName = disp;
            node.type = ElementType::Bone;
            node.boneIndex = boneIdx;
            if (b) {
                node.localPosition = b->bindPos;
            }

            // Expand torso, hips and spine branches by default
            node.expanded = (boneIdx == 0 || boneIdx == 11 || boneIdx == 12 || boneIdx == 13 || boneIdx == 38 || boneIdx == 39);

            if (b) {
                for (int childIdx : b->children) {
                    node.addChild(self(self, childIdx));
                }
            }
            return node;
        };

        // Find root bones in skeleton rig (parentIndex == -1)
        for (size_t i = 0; i < skeleton.getBoneCount(); ++i) {
            const Bone* b = skeleton.getBone(static_cast<int>(i));
            if (b && b->parentIndex == -1) {
                actor.addChild(buildBoneNode(buildBoneNode, b->index));
            }
        }

        // Place Actor at the very top of the hierarchy
        _roots.insert(_roots.begin(), std::move(actor));

        // Select the root bone (Hips) by default
        selectElement("mixamorig:Hips");
    }

    void AnimationSet::addCamera(const std::string& name, const Vec3& pos) {
        ElementNode cam;
        cam.name = name;
        cam.displayName = name + " [Camera]";
        cam.type = ElementType::Camera;
        cam.localPosition = pos;
        _roots.push_back(std::move(cam));
    }

    void AnimationSet::addLight(const std::string& name, const Vec3& pos, const Vec3& color) {
        (void)color;
        ElementNode light;
        light.name = name;
        light.displayName = name + " [Light]";
        light.type = ElementType::Light;
        light.localPosition = pos;
        _roots.push_back(std::move(light));
    }

    void AnimationSet::clearCameras() {
        if (_selectedElement && _selectedElement->type == ElementType::Camera) {
            _selectedElement = nullptr;
        }
        _roots.erase(std::remove_if(_roots.begin(), _roots.end(), [](const ElementNode& n) {
            return n.type == ElementType::Camera;
        }), _roots.end());
    }

    void AnimationSet::clearLights() {
        if (_selectedElement && _selectedElement->type == ElementType::Light) {
            _selectedElement = nullptr;
        }
        _roots.erase(std::remove_if(_roots.begin(), _roots.end(), [](const ElementNode& n) {
            return n.type == ElementType::Light;
        }), _roots.end());
    }

    ElementNode* AnimationSet::findRecursive(ElementNode& node, const std::string& name) {
        if (node.name == name) return &node;
        for (auto& child : node.children) {
            ElementNode* res = findRecursive(child, name);
            if (res) return res;
        }
        return nullptr;
    }

    ElementNode* AnimationSet::findElement(const std::string& name) {
        for (auto& root : _roots) {
            ElementNode* res = findRecursive(root, name);
            if (res) return res;
        }
        return nullptr;
    }

    void AnimationSet::selectElement(const std::string& name) {
        auto clearSelection = [](auto& self, ElementNode& node) -> void {
            node.selected = false;
            for (auto& child : node.children) {
                self(self, child);
            }
        };

        for (auto& root : _roots) {
            clearSelection(clearSelection, root);
        }

        _selectedElement = findElement(name);
        if (_selectedElement) {
            _selectedElement->selected = true;
        }
    }

} // namespace Lab::LFM
