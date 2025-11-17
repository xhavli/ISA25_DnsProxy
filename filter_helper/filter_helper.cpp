/**
 * Project ISA25 Filter Resolver
 * Author: Adam Havlík (xhavli59)
 * Date: 17.11.2025
 */

#include <fstream>
#include <iostream>
#include <algorithm>
#include <cctype>
#include <unordered_set>

#include "filter_helper.hpp"

// Trim leading/trailing whitespace
static inline void trim(std::string &line) {
    size_t start = line.find_first_not_of(" \t\r\n");
    size_t end   = line.find_last_not_of(" \t\r\n");
    if (start == std::string::npos) line.clear();
    else line = line.substr(start, end - start + 1);
}

static std::string extract_domain(std::string line) {
    // Remove inline comments
    size_t hash_pos = line.find('#');
    if (hash_pos != std::string::npos) line = line.substr(0, hash_pos);

    trim(line);

    // Strip protocol if present
    if (line.rfind("http://", 0) == 0)      line = line.substr(7);
    else if (line.rfind("https://", 0) == 0) line = line.substr(8);
    if (line.rfind("www.", 0) == 0) line = line.substr(4);

    // Remove path, query, or fragment
    size_t separator = line.find_first_of("/?#");
    if (separator != std::string::npos)
        line = line.substr(0, separator);

    // Convert to lowercase
    std::transform(line.begin(), line.end(), line.begin(), ::tolower);

    return line;
}

// Validate domain label (RFC 1035)
static bool is_valid_label(const std::string &label) {
    if (label.empty() || label.size() > 63)
        return false;

    if (!std::isalnum(label.front()) || !std::isalnum(label.back()))
        return false;

    for (char c : label) {
        if (!(std::isalnum(c) || c == '-'))
            return false;
    }

    return true;
}

// Validate full domain (RFC-compliant, no wildcards)
static bool validate_domain(const std::string &domain) {
    if (domain.empty() || domain.size() > 253)
        return false;

    size_t start = 0;
    while (true) {
        size_t dot = domain.find('.', start);
        std::string label = (dot == std::string::npos)
                                ? domain.substr(start)
                                : domain.substr(start, dot - start);

        if (!is_valid_label(label))
            return false;

        if (dot == std::string::npos)
            break;

        start = dot + 1;
    }
    return true;
}

// Load domain blocklist from file
std::unordered_set<std::string> load_filters(const std::string &filename, bool verbose) {
    std::ifstream file(filename);
    std::unordered_set<std::string> rules;

    if (!file.is_open()) {
        std::cerr << "ERROR: Cannot open file '" << filename << "'\n";
        return rules;
    }

    std::string line;
    size_t lineno = 0;

    while (std::getline(file, line)) {
        lineno++;

        std::string domain = extract_domain(line);

        if (domain.empty())
            continue;

        if (domain.find('*') != std::string::npos) {
            std::cerr << "WARNING: Wildcards are not allowed on line " << lineno << ": '" << line << "'\n";
            continue;
        }

        if (!validate_domain(domain)) {
            std::cerr << "WARNING: Invalid domain format on line " << lineno << ": '" << line << "'\n";
            continue;
        }

        rules.insert(domain);
    }

    if (verbose) {
        std::cout << "Loaded " << rules.size() << " filter rules\n";
        std::cout << "==========================================\n";
    }

    return rules;
}

// Check if domain is blocked by matching exact or suffix
bool is_blocked(std::string_view domain, const std::unordered_set<std::string> &rules) {
    for (const auto &rule : rules) {
        if (domain == rule)
            return true;
        if (domain.size() > rule.size() &&
            domain.compare(domain.size() - rule.size(), rule.size(), rule) == 0 &&
            domain[domain.size() - rule.size() - 1] == '.') {
            return true;
        }
    }
    return false;
}

