#include <iostream>
#include <iomanip>
#include <arpa/inet.h>

#include "qtype.h"
#include "qclass.h"
#include "print_helper.h"
#include "dns_structures.h"

void print_usage(const char* prog) {
    std::cerr << "Usage: " << prog << " -s server [-p port] -f filter_file [-v]\n";
}

void print_config(const proxy_config& config) {
    char ipbuf[INET_ADDRSTRLEN]{};
    inet_ntop(AF_INET, &config.server_ip, ipbuf, sizeof(ipbuf));
    std::cout << "====== DNS Proxy Configuration ======" << std::endl;
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
    std::cout << "======================================" << std::endl;
}

void print_query(const dns_query& query, const dns_packet& pkt) {
    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &pkt.clientAddr.sin_addr, client_ip, sizeof(client_ip));
    int client_port = ntohs(pkt.clientAddr.sin_port);

    std::cout << "Query received:\n";
    std::cout << "  ID: " << query.id << "\n";
    std::cout << "  From: " << client_ip << ":" << client_port << "\n";
    std::cout << "  Name: " << query.qname << "\n";
    std::cout << "  Type: " << query.qtype;
    if (query.qtype == QTYPE_A) std::cout << " (A)";
    std::cout << "\n";
    std::cout << "  Class: " << query.qclass;
    if (query.qclass == QCLASS_IN) std::cout << " (IN)";
    std::cout << "\n";
    std::cout << "  Blocked: " << (query.blocked ? "YES" : "NO") << "\n";
}
