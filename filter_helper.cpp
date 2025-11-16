#include "filter_helper.h"
#include <fstream>
#include <iostream>
#include <algorithm>

static inline void trim(std::string &s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    size_t end   = s.find_last_not_of(" \t\r\n");
    if (start == std::string::npos) s.clear();
    else s = s.substr(start, end - start + 1);
}

static inline std::string to_lower(const std::string &s) {
    std::string out = s;
    std::transform(out.begin(), out.end(), out.begin(), ::tolower);
    return out;
}

bool valid_label(const std::string &label) {
    if (label.empty() || label.size() > 63) return false;
    for (char c : label) {
        if (!(std::isalnum((unsigned char)c) || c == '-'))
            return false;
    }
    return true;
}

bool validate_domain_no_wildcard(const std::string &d) {
    if (d.empty() || d.size() > 253) return false;
    size_t start = 0;
    while (true) {
        size_t pos = d.find('.', start);
        std::string label = (pos == std::string::npos)
                ? d.substr(start)
                : d.substr(start, pos - start);
        if (!valid_label(label)) return false;
        if (pos == std::string::npos) break;
        start = pos + 1;
    }
    return true;
}

std::vector<filter_rule> load_filters(const std::string &filename, bool verbose) {
    std::ifstream file(filename);
    std::vector<filter_rule> rules;

    if (!file.is_open()) {
        std::cerr << "Failed to open filter file: " << filename << "\n";
        return rules;
    }

    std::string line;
    size_t lineno = 0;

    while (std::getline(file, line)) {
        lineno++;
        trim(line);
        if (line.empty() || line[0] == '#')
            continue;

        std::string raw = to_lower(line);
        bool wildcard = false;

        if (raw[0] == '*') {
            wildcard = true;
            if (raw.size() > 1 && raw[1] == '*') {
                std::cerr << "WARNING: Malformed filter on line "
                          << lineno << ": '" << line << "'\n";
                continue;
            }
            raw = raw.substr(1);
            if (!raw.empty() && raw[0] == '.')
                raw = raw.substr(1);
        }

        if (!validate_domain_no_wildcard(raw)) {
            std::cerr << "WARNING: Malformed filter on line "
                      << lineno << ": '" << line << "'\n";
            continue;
        }

        rules.push_back({ wildcard, raw });
    }

    if (verbose) {
        std::cout << "Loaded " << rules.size()
                  << " filter rules from " << filename << "\n";
        std::cout << "======================================\n";
    }

    return rules;
}

bool is_blocked(const std::string &domain, const std::vector<filter_rule> &rules) {
    std::string d = to_lower(domain);

    for (const auto &r : rules) {
        if (!r.wildcard) {
            if (d == r.domain)
                return true;
            if (d.size() > r.domain.size() &&
                d.compare(d.size() - r.domain.size(), r.domain.size(), r.domain) == 0 &&
                d[d.size() - r.domain.size() - 1] == '.')
                return true;
        } else {
            if (d == r.domain)
                return true;
            if (d.size() > r.domain.size() &&
                d.compare(d.size() - r.domain.size(), r.domain.size(), r.domain) == 0)
                return true;
        }
    }
    return false;
}
