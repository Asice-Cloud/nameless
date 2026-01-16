#include <iostream>
#include <iomanip>
#include <vector>
#include <string>
#include <algorithm>
#include <filesystem>
#include <sys/stat.h>
#include <pwd.h>
#include <grp.h>
#include <unistd.h>
#include <sstream>
#include <fstream>
#include <cmath>
#include <cstring>

namespace fs = std::filesystem;

struct Config {
    bool show_files = false;
    int max_depth = -1;
    bool color = true;
    bool show_info = false;
    bool human_readable = false;
    std::string start = ".";
    std::string export_svg; // if non-empty, path to output SVG
};

static std::string strip_ansi(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\033') {
            // skip CSI sequences like \033[...m
            if (i + 1 < s.size() && s[i+1] == '[') {
                i += 2;
                while (i < s.size() && s[i] != 'm') ++i;
                continue;
            }
        }
        out.push_back(s[i]);
    }
    return out;
}

static std::string xml_escape(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        switch (c) {
            case '&': out += "&amp;"; break;
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            case '"': out += "&quot;"; break;
            case '\'': out += "&apos;"; break;
            default: out += c; break;
        }
    }
    return out;
}

static bool generate_svg_from_text(const std::string& text, const std::string& outpath, int font_size = 12) {
    std::vector<std::string> lines;
    std::string cur;
    for (char c : text) {
        if (c == '\n') { lines.push_back(cur); cur.clear(); }
        else cur.push_back(c);
    }
    if (!cur.empty() || text.empty()) lines.push_back(cur);

    size_t maxlen = 0;
    for (auto &l : lines) if (l.size() > maxlen) maxlen = l.size();

    double char_w = font_size * 0.6; // approximate
    double line_h = font_size * 1.2;
    int width = static_cast<int>(std::ceil(maxlen * char_w)) + 10;
    int height = static_cast<int>(std::ceil(lines.size() * line_h)) + 10;

    std::ostringstream oss;
    oss << "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n";
    oss << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"" << width << "\" height=\"" << height << "\">\n";
    oss << "<rect width=\"100%\" height=\"100%\" fill=\"white\"/>\n";
    oss << "<g font-family=\"DejaVu Sans Mono, monospace\" font-size=\"" << font_size << "\" fill=\"black\">\n";
    double y = font_size;
    for (auto &l : lines) {
        oss << "  <text x=\"5\" y=\"" << static_cast<int>(y) << "\">" << xml_escape(l) << "</text>\n";
        y += line_h;
    }
    oss << "</g>\n</svg>\n";

    std::ofstream fout(outpath, std::ios::binary);
    if (!fout) return false;
    fout << oss.str();
    return true;
}

static bool is_tty() {
    return isatty(fileno(stdout));
}

static const std::string color_reset = "\033[0m";
static const std::string col_dir = "\033[34m";
static const std::string col_symlink = "\033[36m";
static const std::string col_exec = "\033[32m";
static const std::string col_file = "\033[0m";

std::string human_size(std::uintmax_t size) {
    const char* units[] = {"B","K","M","G","T","P"};
    double s = static_cast<double>(size);
    int i = 0;
    while (s >= 1024.0 && i < 5) { s /= 1024.0; ++i; }
    std::ostringstream oss;
    if (i == 0) oss << static_cast<std::uintmax_t>(s) << units[i];
    else oss << std::fixed << std::setprecision(1) << s << units[i];
    return oss.str();
}

std::string perms_to_string(fs::perms p) {
    std::string s = "---------";
    s[0] = ((p & fs::perms::owner_read) != fs::perms::none) ? 'r' : '-';
    s[1] = ((p & fs::perms::owner_write) != fs::perms::none) ? 'w' : '-';
    s[2] = ((p & fs::perms::owner_exec) != fs::perms::none) ? 'x' : '-';
    s[3] = ((p & fs::perms::group_read) != fs::perms::none) ? 'r' : '-';
    s[4] = ((p & fs::perms::group_write) != fs::perms::none) ? 'w' : '-';
    s[5] = ((p & fs::perms::group_exec) != fs::perms::none) ? 'x' : '-';
    s[6] = ((p & fs::perms::others_read) != fs::perms::none) ? 'r' : '-';
    s[7] = ((p & fs::perms::others_write) != fs::perms::none) ? 'w' : '-';
    s[8] = ((p & fs::perms::others_exec) != fs::perms::none) ? 'x' : '-';
    return s;
}

std::string uid_to_name(uid_t uid) {
    struct passwd *pw = getpwuid(uid);
    if (pw) return std::string(pw->pw_name);
    return std::to_string(uid);
}

std::string gid_to_name(gid_t gid) {
    struct group *gr = getgrgid(gid);
    if (gr) return std::string(gr->gr_name);
    return std::to_string(gid);
}

void print_entry_prefix(std::ostream& out, const std::vector<bool>& last) {
    for (std::size_t i = 0; i + 1 < last.size(); ++i) {
        out << (last[i] ? "    " : "│   ");
    }
    if (!last.empty()) out << (last.back() ? "└── " : "├── ");
}

struct Entry {
    fs::directory_entry de;
    std::string name;
    bool is_dir;
};

void print_tree(const fs::path& p, const Config& cfg, int depth, std::vector<bool>& last);

void print_tree(const fs::directory_entry& de, std::string_view name, const Config& cfg, int depth, std::vector<bool>& last, std::ostream& out) {
    if (cfg.max_depth >= 0 && depth > cfg.max_depth) return;

    bool is_dir = de.is_directory();
    bool is_symlink = de.is_symlink();
    bool is_reg = de.is_regular_file();
    bool is_exec = false;
    try {
        if (is_reg) {
            auto perms = de.status().permissions();
            is_exec = (perms & fs::perms::owner_exec) != fs::perms::none ||
                      (perms & fs::perms::group_exec) != fs::perms::none ||
                      (perms & fs::perms::others_exec) != fs::perms::none;
        }
    } catch (...) {}

    if (depth == 0) {
        out << (name.empty() ? de.path().string() : name) << '\n';
    } else {
        print_entry_prefix(out, last);
        if (cfg.color && is_tty()) {
            if (is_dir) out << col_dir << name << color_reset;
            else if (is_symlink) out << col_symlink << name << color_reset;
            else if (is_exec) out << col_exec << name << color_reset;
            else out << col_file << name << color_reset;
        } else {
            out << name;
        }
        if (cfg.show_info) {
            try {
                std::uintmax_t size = 0;
                if (is_reg) size = fs::file_size(de.path());
                auto fperms = de.status().permissions();
                std::string permstr = perms_to_string(fperms);
                struct stat sb;
                if (lstat(de.path().c_str(), &sb) == 0) {
                    std::string u = uid_to_name(sb.st_uid);
                    std::string g = gid_to_name(sb.st_gid);
                    if (cfg.human_readable) {
                        out << "  [" << permstr << " " << u << ":" << g << " " << human_size(size) << "]";
                    } else {
                        out << "  [" << permstr << " " << u << ":" << g << " " << size << "B]";
                    }
                } else {
                    if (cfg.human_readable)
                        out << "  [" << permstr << " " << human_size(size) << "]";
                    else
                        out << "  [" << permstr << " " << "??B]";
                }
            } catch (...) {
            }
        }
        if (is_symlink) {
            try {
                auto target = fs::read_symlink(de.path()).string();
                out << " -> " << target;
            } catch (...) {}
        }
        out << '\n';
    }

    if (!is_dir) return;

    std::vector<Entry> entries;
    try {
        for (auto &d : fs::directory_iterator(de.path())) {
            if (!cfg.show_files && d.is_regular_file()) continue;
            Entry e{d, d.path().filename().string(), d.is_directory()};
            entries.push_back(std::move(e));
        }
    } catch (...) {}

    std::sort(entries.begin(), entries.end(), [](const Entry& a, const Entry& b){
        if (a.is_dir != b.is_dir) return a.is_dir > b.is_dir;
        return a.name < b.name;
    });

    for (std::size_t i = 0; i < entries.size(); ++i) {
        last.push_back(i + 1 == entries.size());
        print_tree(entries[i].de, entries[i].name, cfg, depth + 1, last, out);
        last.pop_back();
    }
}

void print_tree(const fs::path& p, const Config& cfg, int depth, std::vector<bool>& last, std::ostream& out) {
    if (cfg.max_depth >= 0 && depth > cfg.max_depth) return;
    // Use generic directory iteration for path-rooted calls: build entries and delegate
    bool is_dir = fs::is_directory(p);
    if (depth == 0) {
        std::string name = p.filename().string();
        out << (name.empty() ? p.string() : name) << '\n';
    }

    if (!is_dir) return;

    std::vector<Entry> entries;
    try {
        for (auto &d : fs::directory_iterator(p)) {
            if (!cfg.show_files && d.is_regular_file()) continue;
            Entry e{d, d.path().filename().string(), d.is_directory()};
            entries.push_back(std::move(e));
        }
    } catch (...) {}

    std::sort(entries.begin(), entries.end(), [](const Entry& a, const Entry& b){
        if (a.is_dir != b.is_dir) return a.is_dir > b.is_dir;
        return a.name < b.name;
    });

    for (std::size_t i = 0; i < entries.size(); ++i) {
        last.push_back(i + 1 == entries.size());
        print_tree(entries[i].de, entries[i].name, cfg, depth + 1, last, out);
        last.pop_back();
    }
}

int main(int argc, char** argv) {
    Config cfg;
    cfg.color = is_tty();
    for (int i = 1; i < argc; ++i) {
        std::string s = argv[i];
        if (s == "-a" || s == "--all") cfg.show_files = true;
        else if ((s == "-L" || s == "--level") && i + 1 < argc) cfg.max_depth = std::stoi(argv[++i]);
        else if (s == "-S" || s == "--export-svg") {
            if (i + 1 < argc) cfg.export_svg = argv[++i];
            else {
                std::cerr << "Option " << s << " requires a file path argument\n";
                return 2;
            }
        }
        else if (s == "-h" || s == "--help") {
            std::cout << "Usage: " << argv[0] << " [options] [path]\n"
                      << "  -a, --all           show files as well as directories\n"
                      << "  -L n, --level n     descend only n levels\n"
                      << "  -c, --color         force color output\n"
                      << "  --no-color          disable color output\n"
                      << "  -S, --export-svg F  export tree as SVG to file F\n"
                      << "  -i, --info          show permissions/owner/size\n"
                      << "  -H, --human         human-readable sizes (with -i)\n"
                      << "  -h, --help          show this help\n";
            return 0;
        } else if (s == "-c" || s == "--color") cfg.color = true;
        else if (s == "--no-color") cfg.color = false;
        else if (s == "-i" || s == "--info") cfg.show_info = true;
        else if (s == "-H" || s == "--human") cfg.human_readable = true;
        else if (!s.empty() && s[0] == '-') {
            std::cerr << "Unknown option: " << s << '\n';
            return 2;
        } else {
            cfg.start = s;
        }
    }

    fs::path p(cfg.start);
    if (!fs::exists(p)) {
        std::cerr << "Path does not exist: " << cfg.start << '\n';
        return 2;
    }

    std::vector<bool> last;
    if (!cfg.export_svg.empty()) {
        // capture text output, strip ANSI, then convert to SVG
        cfg.color = false; // avoid ANSI codes in captured text
        std::ostringstream oss;
        print_tree(p, cfg, 0, last, oss);
        std::string txt = strip_ansi(oss.str());
        if (!generate_svg_from_text(txt, cfg.export_svg)) {
            std::cerr << "Failed to write SVG to " << cfg.export_svg << '\n';
            return 2;
        }
    } else {
        print_tree(p, cfg, 0, last, std::cout);
    }
    return 0;
}
