#pragma once

#include <cstdint>
#include <netinet/in.h>
#include <string>

constexpr int BUFFER_SIZE = 512; // Standard DNS packet size over UDP
constexpr int DNS_HEADER_LENGTH = 12;


struct dns_packet {
    uint8_t data[BUFFER_SIZE];
    ssize_t length;
    sockaddr_storage clientAddr;
    socklen_t clientLen;
};


struct dns_query {
    bool valid = false;
    bool blocked = false;
    std::string qname;
    uint16_t qtype = 0;
    uint16_t qclass = 0;
    uint16_t id = 0;
};
