#pragma once
#include "LabComponent.h"
#include "LabTransform.h"
#include <string>
#include <vector>
#include <memory>
#include <algorithm>
#include <type_traits>
#include <typeindex>
#include <unordered_map>

namespace Lab {

    /// @brief Entity class representing an object in the scene hierarchy with components
    class Entity : public std::enable_shared_from_this<Entity> {
    public:
        std::string name{"Entity"};
        std::string tag{""};
        bool active{true};
        std::shared_ptr<Transform> transform;

        Entity(const std::string& name = "Entity", const std::string& tag = "")
            : name(name), tag(tag) {
            transform = std::make_shared<Transform>();
            transform->entity = this;
        }

        void AddTag(const std::string& t) { tag = t; }

        virtual ~Entity() {
            m_componentCache.clear();
            for (auto& comp : m_components) {
                if (comp) comp->OnDestroy();
            }
            m_components.clear();
        }

        template<typename T, typename... Args>
        std::shared_ptr<T> AddComponent(Args&&... args) {
            static_assert(std::is_base_of<Component, T>::value, "T must derive from Component");
            auto comp = std::make_shared<T>(std::forward<Args>(args)...);
            comp->entity = this;
            m_components.push_back(comp);
            m_componentCache[std::type_index(typeid(T))] = comp.get();
            comp->Awake();
            comp->Start();
            return comp;
        }

        template<typename T>
        T* GetComponent() const {
            static_assert(std::is_base_of<Component, T>::value, "T must derive from Component");
            if constexpr (std::is_same<T, Transform>::value) {
                return transform.get();
            }
            const std::type_index key(typeid(T));
            auto it = m_componentCache.find(key);
            if (it != m_componentCache.end()) {
                return static_cast<T*>(it->second);
            }
            for (const auto& comp : m_components) {
                T* casted = dynamic_cast<T*>(comp.get());
                if (casted) {
                    m_componentCache[key] = casted;
                    return casted;
                }
            }
            return nullptr;
        }

        template<typename T>
        bool HasComponent() const {
            return GetComponent<T>() != nullptr;
        }

        template<typename T>
        bool RemoveComponent() {
            static_assert(std::is_base_of<Component, T>::value, "T must derive from Component");
            for (auto it = m_components.begin(); it != m_components.end(); ++it) {
                T* casted = dynamic_cast<T*>(it->get());
                if (casted) {
                    m_componentCache.clear();
                    (*it)->OnDestroy();
                    m_components.erase(it);
                    return true;
                }
            }
            return false;
        }

        const std::vector<std::shared_ptr<Component>>& getComponents() const {
            return m_components;
        }

        void Update(float dt) {
            if (!active) return;
            for (auto& comp : m_components) {
                if (comp && comp->enabled) comp->Update(dt);
            }
        }

        void LateUpdate(float dt) {
            if (!active) return;
            for (auto& comp : m_components) {
                if (comp && comp->enabled) comp->LateUpdate(dt);
            }
        }

    private:
        std::vector<std::shared_ptr<Component>> m_components;
        mutable std::unordered_map<std::type_index, Component*> m_componentCache;
    };

    inline Transform* Component::getTransform() const {
        return entity ? entity->transform.get() : nullptr;
    }

} // namespace Lab
