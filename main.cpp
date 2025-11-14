#include <iostream>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <stdexcept>
#include <unistd.h>
#include <vector>
#include <algorithm>
#include <fstream>

#include <cstring>
#include <string>
#include <format>
#include <cstdint>
#include <cctype>
#include <iomanip>

#include <csignal>
#include <signal.h>
#include <atomic>

#include "qclass.h"
#include "qtype.h"
#include "rcode.h"
#include "proxy_config.h"

using byte = uint8_t;

constexpr int BUF_SIZE = 512; // Standard DNS packet size over UDP
volatile sig_atomic_t running = 1;
proxy_config config;

struct dns_packet {
    uint8_t data[BUF_SIZE];
    int length{};
    sockaddr_in clientAddr{};
    socklen_t clientLen{};
};

void signal_handler([[maybe_unused]] int signal) {
    running = 0;
}

void init_signal_handling() {
    std::signal(SIGINT, signal_handler);  // Ctrl+C
    std::signal(SIGTERM, signal_handler); // Termination signal kill

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0; // Disable SA_RESTART, so recvfrom() returns EINTR

    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }
    if (sigaction(SIGTERM, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }
}

void print_usage(const char* prog) {
    std::cerr << "Usage: " << prog << " -s server [-p port] -f filter_file [-v]\n";
}

void resolve_host_ipv4(const std::string& host, in_addr* out_addr) {
    if (!out_addr) return;

    // 1) Numeric IPv4?
    in_addr tmp{};
    if (inet_pton(AF_INET, host.c_str(), &tmp) == 1) {
        *out_addr = tmp;
        return;
    }

    // 2) Resolve hostname → IPv4
    addrinfo hints{}, *res = nullptr;
    hints.ai_family   = AF_INET;     // IPv4 only (DNS over UDP here)
    hints.ai_socktype = SOCK_DGRAM;  // UDP
    hints.ai_flags    = 0;

    int ret = getaddrinfo(host.c_str(), nullptr, &hints, &res);
    if (ret != 0 || !res) {
        std::cerr << "ERROR: Failed to resolve host '" << host << "': " << gai_strerror(ret) << "\n";
        if (res) freeaddrinfo(res);
        exit(EXIT_FAILURE);
    }

    *out_addr = reinterpret_cast<sockaddr_in*>(res->ai_addr)->sin_addr;
    freeaddrinfo(res);
    return;
}

uint16_t parse_port(const char* optarg) {
    if (!optarg || !*optarg) {
        std::cerr << "WARNING: Missing port value. Using default 53.\n";
        return 53;
    }

    // Validate if the entire string contains only digits
    size_t pos = 0;
    for (; pos < std::strlen(optarg); ++pos) {
        if (!std::isdigit(static_cast<unsigned char>(optarg[pos]))) break;
    }

    if (pos != std::strlen(optarg)) {
        std::cerr << "WARNING: Port '" << optarg << "' contains non-numeric characters. Using default 53.\n";
        return 53;
    }

    int value = std::atoi(optarg);
    if (value <= 0 || value > 65535) {
        std::cerr << "WARNING: Port '" << value << "' is out of range (1-65535). Using default 53.\n";
        return 53;
    }

    return static_cast<uint16_t>(value);
}

void parse_arguments(int argc, char *argv[], proxy_config &config) {
    if (argc < 3) {
        print_usage(argv[0]);
        exit(EXIT_FAILURE);
    }

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "-s") == 0) {
            if (i + 1 >= argc) {
                std::cerr << "ERROR: missing argument for -s\n";
                exit(EXIT_FAILURE);
            }
            config.server = argv[++i];

            resolve_host_ipv4(config.server, &config.server_ip);
        }
        else if (std::strcmp(argv[i], "-p") == 0) {
            if (i + 1 >= argc) {
                std::cerr << "ERROR: missing argument for -p\n";
                exit(EXIT_FAILURE);
            }
            config.port = parse_port(argv[++i]);
        }
        else if (std::strcmp(argv[i], "-f") == 0) {
            if (i + 1 >= argc) {
                std::cerr << "ERROR: missing argument for -f\n";
                exit(EXIT_FAILURE);
            }
            config.filter_file = argv[++i];
        }
        else if (std::strcmp(argv[i], "-v") == 0) {
            config.verbose = true;
        }
        else {
            std::cerr << "ERROR: unknown option '" << argv[i] << "'\n";
            print_usage(argv[0]);
            exit(EXIT_FAILURE);
        }
    }

    if (config.server.empty()) {
        std::cerr << "ERROR: missing required -s <server>\n";
        print_usage(argv[0]);
        exit(EXIT_FAILURE);
    }

    if (config.filter_file.empty()) {
        std::cerr << "ERROR: missing required -f <filter_file>\n";
        print_usage(argv[0]);
        exit(EXIT_FAILURE);
    }
}

int make_udp_socket_with_timeout(int sec = 3) {
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    timeval tv{sec, 0};
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    return sockfd;
}

bool forward_and_relay(int listen_sock, const dns_packet &pkt, const proxy_config &cfg) {
    // Build upstream sockaddr_in completely
    sockaddr_in upstream{};
    upstream.sin_family = AF_INET;
    upstream.sin_port   = htons(53); // DNS port
    upstream.sin_addr   = cfg.server_ip;

    int upstreamfd = make_udp_socket_with_timeout();
    if (upstreamfd < 0) return false;

    ssize_t sent = sendto(upstreamfd, pkt.data, pkt.length, 0, reinterpret_cast<sockaddr*>(&upstream), sizeof(upstream));
    if (sent < 0) {
        perror("sendto (upstream)");
        close(upstreamfd);
        return false;
    }

    uint8_t buf[BUF_SIZE];
    ssize_t rcv = recv(upstreamfd, buf, sizeof(buf), 0);
    if (rcv < 0) {
        perror("recv(upstream)");
        close(upstreamfd);
        return false;
    }

    if (sendto(listen_sock, buf, rcv, 0,(sockaddr*)&pkt.clientAddr, pkt.clientLen) < 0) {
        perror("sendto(client)");
        close(upstreamfd);
        return false;
    }

    if (cfg.verbose) {
        char cip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &pkt.clientAddr.sin_addr, cip, sizeof(cip));
        std::cout << "Relayed " << rcv << " bytes from " << cfg.server
                  << " to " << cip << ":" << ntohs(pkt.clientAddr.sin_port) << "\n";
    }

    close(upstreamfd);
    return true;
}

std::vector<std::string> load_filters(const std::string &filename) {
    std::vector<std::string> filters;
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Failed to open filter file: " << filename << "\n";
        return filters;
    }

    std::string line;
    while (std::getline(file, line)) {
        // Trim whitespace
        line.erase(0, line.find_first_not_of(" \t\r\n"));
        line.erase(line.find_last_not_of(" \t\r\n") + 1);
        if (line.empty() || line[0] == '#') continue;

        // Normalize to lowercase
        std::transform(line.begin(), line.end(), line.begin(), ::tolower);
        filters.push_back(line);
    }

    if (config.verbose) {
        std::cout << "Loaded " << filters.size() << " blocked domains from " << filename << std::endl;
        std::cout << "================================" << std::endl;
    }

    return filters;
}

std::string extract_domain(const dns_packet &pkt) {
    if (pkt.length < 12) return "";
    int offset = 12;    // Skip DNS 12 bytes header
    std::string name;

    while (offset < pkt.length && pkt.data[offset] != 0) {
        int len = pkt.data[offset++];
        if (offset + len > pkt.length) return "";
        if (!name.empty()) name += ".";
        name.append(reinterpret_cast<const char*>(&pkt.data[offset]), len);
        offset += len;
    }

    return name;
}

bool is_blocked(const std::string &domain, const std::vector<std::string> &filters) {
    std::string d = domain;
    std::transform(d.begin(), d.end(), d.begin(), ::tolower);

    for (const auto &blocked : filters) {
        if (d == blocked) return true;
        if (d.size() > blocked.size() &&
            d.compare(d.size() - blocked.size(), blocked.size(), blocked) == 0 &&
            d[d.size() - blocked.size() - 1] == '.') {
            return true; // subdomain match
        }
    }

    return false;
}

void process_query(const dns_packet &pkt) {
    if (pkt.length < 12) {
        std::cerr << "Invalid packet length\n";
        return;
    }

    // --- Parse header ---
    uint16_t id = (pkt.data[0] << 8) | pkt.data[1];
    uint16_t qdcount = (pkt.data[4] << 8) | pkt.data[5];

    // --- Parse question section ---
    int offset = 12; // DNS header is 12 bytes
    std::string qname;

    // extract QNAME (sequence of labels)
    while (offset < pkt.length && pkt.data[offset] != 0) {
        int label_len = pkt.data[offset++];
        if (offset + label_len > pkt.length) return;
        if (!qname.empty()) qname += ".";
        qname.append(reinterpret_cast<const char*>(&pkt.data[offset]), label_len);
        offset += label_len;
    }

    offset++; // skip null byte

    if (offset + 4 > pkt.length) return;

    uint16_t qtype = (pkt.data[offset] << 8) | pkt.data[offset + 1];
    uint16_t qclass = (pkt.data[offset + 2] << 8) | pkt.data[offset + 3];

    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &pkt.clientAddr.sin_addr, client_ip, sizeof(client_ip));
    int client_port = ntohs(pkt.clientAddr.sin_port);

    std::cout << "Query received:\n";
    std::cout << "  ID: " << id << "\n";
    std::cout << "  From: " << client_ip << ":" << client_port << "\n";
    std::cout << "  Questions: " << qdcount << "\n";
    std::cout << "  Name: " << qname << "\n";
    std::cout << "  Type: " << qtype << " ("
              << (qtype == 1 ? "A" : qtype == 28 ? "AAAA" : "Other") << ")\n";
    std::cout << "  Class: " << qclass << "\n";
}

int create_socket(uint16_t port = 53) {
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);    // IPv4 UDP socket
    if (sockfd < 0) {
        perror("socket");
        return -1;
    }

    sockaddr_in local{};
    local.sin_family = AF_INET; // IPv4
    local.sin_addr.s_addr = INADDR_ANY; // Bind to all interfaces
    local.sin_port = htons(port);   // Host to network byte order

    if (bind(sockfd, reinterpret_cast<sockaddr*>(&local), sizeof(local)) < 0) {
        perror("bind");
        close(sockfd);
        return -1;
    }

    return sockfd;
}

bool receive_query(int sockfd, dns_packet &pkt) {
    pkt.clientLen = sizeof(pkt.clientAddr);
    pkt.length = recvfrom(sockfd, pkt.data, BUF_SIZE, 0, reinterpret_cast<sockaddr*>(&pkt.clientAddr), &pkt.clientLen);

    if (pkt.length < 0) {
        if (!running) return false;
        perror("recvfrom");
        return false;
    }

    return true;
}

void send_response(int sockfd, const dns_packet &pkt, RCODE code) {
    if (pkt.length < 12) {
        // DNS header is at least 12 bytes
        std::cerr << "Invalid DNS packet received\n";
        return;
    }

    uint8_t response[BUF_SIZE];
    memcpy(response, pkt.data, pkt.length);

    // QR = 1 (response)
    response[2] |= 0x80;
    // AA, TC, RD, RA, Z = 0 (not authoritative, not truncated, recursion desired/available = 0)    //TODO mention RA in documentation
    response[2] &= 0x81; // keep only RD, others 0
    // Set RCODE
    response[3] = (response[3] & 0xF0) | (code & 0x0F);
    // ANCOUNT, NSCOUNT, ARCOUNT = 0
    response[6] = response[7] = 0;
    response[8] = response[9] = 0;
    response[10] = response[11] = 0;

    ssize_t sent = sendto(sockfd, response, pkt.length, 0, reinterpret_cast<const sockaddr*>(&pkt.clientAddr), pkt.clientLen);

    if (sent < 0)
        perror("sendto (response)");
}

int main(int argc, char *argv[]) {
    init_signal_handling();

    parse_arguments(argc, argv, config);

    if (config.verbose) {
        print_config(config);
    }

    auto filters = load_filters(config.filter_file);

    int sockfd = create_socket(config.port);
    if (sockfd < 0) {
        perror("ERROR: creating socket");
        return 1;
    }

    while (running) {
        dns_packet pkt{};
        if (!receive_query(sockfd, pkt))
            continue;

        process_query(pkt);

        std::string qname = extract_domain(pkt);
        if (qname.empty()) {
            std::cerr << "WARNING: Failed to parse query\n";
            continue;
        }

        bool blocked = is_blocked(qname, filters);
        if (blocked) {
            std::cout << "  RESPONSE: Blocked (RCODE_REFUSED)\n";
            send_response(sockfd, pkt, RCODE_REFUSED);
        } else {
            // Only A queries over UDP are supported here. Everything else => NOT_IMPLEMENTED
            int off = 12;
            while (off < pkt.length && pkt.data[off] != 0) off += 1 + pkt.data[off];
            off++; // zero
            if (off + 4 > pkt.length) { send_response(sockfd, pkt, RCODE_FORMAT_ERROR); continue; }
            
            uint16_t qtype  = (pkt.data[off] << 8) | pkt.data[off+1];
            uint16_t qclass = (pkt.data[off+2] << 8) | pkt.data[off+3];
            uint16_t qdcount = (pkt.data[4] << 8) | pkt.data[5];
            
            if (qclass != QCLASS_IN) {  // IN only
                std::cout << "  RESPONSE: QCLASS " << qclass << " (RCODE_NOT_IMPLEMENTED)\n";
                send_response(sockfd, pkt, RCODE_NOT_IMPLEMENTED);
                continue; 
            }
            if (qtype  != QTYPE_A) {    // A only
                std::cout << "  RESPONSE: QTYPE " << qtype << " (RCODE_NOT_IMPLEMENTED)\n";
                send_response(sockfd, pkt, RCODE_NOT_IMPLEMENTED);
                continue;
            }

            if (qdcount != 1) { // Only 1 question supported
                send_response(sockfd, pkt, RCODE_FORMAT_ERROR);
                continue;
            }
            
            // Forward to real resolver and relay the answer
            if (!forward_and_relay(sockfd, pkt, config)) {
                send_response(sockfd, pkt, RCODE_SERVER_FAILURE);
            }
            std::cout << "  RESPONSE: Allowed (RCODE_NO_ERROR)\n";
        }
        std::cout << "------------------------------------" << std::endl;
    }

    close(sockfd);
    std::cout << std::endl << "Proxy terminated successfully" << std::endl;
    return 0;
    // TODO dig @127.0.0.1 -p 5300 example.com A +short +tries=1 +retry=0
}
