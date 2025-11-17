/**
 * Project ISA25 Filter Resolver
 * Author: Adam Havlík (xhavli59)
 * Date: 17.11.2025
 */

#pragma once

#include <string>
#include <unordered_set>

std::unordered_set<std::string> load_filters(const std::string &filename, bool verbose);

bool is_blocked(std::string_view domain, const std::unordered_set<std::string>& rules);

