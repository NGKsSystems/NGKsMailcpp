#pragma once

#include <functional>

namespace ngks::core::bus {
class EventBus {
public:
    using Handler = std::function<void()>;
    void Publish();
    void Subscribe(Handler handler);
private:
    Handler handler_{};
};
}
