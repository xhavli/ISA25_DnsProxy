/**
 * Project ISA25 Filter Resolver
 * Author: Adam Havlík (xhavli59)
 * Date: 17.11.2025
 */

#include <iostream>
#include <iomanip>
#include <vector>
#include <string>
#include <cstring>
#include <cstdint>
#include <cctype>
#include <csignal>
#include <atomic>
#include <algorithm>
#include <fstream>
#include <thread>

#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "qclass.hpp"
#include "qtype.hpp"
#include "rcode.hpp"
#include "proxy_config.hpp"
#include "print_helper.hpp"
#include "filter_helper.hpp"
#include "dns_structures.hpp"

volatile sig_atomic_t running = 1;
proxy_config config;
upstream_server upstream;

#include <fcntl.h>

void signal_handler([[maybe_unused]] int signal) { running = 0; }

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

void resolve_upstream(const std::string& host, upstream_server& up) {
    addrinfo hints{}, *res = nullptr;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_family = AF_UNSPEC;

    if (int ret = getaddrinfo(host.c_str(), nullptr, &hints, &res); ret != 0) {
        std::cerr <<"ERROR: Failed to resolve upstream: " << gai_strerror(ret) << "\n";
        exit(1);
    }

    for (auto* p = res; p; p = p->ai_next) {
        if (p->ai_family == AF_INET) {
            up.has_ipv4 = true;
            up.ipv4 = *reinterpret_cast<sockaddr_in*>(p->ai_addr);
            up.ipv4.sin_port = htons(53);
        } else if (p->ai_family == AF_INET6) {
            up.has_ipv6 = true;
            up.ipv6 = *reinterpret_cast<sockaddr_in6*>(p->ai_addr);
            up.ipv6.sin6_port = htons(53);
        }
    }

    freeaddrinfo(res);
    if (!up.has_ipv4 && !up.has_ipv6) {
        std::cerr <<"ERROR: upstream has no valid IPv4/IPv6 address\n";
        exit(1);
    }
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

                resolve_upstream(config.server, upstream);
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

int bind_ipv4(uint16_t port) {
    int sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;
    if (bind(sock_fd, (sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind ipv4");
        close(sock_fd);
        return -1;
    }

    return sock_fd;
}

int bind_ipv6(uint16_t port) {
    int sock_fd = socket(AF_INET6, SOCK_DGRAM, 0);
    int on = 1;
    setsockopt(sock_fd, IPPROTO_IPV6, IPV6_V6ONLY, &on, sizeof(on));
    sockaddr_in6 addr{};
    addr.sin6_family = AF_INET6;
    addr.sin6_port = htons(port);
    addr.sin6_addr = in6addr_any;
    if (bind(sock_fd, (sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind ipv6");
        close(sock_fd);
        return -1;
    }

    return sock_fd;
}

bool relay(uint8_t* data, ssize_t len, const dns_packet& pkt, int timeout_sec = 3) {
    // Prefer IPv4 if available
    int family = upstream.has_ipv4 ? AF_INET : AF_INET6;
    sockaddr* up_addr = upstream.has_ipv4
        ? (sockaddr*)&upstream.ipv4
        : (sockaddr*)&upstream.ipv6;
    socklen_t up_len = upstream.has_ipv4
        ? sizeof(sockaddr_in)
        : sizeof(sockaddr_in6);

    int sock = socket(family, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("ERROR: socket (upstream)");
        return false;
    }

    // Send to upstream
    if (sendto(sock, data, len, 0, up_addr, up_len) < 0) {
        perror("ERROR: sendto (upstream)");
        close(sock);
        return false;
    }

    // Set receive timeout for upstream reply
    uint8_t buffer[BUFFER_SIZE];
    struct timeval tv{timeout_sec, 0};
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    // Receive from upstream
    ssize_t recvd = recv(sock, buffer, sizeof(buffer), 0);
    if (recvd < 0) {
        perror("ERROR: recv (upstream)");
        close(sock);
        return false;
    }

    if (config.verbose) {
        std::string resolved_ip = extract_ip(buffer, recvd);
        if (!resolved_ip.empty()) {
            std::cout << "  Resolved: " << resolved_ip << "\n";
        } else {
            std::cout << "  No A/AAAA record in response\n";
        }
    }

    // Send response back to client
    if (sendto(pkt.sockfd, buffer, recvd, 0, (sockaddr*)&pkt.clientAddr, pkt.clientLen) < 0) {
        perror("ERROR: sendto (client)");
        close(sock);
        return false;
    }

    if(config.verbose) {
        std::cout << "  Response: " << RCODE_to_string(RCODE_NO_ERROR) << "\n";
    }

    close(sock);
    return true;
}

dns_query analyze_query(const dns_packet &pkt, const std::unordered_set<std::string> &filters, const proxy_config &cfg)
{
    dns_query query;
    if (pkt.length < DNS_HEADER_LENGTH)
        return query;

    // --- Parse header ---
    query.id = (pkt.data[0] << 8) | pkt.data[1];
    query.qdcount = (pkt.data[4] << 8) | pkt.data[5];

    // if qdcount != 1, special case, valid but currently not supported
    if (query.qdcount != 1){
        query.valid = true;
        query.blocked = false;
        return query;
    }

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
    std::string domain = query.qname;
    std::transform(domain.begin(), domain.end(), domain.begin(), ::tolower);

    // --- Check filter list ---
    query.blocked = is_blocked(domain, filters);

    query.valid = true;

    // --- Verbose logging ---
    if (cfg.verbose) {
        print_query(query, pkt);
    }

    return query;
}

void send_response(int sock_fd, const dns_packet &pkt, RCODE code) {
    if (pkt.length < DNS_HEADER_LENGTH) {
        std::cerr << "WARNING: Invalid DNS packet received\n";
        return;
    }

    if(config.verbose) {
        std::cout << "  Response: " << RCODE_to_string(code) << "\n";
    }

    uint8_t response[BUFFER_SIZE];
    memcpy(response, pkt.data, pkt.length);

    // QR = 1 (response)
    response[2] |= 0x80;
    // AA, TC, RD, RA, Z = 0 (not authoritative, not truncated, recursion desired/available = 0)
    response[2] &= 0x81; // keep only RD, others 0
    // Set RCODE
    response[3] = (response[3] & 0xF0) | (code & 0x0F);
    // ANCOUNT, NSCOUNT, ARCOUNT = 0
    response[6] = response[7] = 0;
    response[8] = response[9] = 0;
    response[10] = response[11] = 0;

    if(sendto(sock_fd, response, pkt.length, 0, reinterpret_cast<const sockaddr*>(&pkt.clientAddr), pkt.clientLen) < 0) {
        perror("ERROR: sendto (client)");
    }
}

// Worker per-socket
void worker(int sock, const std::unordered_set<std::string>& filters) {
    dns_packet pkt{};
    pkt.sockfd = sock;
    
    while (running) {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(sock, &fds);

        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 250000; // 250ms timeout

        int ret = select(sock + 1, &fds, nullptr, nullptr, &tv);

        if (ret < 0) {
            if (errno == EINTR) continue; // Interrupted by signal
            perror("ERROR: select");
            break;
        } else if (ret == 0) {
            continue; // timeout, loop around and check `running`
        }
        
        pkt.clientLen = sizeof(pkt.clientAddr);
        pkt.length = recvfrom(sock, pkt.data, BUFFER_SIZE, 0, (sockaddr*)&pkt.clientAddr, &pkt.clientLen);

        if (pkt.length < 0) {
            if (errno == EINTR) continue;
            perror("ERROR: recvfrom");
            continue;
        }

        dns_query query = analyze_query(pkt, filters, config);

        if (!query.valid) {
            send_response(sock, pkt, RCODE_FORMAT_ERROR);
        } else if (query.blocked) {
            send_response(sock, pkt, RCODE_REFUSED);
        } else if (query.qtype != QTYPE_A || query.qclass != QCLASS_IN || query.qdcount != 1) {
            send_response(sock, pkt, RCODE_NOT_IMPLEMENTED);
        } else if (!relay(pkt.data, pkt.length, pkt)) {
            send_response(sock, pkt, RCODE_SERVER_FAILURE);
        }

        if(config.verbose) {
            std::cout << "--------------------------------------" << std::endl;
        }
    }
}

int main(int argc, char *argv[]) {
    init_signal_handling();
    parse_arguments(argc, argv, config);

    if (config.verbose) { print_config(config, upstream); }
    std::unordered_set<std::string> filters = load_filters(config.filter_file, config.verbose);

    int ipv4_sock_fd = bind_ipv4(config.port);
    int ipv6_sock_fd = bind_ipv6(config.port);

    if (ipv4_sock_fd < 0 && ipv6_sock_fd < 0) {
        std::cerr <<"ERROR: could not bind IPv4 and IPv6 sockets\n";
        return 1;
    }
    else if (ipv4_sock_fd < 0) {
        std::cerr <<"ERROR: could not bind IPv4 socket\n";
    }
    else if (ipv6_sock_fd < 0) {
        std::cerr <<"ERROR: could not bind IPv6 socket\n";
    }

    std::vector<std::thread> threads;
    if (ipv4_sock_fd >= 0) threads.emplace_back(worker, ipv4_sock_fd, std::cref(filters));
    if (ipv6_sock_fd >= 0) threads.emplace_back(worker, ipv6_sock_fd, std::cref(filters));

    for (auto& thread : threads) thread.join();

    if (ipv4_sock_fd >= 0) close(ipv4_sock_fd);
    if (ipv6_sock_fd >= 0) close(ipv6_sock_fd);

    std::cout << std::endl << "DNS Proxy terminated successfully with exit code 0" << std::endl;
    return 0;
}
