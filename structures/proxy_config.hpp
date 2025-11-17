/**
 * Project ISA25 Filter Resolver
 * Author: Adam Havlík (xhavli59)
 * Date: 17.11.2025
 */

#pragma once

#include <string>
#include <netinet/in.h>

struct proxy_config {
    std::string server;      // Hostname or IP address of real DNS server
    in_addr server_ip;       // IPv4 address of real DNS server
    uint16_t port = 53;      // Default DNS port
    std::string filter_file; // Path to filter file
    bool verbose = false;    // Verbose output
};

struct upstream_server {
    bool has_ipv4 = false;
    bool has_ipv6 = false;
    sockaddr_in  ipv4{};
    sockaddr_in6 ipv6{};
};
