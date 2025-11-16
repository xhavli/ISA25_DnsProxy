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

#include "qclass.hpp"
#include "qtype.hpp"
#include "rcode.hpp"
#include "proxy_config.hpp"
#include "print_helper.hpp"
#include "filter_helper.hpp"
#include "dns_structures.hpp"

volatile sig_atomic_t running = 1;
proxy_config config;

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



int make_udp_socket_with_timeout(int sec) {
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

    int upstream_sock_fd = make_udp_socket_with_timeout(3);
    if (upstream_sock_fd < 0) return false;

    ssize_t sent = sendto(upstream_sock_fd, pkt.data, pkt.length, 0, reinterpret_cast<sockaddr*>(&upstream), sizeof(upstream));
    if (sent < 0) {
        perror("sendto (upstream)");
        close(upstream_sock_fd);
        return false;
    }

    uint8_t buf[BUFFER_SIZE];
    ssize_t rcv = recv(upstream_sock_fd, buf, sizeof(buf), 0);
    if (rcv < 0) {
        perror("recv(upstream)");
        close(upstream_sock_fd);
        return false;
    }

    if (sendto(listen_sock, buf, rcv, 0,(sockaddr*)&pkt.clientAddr, pkt.clientLen) < 0) {
        perror("sendto(client)");
        close(upstream_sock_fd);
        return false;
    }

    if (cfg.verbose) {
        char cip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &pkt.clientAddr.sin_addr, cip, sizeof(cip));
        std::cout << "  Relayed " << rcv << " bytes from " << cfg.server
                  << " to " << cip << ":" << ntohs(pkt.clientAddr.sin_port) << "\n";
    }

    close(upstream_sock_fd);
    return true;
}

dns_query analyze_query(const dns_packet &pkt, const std::vector<filter_rule> &filters, const proxy_config &cfg)
{
    dns_query query;
    if (pkt.length < DNS_HEADER_LENGTH)
        return query;

    // --- Parse header ---
    query.id = (pkt.data[0] << 8) | pkt.data[1];
    uint16_t qdcount = (pkt.data[4] << 8) | pkt.data[5];

    if (qdcount != 1)
        return query;

    // --- Extract QNAME ---
    int offset = DNS_HEADER_LENGTH;
    std::string qname;

    while (offset < pkt.length && pkt.data[offset] != 0) {
        int label_len = pkt.data[offset++];
        if (offset + label_len > pkt.length) return query;

        if (!qname.empty()) qname += ".";
        qname.append(reinterpret_cast<const char*>(&pkt.data[offset]), label_len);
        offset += label_len;
    }

    offset++; // Skip null terminator

    if (offset + 4 > pkt.length)
        return query;

    query.qname = qname;

    query.qtype  = (pkt.data[offset] << 8) | pkt.data[offset + 1];
    query.qclass = (pkt.data[offset + 2] << 8) | pkt.data[offset + 3];

    // --- Normalize for checking ---
    std::string d = query.qname;
    std::transform(d.begin(), d.end(), d.begin(), ::tolower);

    // --- Check filter list ---
    query.blocked = is_blocked(d, filters);

    query.valid = true;

    // --- Verbose logging ---
    if (cfg.verbose) {
        print_query(query, pkt);
    }

    return query;
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
    pkt.length = recvfrom(sockfd, pkt.data, BUFFER_SIZE, 0, reinterpret_cast<sockaddr*>(&pkt.clientAddr), &pkt.clientLen);

    if (pkt.length < 0) {
        if (!running) return false;
        perror("recvfrom");
        return false;
    }

    return true;
}

void send_response(int sockfd, const dns_packet &pkt, RCODE code) {
    if (pkt.length < DNS_HEADER_LENGTH) {
        std::cerr << "Invalid DNS packet received\n";
        return;
    }

    uint8_t response[BUFFER_SIZE];
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

    auto filters = load_filters(config.filter_file, config.verbose);

    int wellcome_sock_fd = create_socket(config.port);
    if (wellcome_sock_fd < 0) {
        perror("ERROR: creating socket");
        return 1;
    }

    while (running) {
        dns_packet pkt{};
        if (!receive_query(wellcome_sock_fd, pkt))
            continue;

        auto qi = analyze_query(pkt, filters, config);

        if (!qi.valid) {
            send_response(wellcome_sock_fd, pkt, RCODE_FORMAT_ERROR);
            continue;
        }

        if (qi.blocked) {
            std::cout << "  RESPONSE: Blocked (RCODE_REFUSED)\n";
            send_response(wellcome_sock_fd, pkt, RCODE_REFUSED);
            continue;
        }

        // Only support A IN queries
        if (qi.qclass != QCLASS_IN || qi.qtype != QTYPE_A) {
            std::cout << "  RESPONSE: Not implemented\n";
            send_response(wellcome_sock_fd, pkt, RCODE_NOT_IMPLEMENTED);
            continue;
        }

        // Relay upstream
        if (!forward_and_relay(wellcome_sock_fd, pkt, config)) {
            send_response(wellcome_sock_fd, pkt, RCODE_SERVER_FAILURE);
        }

        std::cout << "  RESPONSE: Allowed (RCODE_NO_ERROR)\n";
        std::cout << "------------------------------------\n";
    }

    close(wellcome_sock_fd);
    std::cout << std::endl << "Proxy terminated successfully" << std::endl;
    return 0;
    // TODO dig @127.0.0.1 -p 5300 example.com A +short +tries=1 +retry=0
}
