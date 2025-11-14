#include <iostream>
#include <iomanip>
#include "proxy_config.h"
#include <arpa/inet.h>

void print_config(const proxy_config &config) {
    char ipbuf[INET_ADDRSTRLEN]{};
    inet_ntop(AF_INET, &config.server_ip, ipbuf, sizeof(ipbuf));
    std::cout << "=== DNS Proxy Configuration ===" << std::endl;
    std::cout << std::left << std::setw(15) << "Server addr:"
              << (config.server.empty() ? "(none)" : config.server) << std::endl;
    std::cout << std::left << std::setw(15) << "Resolved IP:"
              << (config.server.empty() ? "(none)" : ipbuf) << std::endl;
    std::cout << std::left << std::setw(15) << "Port:"
              << config.port << std::endl;
    std::cout << std::left << std::setw(15) << "Filter file:"
              << (config.filter_file.empty() ? "(none)" : config.filter_file) << std::endl;
    std::cout << std::left << std::setw(15) << "Verbose:"
              << (config.verbose ? "enabled" : "disabled") << std::endl;
    std::cout << "================================" << std::endl;
}
