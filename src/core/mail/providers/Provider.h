#pragma once

#include <string>

namespace ngks::core::mail::providers {
class Provider {
public:
    virtual ~Provider() = default;
    virtual std::string Name() const = 0;
};
}
