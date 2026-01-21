#pragma once
#include <string>
#include <vector>

struct Link {
    std::string href;
    std::string text;
    std::string rel;
    bool nofollow = false;
};

struct ParseResult {
    std::string title;
    std::string meta_description;
    std::string canonical;
    std::vector<Link> links;
    std::string excerpt;
    std::string text;
    std::string charset;
    std::string safe_html;
    std::vector<std::string> errors;
};

class Parser {
public:
    ParseResult parse(const std::string& url, const std::string& content_type, const std::string& body) const;
    std::string extract_title(const std::string& html) const;
};
