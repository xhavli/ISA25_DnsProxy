#pragma once

#include <string>
#include <netinet/in.h>

// Proxy configuration structure
struct proxy_config {
    std::string server;      // Hostname or IP address of real DNS server
    in_addr server_ip;       // IPv4 address of real DNS server
    uint16_t port = 53;      // Default DNS port
    std::string filter_file; // Path to filter file
    bool verbose = false;    // Verbose output
};

// Function prototype
void print_config(const proxy_config &cfg);
