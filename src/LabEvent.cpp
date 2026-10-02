#include "LabEvent.h"
#include <algorithm>

namespace Lab {

EventBus& EventBus::get() {
    static EventBus s_instance;
    return s_instance;
}

void EventBus::unsubscribe(EventSubscriptionId id) {
    for (auto& pair : m_subscribers) {
        auto& list = pair.second;
        auto it = std::find_if(list.begin(), list.end(), [id](const Subscription& sub) {
            return sub.id == id;
        });
        if (it != list.end()) {
            list.erase(it);
            return;
        }
    }
}

void EventBus::dispatchQueued() {
    if (m_queuedEvents.empty()) return;

    auto eventsToDispatch = std::move(m_queuedEvents);
    m_queuedEvents.clear();

    for (const auto& ev : eventsToDispatch) {
        if (ev.dispatchFn) {
            ev.dispatchFn();
        }
    }
}

void EventBus::clear() {
    m_subscribers.clear();
    m_queuedEvents.clear();
}

size_t EventBus::getSubscriptionCount() const {
    size_t count = 0;
    for (const auto& pair : m_subscribers) {
        count += pair.second.size();
    }
    return count;
}

} // namespace Lab
