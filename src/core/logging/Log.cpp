#include "core/logging/Log.h"

#include <iostream>

namespace ngks::core::logging {
void Info(const std::string& message) {
    std::cout << message << std::endl;
}
}
