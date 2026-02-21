#include "core/bus/EventBus.h"

namespace ngks::core::bus {
void EventBus::Publish() {
    if (handler_) {
        handler_();
    }
}

void EventBus::Subscribe(Handler handler) {
    handler_ = std::move(handler);
}
}
