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

void print_config(const proxy_config& config, const upstream_server& upstream) {
    std::cout << "====== DNS Proxy Configuration ======\n";
    std::cout << std::left << std::setw(15) << "Server addr:" << config.server << "\n";

    if (upstream.has_ipv4) {
        char buf4[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &upstream.v4.sin_addr, buf4, sizeof(buf4));
        std::cout << std::left << std::setw(15) << "IPv4 upstream:" << buf4 << "\n";
    }
    if (upstream.has_ipv6) {
        char buf6[INET6_ADDRSTRLEN];
        inet_ntop(AF_INET6, &upstream.v6.sin6_addr, buf6, sizeof(buf6));
        std::cout << std::left << std::setw(15) << "IPv6 upstream:" << buf6 << "\n";
    }

    std::cout << std::left << std::setw(15) << "Listening on:";

    if (upstream.has_ipv4) std::cout << "IPv4 ";
    if (upstream.has_ipv6) std::cout << "IPv6 ";
    std::cout << "\n";

    std::cout << std::left << std::setw(15) << "Port:" << config.port << "\n";
    std::cout << std::left << std::setw(15) << "Filter file:" << config.filter_file << "\n";
    std::cout << std::left << std::setw(15) << "Verbose:" << (config.verbose ? "enabled" : "disabled") << "\n";
    std::cout << "======================================\n";
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
