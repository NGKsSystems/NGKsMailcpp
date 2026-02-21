#pragma once

#include <string>

namespace ngks::core::mail::types {
struct MimePart {
    std::string contentType;
    std::string body;
};
}
