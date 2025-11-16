#pragma once

#include "proxy_config.hpp"
#include "dns_structures.hpp"

void print_usage(const char* prog);
void print_config(const proxy_config& cfg);
void print_query(const dns_query& query, const dns_packet& pkt);
