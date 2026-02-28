#pragma once

#include <string>
#include <vector>

namespace ngks::core::mail::mime {

struct Attachment {
    std::string name;
    std::string contentType;
    std::string contentId;
    bool isInline = false;
    std::string dataBase64;
};

struct MimeParseResult {
    std::string subject;
    std::string from;
    std::string date;
    std::string messageId;
    std::string text;
    std::string html;
    std::vector<Attachment> attachments;
};

class MimeParser {
public:
    bool Parse(const std::string& rawMime, MimeParseResult& out);
};

}
