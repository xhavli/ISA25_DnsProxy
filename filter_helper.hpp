#pragma once

#include <string>
#include <unordered_set>

// Load and validate domains from file
std::unordered_set<std::string> load_filters(const std::string &filename, bool verbose);

// Return true if queried domain matches any rule
bool is_blocked(const std::string &domain, const std::unordered_set<std::string> &rules);
