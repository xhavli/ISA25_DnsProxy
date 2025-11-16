#include <iostream>
#include <iomanip>
#include <arpa/inet.h>

#include "qtype.hpp"
#include "qclass.hpp"
#include "print_helper.hpp"
#include "dns_structures.hpp"

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
    char client_ip[INET6_ADDRSTRLEN];
    int client_port = 0;
    const void* addr_ptr = nullptr;

    if (pkt.clientAddr.ss_family == AF_INET) {
        const sockaddr_in* addr4 = reinterpret_cast<const sockaddr_in*>(&pkt.clientAddr);
        addr_ptr = &(addr4->sin_addr);
        client_port = ntohs(addr4->sin_port);
    } else if (pkt.clientAddr.ss_family == AF_INET6) {
        const sockaddr_in6* addr6 = reinterpret_cast<const sockaddr_in6*>(&pkt.clientAddr);
        addr_ptr = &(addr6->sin6_addr);
        client_port = ntohs(addr6->sin6_port);
    }

    if (addr_ptr) {
        inet_ntop(pkt.clientAddr.ss_family, addr_ptr, client_ip, sizeof(client_ip));
    } else {
        std::snprintf(client_ip, sizeof(client_ip), "(unknown)");
    }

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
