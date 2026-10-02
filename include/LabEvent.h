#pragma once
#include "LabCore.h"
#include <functional>
#include <unordered_map>
#include <vector>
#include <typeindex>
#include <memory>
#include <string>

namespace Lab {

/// @brief Identifier for event subscription cancellations.
using EventSubscriptionId = uint64_t;

/// @brief Universal thread-safe publish-subscribe Event Bus.
/// Eliminates hard header couplings between engine modules, plugins, and gameplay.
class LAB_CORE_API EventBus {
public:
    static EventBus& get();

    template<typename T>
    EventSubscriptionId subscribe(std::function<void(const T&)> handler) {
        std::type_index typeId = std::type_index(typeid(T));
        EventSubscriptionId subId = ++m_nextSubscriptionId;

        auto wrapper = [handler](const void* eventData) {
            handler(*static_cast<const T*>(eventData));
        };

        m_subscribers[typeId].push_back({ subId, wrapper });
        return subId;
    }

    void unsubscribe(EventSubscriptionId id);

    template<typename T>
    void broadcast(const T& eventData) {
        std::type_index typeId = std::type_index(typeid(T));
        auto it = m_subscribers.find(typeId);
        if (it != m_subscribers.end()) {
            for (const auto& sub : it->second) {
                if (sub.handler) {
                    sub.handler(&eventData);
                }
            }
        }
    }

    template<typename T>
    void queue(const T& eventData) {
        std::type_index typeId = std::type_index(typeid(T));
        auto copy = std::make_shared<T>(eventData);
        m_queuedEvents.push_back({
            typeId,
            [this, copy]() {
                this->broadcast(*copy);
            }
        });
    }

    /// @brief Dispatches all queued events accumulated during the frame.
    void dispatchQueued();

    /// @brief Clears all subscriptions and pending events.
    void clear();

    /// @brief Returns the total count of active subscriptions across all event types.
    size_t getSubscriptionCount() const;

private:
    EventBus() = default;
    ~EventBus() = default;
    EventBus(const EventBus&) = delete;
    EventBus& operator=(const EventBus&) = delete;

    struct Subscription {
        EventSubscriptionId id;
        std::function<void(const void*)> handler;
    };

    struct QueuedEvent {
        std::type_index typeId;
        std::function<void()> dispatchFn;
    };

    EventSubscriptionId m_nextSubscriptionId = 0;
    std::unordered_map<std::type_index, std::vector<Subscription>> m_subscribers;
    std::vector<QueuedEvent> m_queuedEvents;
};

} // namespace Lab
