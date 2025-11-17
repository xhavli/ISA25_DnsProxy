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
    std::cout << "======== DNS Proxy Configuration ========\n";
    std::cout << std::left << std::setw(15) << "Server addr:" << config.server << "\n";

    if (upstream.has_ipv4) {
        char buf4[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &upstream.ipv4.sin_addr, buf4, sizeof(buf4));
        std::cout << std::left << std::setw(15) << "IPv4 upstream:" << buf4 << "\n";
    }
    if (upstream.has_ipv6) {
        char buf6[INET6_ADDRSTRLEN];
        inet_ntop(AF_INET6, &upstream.ipv6.sin6_addr, buf6, sizeof(buf6));
        std::cout << std::left << std::setw(15) << "IPv6 upstream:" << buf6 << "\n";
    }

    std::cout << std::left << std::setw(15) << "Listening on:";

    if (upstream.has_ipv4) std::cout << "IPv4 ";
    if (upstream.has_ipv6) std::cout << "IPv6 ";
    std::cout << "\n";

    std::cout << std::left << std::setw(15) << "Port:" << config.port << "\n";
    std::cout << std::left << std::setw(15) << "Filter file:" << config.filter_file << "\n";
    std::cout << std::left << std::setw(15) << "Verbose:" << (config.verbose ? "enabled" : "disabled") << "\n";
    std::cout << "==========================================\n";
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
    std::cout << "  From: " << client_ip << ":" << client_port << "\n";
    std::cout << "  ID: " << query.id << "\n";
    std::cout << "  Name: " << query.qname << "\n";
    std::cout << "  Type: " << query.qtype << " (" << QTYPE_to_string(static_cast<QTYPE>(query.qtype)) << ")\n";
    std::cout << "  Class: " << query.qclass << " (" << QCLASS_to_string(static_cast<QCLASS>(query.qclass)) << ")\n";
    std::cout << "  --\n";
    std::cout << "  Blocked: " << (query.blocked ? "YES" : "NO") << "\n";
}



const uint8_t* skip_dns_name(const uint8_t* ptr, [[maybe_unused]] const uint8_t* base) {
    while (*ptr) {
        if ((*ptr & 0xC0) == 0xC0) { // compressed label
            return ptr + 2;
        }
        ptr += *ptr + 1;
    }
    return ptr + 1; // skip zero byte
}

std::string extract_ip(const uint8_t* buffer, ssize_t len) {
    if (len < DNS_HEADER_LENGTH) return ""; // minimum DNS header size
    const uint8_t* ptr = buffer + DNS_HEADER_LENGTH; // skip DNS header
    const uint8_t* end = buffer + len;

    // Skip questions
    uint16_t qdcount = ntohs(*(uint16_t*)(buffer + 4));
    for (int i = 0; i < qdcount; i++) {
        ptr = skip_dns_name(ptr, buffer);
        if (ptr + 4 > end) return "";
        ptr += 4; // QTYPE + QCLASS
    }

    // Parse answers
    uint16_t ancount = ntohs(*(uint16_t*)(buffer + 6));
    for (int i = 0; i < ancount; i++) {
        ptr = skip_dns_name(ptr, buffer);
        if (ptr + 10 > end) return "";

        uint16_t type = ntohs(*(uint16_t*)ptr);
        ptr += 8; // TYPE + CLASS + TTL
        uint16_t rdlen = ntohs(*(uint16_t*)ptr);
        ptr += 2;

        if ((type == QTYPE_A && rdlen == 4) || (type == QTYPE_AAAA && rdlen == 16)) {
            char ip[INET6_ADDRSTRLEN];
            inet_ntop(type == QTYPE_A ? AF_INET : AF_INET6, ptr, ip, sizeof(ip));
            return std::string(ip);
        }

        ptr += rdlen;
    }

    return "";
}

