#pragma once

#include <string>

namespace ngks::core::mail::mime {
class MimeParser {
public:
    bool Parse(const std::string& rawMime);
};
}
