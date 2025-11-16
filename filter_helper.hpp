#pragma once

#include <string>
#include <vector>

// Represents a single rule in the filter system
struct filter_rule {
    bool wildcard = false;   // leading wildcard '*'?
    std::string domain;      // normalized lowercase domain (no '*' or leading dots)
};

// Load and validate rules from file
std::vector<filter_rule> load_filters(const std::string &filename, bool verbose);

// Return true if queried domain matches any rule
bool is_blocked(const std::string &domain, const std::vector<filter_rule> &rules);