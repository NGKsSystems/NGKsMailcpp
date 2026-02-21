#include "core/mail/mime/MimeParser.h"

namespace ngks::core::mail::mime {
bool MimeParser::Parse(const std::string& rawMime) {
    return !rawMime.empty();
}
}
