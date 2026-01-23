#include "Parser.hpp"
#include <libxml/HTMLparser.h>
#include <libxml/uri.h>
#include <libxml/xpath.h>
// #include <libxml/encoding.h>
#include <iconv.h>
#include <regex>
#include <vector>
// #include <cstring>
#include <algorithm>
#include <cctype>

static std::string to_string(const xmlChar* s) {
    return s ? std::string(reinterpret_cast<const char*>(s)) : std::string();
}

static std::string collapse_ws(const std::string& s) {
    std::string out;
    bool ws = false;
    for (const unsigned char c : s) {
        if (std::isspace(c)) { if (!ws) { out.push_back(' '); ws = true; } }
        else { out.push_back(c); ws = false; }
    }
    return out;
}

static size_t node_text_length(const xmlNodePtr node) {
    xmlChar* txt = xmlNodeGetContent(node);
    size_t len = 0;
    if (txt) {
        const std::string s = to_string(txt);
        const std::string c = collapse_ws(s);
        len = c.size();
        xmlFree(txt);
    }
    return len;
}

static size_t node_link_text_length(const xmlNodePtr node) {
    size_t total = 0;
    for (xmlNodePtr cur = node->children; cur; cur = cur->next) {
        if (cur->type == XML_ELEMENT_NODE) {
            if (xmlStrcasecmp(cur->name, reinterpret_cast<const xmlChar*>("a"))==0) {
                if (xmlChar* t = xmlNodeGetContent(cur)) { total += to_string(t).size(); xmlFree(t); }
            }
            total += node_link_text_length(cur);
        }
    }
    return total;
}

static int count_p_tags(const xmlNodePtr node) {
    int cnt = 0;
    for (xmlNodePtr cur = node->children; cur; cur = cur->next) {
        if (cur->type == XML_ELEMENT_NODE) {
            if (xmlStrcasecmp(cur->name, reinterpret_cast<const xmlChar*>("p"))==0) cnt++;
            cnt += count_p_tags(cur);
        }
    }
    return cnt;
}

static double score_node(const xmlNodePtr node) {
    const size_t text_len = node_text_length(node);
    if (text_len == 0) return 0.0;
    const size_t link_len = node_link_text_length(node);
    const double link_density = static_cast<double>(link_len) / static_cast<double>(text_len);
    const int p_count = count_p_tags(node);
    xmlChar* txt = xmlNodeGetContent(node);
    int commas = 0;
    if (txt) {
        std::string s = to_string(txt);
        commas = std::ranges::count(s, ',');
        xmlFree(txt);
    }
    const double score = static_cast<double>(text_len) * (1.0 - link_density) + static_cast<double>(commas) * 20.0 + static_cast<double>(p_count) * 15.0;
    return score;
}

static xmlNodePtr find_best_content_node(const xmlDocPtr doc) {
    xmlNodePtr best = nullptr;
    double best_score = 0.0;
    const xmlNodePtr cur = xmlDocGetRootElement(doc);
    std::vector<xmlNodePtr> stack;
    if (cur) stack.push_back(cur);
    while (!stack.empty()) {
        const xmlNodePtr n = stack.back(); stack.pop_back();
        if (n->type == XML_ELEMENT_NODE) {
            if (xmlStrcasecmp(n->name, reinterpret_cast<const xmlChar*>("div"))==0 ||
                xmlStrcasecmp(n->name, reinterpret_cast<const xmlChar*>("article"))==0 ||
                xmlStrcasecmp(n->name, reinterpret_cast<const xmlChar*>("section"))==0 ||
                xmlStrcasecmp(n->name, reinterpret_cast<const xmlChar*>("main"))==0 ||
                xmlStrcasecmp(n->name, reinterpret_cast<const xmlChar*>("body"))==0) {
                if (const double sc = score_node(n); sc > best_score) { best_score = sc; best = n; }
            }
        }
        for (xmlNodePtr c = n->children; c; c = c->next) stack.push_back(c);
    }
    return best;
}

static bool convert_to_utf8(const std::string& in, const std::string& from_enc, std::string& out) {
    if (from_enc.empty()) return false;
    const std::string to_enc = "UTF-8";
    const iconv_t cd = iconv_open(to_enc.c_str(), from_enc.c_str());
    if (cd == reinterpret_cast<iconv_t>(-1)) return false;
    const size_t in_bytes = in.size();
    const size_t out_bytes = in_bytes * 4 + 16;
    std::vector<char> buf(out_bytes);
    const auto in_buf = const_cast<char*>(in.data());
    char* outbuf = buf.data();
    size_t in_left = in_bytes;
    size_t out_left = out_bytes;
    char* pin = in_buf;
    char* pout = outbuf;
    if (const size_t res = iconv(cd, &pin, &in_left, &pout, &out_left); res == static_cast<size_t>(-1)) {
        iconv_close(cd);
        return false;
    }
    out.assign(buf.data(), out_bytes - out_left);
    iconv_close(cd);
    return true;
}

static std::string get_xpath_string(xmlDocPtr doc, const xmlXPathContextPtr ctx, const char* expr) {
    const xmlXPathObjectPtr res = xmlXPathEvalExpression(reinterpret_cast<const xmlChar*>(expr), ctx);
    if (!res) return {};
    std::string out;
    if (res->nodesetval && res->nodesetval->nodeNr > 0) {
        const xmlNodePtr node = res->nodesetval->nodeTab[0];
        xmlChar* content = xmlNodeGetContent(node);
        out = to_string(content);
        xmlFree(content);
    }
    xmlXPathFreeObject(res);
    return out;
}

static std::string lower_str(const std::string& s) {
    std::string r = s;
    std::ranges::transform(r, r.begin(), [](const unsigned char c){ return std::tolower(c); });
    return r;
}

static void sanitize_node(const xmlNodePtr node) {
    for (xmlNodePtr cur = node ? node->children : nullptr; cur; ) {
        const xmlNodePtr next = cur->next;
        if (cur->type == XML_ELEMENT_NODE) {
            std::string name = to_string(cur->name);
            if (std::string ln = lower_str(name); ln == "script" || ln == "style" || ln == "no script" || ln == "iframe" || ln == "object" || ln == "embed") {
                xmlUnlinkNode(cur);
                xmlFreeNode(cur);
                cur = next;
                continue;
            }
            for (xmlAttrPtr attr = cur->properties; attr; ) {
                const xmlAttrPtr nextAttr = attr->next;
                std::string a_name = to_string(attr->name);
                if (std::string lan = lower_str(a_name); lan.size() >= 2 && lan[0] == 'o' && lan[1] == 'n') {
                    xmlUnsetProp(cur, attr->name);
                } else if (lan == "href" || lan == "src") {
                    if (xmlChar* val = xmlGetProp(cur, attr->name)) {
                        if (std::string v = lower_str(to_string(val)); v.rfind("javascript:", 0) == 0 || v.rfind("data:", 0) == 0) {
                            xmlUnsetProp(cur, attr->name);
                        }
                        xmlFree(val);
                    }
                }
                attr = nextAttr;
            }
            sanitize_node(cur);
        }
        cur = next;
    }
}

ParseResult Parser::parse(const std::string& url, const std::string& content_type, const std::string& body)
{
    ParseResult r;
    std::string encoding;
    if (auto pos = content_type.find("charset="); pos != std::string::npos) encoding = content_type.substr(pos + 8);

    std::string body_to_parse = body;
    if (encoding.empty()) {
        std::regex r_charset(R"(<meta\s+charset=[\"']?([^\"'>\s]+))", std::regex::icase);
        if (std::smatch m; std::regex_search(body, m, r_charset) && m.size() > 1) encoding = m[1].str();
        else {
            if (std::regex r_ct(R"(<meta[^>]*http-equiv=[\"']?Content-Type[\"']?[^>]*content=[\"']?([^\"'>]+))", std::regex::icase); std::regex_search(body, m, r_ct) && m.size() > 1) {
                std::string v = m[1].str();
                if (auto p2 = v.find("charset="); p2 != std::string::npos) encoding = v.substr(p2 + 8);
            }
        }
    }

    if (!encoding.empty()) {
        std::string enc_l = encoding;
        for (auto &c: enc_l) c = std::tolower(static_cast<unsigned char>(c));
        if (enc_l != "utf-8" && enc_l != "utf8") {
            if (std::string converted; convert_to_utf8(body, encoding, converted)) {
                body_to_parse.swap(converted);
            } else {
                r.errors.push_back(std::string("charset conversion failed: ") + encoding);
            }
        }
    }

    htmlDocPtr doc = htmlReadMemory(body_to_parse.c_str(), static_cast<int>(body_to_parse.size()), url.c_str(), "UTF-8",
                                    HTML_PARSE_NOERROR | HTML_PARSE_NOWARNING | HTML_PARSE_RECOVER | HTML_PARSE_NONET);
    if (!doc) {
        r.errors.push_back("failed to parse HTML");
        return r;
    }

    xmlXPathContextPtr ctx = xmlXPathNewContext(doc);
    if (!ctx) {
        r.errors.push_back("failed to create xpath context");
        xmlFreeDoc(doc);
        return r;
    }

    r.title = get_xpath_string(doc, ctx, "//title");
    r.meta_description = get_xpath_string(doc, ctx, "//meta[translate(@name,'ABCDEFGHIJKLMNOPQRSTUVWXYZ','abcdefghijklmnopqrstuvwxyz')='description']/@content");
    r.canonical = get_xpath_string(doc, ctx, "//link[translate(@rel,'ABCDEFGHIJKLMNOPQRSTUVWXYZ','abcdefghijklmnopqrstuvwxyz')='canonical']/@href");
    std::string base = get_xpath_string(doc, ctx, "//base/@href");

    if (doc->encoding) r.charset = to_string(doc->encoding);
    if (r.charset.empty()) {
        if (auto pos2 = content_type.find("charset="); pos2 != std::string::npos) r.charset = content_type.substr(pos2 + 8);
    }

    xmlXPathObjectPtr links = xmlXPathEvalExpression(reinterpret_cast<const xmlChar*>("//a[@href]"), ctx);
    if (links && links->nodesetval) {
        for (int i = 0; i < links->nodesetval->nodeNr; ++i) {
            xmlNodePtr node = links->nodesetval->nodeTab[i];
            xmlChar* href = xmlGetProp(node, reinterpret_cast<const xmlChar*>("href"));
            if (!href) continue;
            Link L;
            const xmlChar* base_c = base.empty() ? reinterpret_cast<const xmlChar*>(url.c_str()) : reinterpret_cast<const xmlChar*>(base.c_str());
            if (xmlChar* abs = xmlBuildURI(href, base_c)) { L.href = to_string(abs); xmlFree(abs); }
            else L.href = to_string(href);
            xmlChar* text = xmlNodeGetContent(node);
            L.text = to_string(text);
            if (text) xmlFree(text);
            if (xmlChar* rel = xmlGetProp(node, reinterpret_cast<const xmlChar*>("rel"))) { std::string rels = to_string(rel); if (rels.find("nofollow")!=std::string::npos) L.nofollow = true; L.rel = rels; xmlFree(rel); }
            r.links.push_back(std::move(L));
            xmlFree(href);
        }
    }
    if (links) xmlXPathFreeObject(links);

    // body node: sanitize and produce safe_html + text
    xmlXPathObjectPtr body_nodes = xmlXPathEvalExpression(reinterpret_cast<const xmlChar*>("//body"), ctx);
    xmlNodePtr body_node = nullptr;
    if (body_nodes && body_nodes->nodesetval && body_nodes->nodesetval->nodeNr > 0) {
        body_node = body_nodes->nodesetval->nodeTab[0];
    } else {
        body_node = reinterpret_cast<xmlNodePtr>(xmlDocGetRootElement(doc));
    }
    if (body_nodes) xmlXPathFreeObject(body_nodes);

    if (body_node) {
        sanitize_node(body_node);
        if (xmlBufferPtr buf = xmlBufferCreate()) {
            xmlNodeDump(buf, doc, body_node, 0, 0);
            if (const xmlChar* content = xmlBufferContent(buf)) r.safe_html = to_string(content);
            xmlBufferFree(buf);
        }
        xmlChar* txt = xmlNodeGetContent(body_node);
        if (txt) { r.text = to_string(txt); xmlFree(txt); }
    }

    // Prefer best content node (Readability-style) if available
    if (xmlNodePtr best = find_best_content_node(doc)) {
        if (xmlBufferPtr buf2 = xmlBufferCreate()) {
            xmlNodeDump(buf2, doc, best, 0, 0);
            if (const xmlChar* c = xmlBufferContent(buf2)) r.safe_html = to_string(c);
            xmlBufferFree(buf2);
        }
        if (xmlChar* bt = xmlNodeGetContent(best)) { r.text = collapse_ws(to_string(bt)); xmlFree(bt); }
    }

    // excerpt (first 200 chars of collapsed text)
    if (!r.text.empty()) {
        std::string t = r.text;
        std::string out; bool ws=false;
        for (char c : t) {
            if (isspace(static_cast<unsigned char>(c))) { if (!ws) { out.push_back(' '); ws=true; } }
            else { out.push_back(c); ws=false; }
            if (out.size() >= 200) break;
        }
        r.excerpt = out;
    }

    xmlXPathFreeContext(ctx);
    xmlFreeDoc(doc);
    return r;
}
