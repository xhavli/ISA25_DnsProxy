# ISA DNS filtering resolver

![LogoFIT](docs/LogoFIT.png)

Author: Adam Havlik - xhavli59

Date: 17.11.2025

## About

A lightweight DNS proxy server implemented in C++ that filters A-record over UDP queries based on a blacklist. It forwards allowed requests to an upstream resolver and blocks unwanted domains with custom DNS replies.

Program support only A in IN records which can filter and refuse or relay to real DNS server and return result back to a client with No Error. Others are marked as Not Implemented and returned back to a client.

Task assignment available on [link](https://www.vut.cz/studis/student.phtml?script_name=zadani_detail&apid=294160&zid=61294)

Solution available on GitHub [repository](https://github.com/xhavli/ISA25_DnsProxy)

## Summary

- [Problem Introduction](#problem-introduction)
- [Program Dependencies](#program-dependencies)
- [Program Execution](#program-execution)
  - [Makefile](#makefile)
  - [Run Commands](#run-commands)
    - [CLI arguments](#cli-arguments)
  - [Filter File](#filter-file)
- [Application Output](#application-output)
- [Implementation Details](#implementation-details)
  - [Architecture](#architecture)
  - [Program Flow](#program-flow)
  - [Worker Code Snippet](#worker-code-snippet)
  - [Return Codes](#return-codes)
- [Tests](#tests)
  - [Manual Tests](#manual-tests)
  - [Automatic Tests](#automatic-tests)
- [Known Problems](#known-problems)
- [Bibliography](#bibliography)
- [Notes](#notes)

## Problem Introduction

This is a short introduction to pure DNS concepts and flow, not DNSSEC - secured version of DNS, DoH etc.

### TCP/IP Stack

Heres is a pictures displaying whats happening in TCP/IP model, its layers and some important protocols for this task on each layer or sample build packet.

![TCP/IP_STACK](docs/TcpIpStack.png)  
Picture taken from [ISA 2025 BASICS](#bibliography)

![TCP/IP_MODEL](docs/TcpIpModel.png)  
Picture taken from [ISA 2025 BASICS](#bibliography)

## DNS

The Domain Name System (DNS) is a hierarchical and distributed naming system that translates human-friendly domain names (like example.com) into IP addresses that computers use to identify each other on the network. Its primary purpose is to make accessing websites and online services easier for users by using readable names instead of complex numerical IP addresses. DNS operates in a client-server architecture, where a DNS client (often part of your operating system) sends a query to a DNS server to resolve a domain name. This request may be answered directly by the server if it has the information cached, or it may query other DNS servers recursively until the IP address is found. Once resolved, the IP address is returned to the client, enabling communication with the desired website or service.

Format of DNS communication is based on binary protocol described in [RFC1035](#bibliography).

- Transport Protocols on L4 layers

    According to _Assessing Support for DNS-over-TCP in the
    Wild_ [research paper](https://www.akamai.com/site/en/documents/research-paper/assessing-support-for-dns-over-tcp-in-the-wild.pdf) and many more articles common communication goes over UDP (more than 99%). Reason is simple, its much more faster than initiating a connection in case of TCP but theres a risk of packet loss based on nature of the UDP.

- Internet Protocols on L3 layers

    DNS protocol naturally support IPv4 and IPv6 internet protocols both on port 53. According to statistics from [Google](https://www.google.com/intl/en/ipv6/statistics.html#tab=ipv6-adoption) public DNS servers, majority communication incoming from IPv4 address and minority is on IPv6 (less than 50%). But we can observe a sharply rising trend from 2013 when it was around 2%.

![DnsProtocol](docs/DnsProtocol.png)  
Picture taken from [ISA 2025 DNS](#bibliography)

## Program Dependencies

- Language C++ std 17
- Compiler g++
- License AGPL-3.0
- Developed for linux

## Program Execution

As application is listening by default every outgoing traffic from interface on port 53, which is standard port for system DNS service it may requires a `sudo` permissions to run it in this configuration.

### Makefile

Makefile commands:

- `make` will compile program to a `dns` executable file
- `make clean` will remove `dns` executable file
- `make test` will run tests

### Run Commands

Provide every possible arguments:

- Use upstream DNS server by `dns.google`, listen on port `5300`, load the blocked domains from `filter_file.txt` and enable verbose output.

    ``` bash
    ./dns -s dns.google -p 5300 -f filter_file.txt -v
    ```

#### CLI arguments

| Name           | Argument | Need       | Default values | Possible values | Meaning or expected program behavior
| -------------- | -------- | ---------- | -------------- | --------------- | ----------------------------------------------------
| Server         | `-s`     | required   |                | `string`        | Specify domain of ip address of upstream DNS server
| Listen on port | `-p`     | optional   | `53`           | `uint_16`       | Set listening port of outgoing DNS queries
| Filter file    | `-f`     | required   |                | `string`        | Specify file with blocked domains and its subdomains
| Verbose        | `-v`     | optional   | false          |                 | Enable verbose output if provided

- In case some of optional argument `-p` will not be provided, "WARNING" will be shown and default values will be set

### Filter file

This chapter takes part about filter file syntax. Filter file is list of blocked domains or subdomains concatenated per line. Syntax allow line comments starts with `#` which mean ignore everything in this line after `#`. Logic also ignore whitespaces and protocol names (www/http). Wildcard is not allowed. every domain should be standard domain described in [RFC1035](#bibliography) and [RFC1123](https://datatracker.ietf.org/doc/html/rfc1123). Example file is available on this [link](https://pgl.yoyo.org/adservers/serverlist.php?hostformat=nohtml&showintro=1).

## Application Output

Application naturally does not print any unimportant outputs except warnings caused on setup to inform user about maybe unexpected configuration.

In verbose mode application print some details about incoming DNS queries to `STDOUT`, warnings and errors to `STDERR`.

Output examples:

- Configuration information:

    ```plaintext
    ======== DNS Proxy Configuration ========
    Server addr:   dns.google
    IPv4 upstream: 8.8.8.8
    IPv6 upstream: 2001:4860:4860::8888
    Listening on:  IPv4 IPv6
    Port:          5300
    Filter file:   filter_file.txt
    Verbose:       enabled
    ==========================================
    WARNING: Wildcards are not allowed on line 6: '*example.com'
    Loaded 3 filter rules
    ==========================================
    ```

- Received query with allowed domain over IPv4:

    ```plaintext
    Query received:
        From: 127.0.0.1:60541
        ID: 64316
        Name: google.com
        Type: 1 (A)
        Class: 1 (IN)
        --
        Blocked: NO
        Resolved: 142.251.36.110
        Response: RCODE_NO_ERROR
    ```

- Received query with allowed domain over IPv6:

    ```plaintext
    Query received:
        From: [::1]:45422
        ID: 20976
        Name: google.com
        Type: 1 (A)
        Class: 1 (IN)
        --
        Blocked: NO
        Resolved: 142.251.36.110
        Response: RCODE_NO_ERROR
    ```

- Received query with blocked domain over IPv4:

    ```plaintext
    Query received:
        From: 127.0.0.1:40969
        ID: 10758
        Name: ads.cybersales.cz
        Type: 1 (A)
        Class: 1 (IN)
        --
        Blocked: YES
        Response: RCODE_REFUSED
    ```

## Implementation Details

The application avoids object-oriented programming (OOP) in C++ and uses a plain C-style approach.

### Architecture

```plaintext
ISA25_DNSPROXY

├── dns_flags/
│   ├── qclass.cpp
│   ├── qclass.hpp
│   ├── qtype.cpp
│   ├── qtype.hpp
│   ├── rcode.cpp
│   └── rcode.hpp
│
├── docs/
│   ├── AllowedIPv4Query.png
│   ├── AllowedIPv6Query.png
│   ├── BlockedIPv4Query.png
│   ├── DnsProtocol.png
│   ├── LogoFIT.png
│   ├── TcpIpModel.png
│   └── TcpIpStack.png
│
├── filter_helper/
│   ├── filter_helper.cpp
│   └── filter_helper.hpp
│
├── print_helper/
│   ├── print_helper.cpp
│   └── print_helper.hpp
│
├── structures/
│   ├── dns_structures.hpp
│   └── proxy_config.hpp
│
├── LICENSE
├── main.cpp
├── Makefile
└── README.md
```

### Program Flow

- Init signal handling
- Parse arguments
- Resolve upstream DNS server
- Load filters
- Bind listeners
- Start listener threads
- Capture DNS query
- Analyze DNS query
- Refuse answer or relay to upstream server and then back to client
- Exit on Ctrl+C

```c++
int main(int argc, char *argv[]) {
    init_signal_handling();
    parse_arguments(argc, argv, config);

    std::unordered_set<std::string> filters = load_filters(config.filter_file, config.verbose);

    int ipv4_sock_fd = bind_ipv4(config.port);
    int ipv6_sock_fd = bind_ipv6(config.port);

    if (ipv4_sock_fd < 0 && ipv6_sock_fd < 0) {
        std::cerr <<"ERROR: could not bind IPv4 or IPv6 sockets\n";
        return 1;
    }

    std::vector<std::thread> threads;
    if (ipv4_sock_fd >= 0) threads.emplace_back(worker, ipv4_sock_fd, std::cref(filters));
    if (ipv6_sock_fd >= 0) threads.emplace_back(worker, ipv6_sock_fd, std::cref(filters));

    for (auto& thread : threads) thread.join();

    if (ipv4_sock_fd >= 0) close(ipv4_sock_fd);
    if (ipv6_sock_fd >= 0) close(ipv6_sock_fd);

    return 0;
}
```

### Worker Code Snippet

```c++
void worker(int sock, const std::unordered_set<std::string>& filters) {
    dns_packet pkt{};
    pkt.sockfd = sock;
    pkt.clientLen = sizeof(pkt.clientAddr);
    
    while (running) {
        pkt.length = recvfrom(sock, pkt.data, BUFFER_SIZE, 0, (sockaddr*)&pkt.clientAddr, &pkt.clientLen);

        dns_query query = analyze_query(pkt, filters, config);

        if (!query.valid) {
            send_response(sock, pkt, RCODE_FORMAT_ERROR);
        } else if (query.blocked) {
            send_response(sock, pkt, RCODE_REFUSED);
        } else if (query.qtype != QTYPE_A || query.qclass != QCLASS_IN) {
            send_response(sock, pkt, RCODE_NOT_IMPLEMENTED);
        } else if (!relay(pkt.data, pkt.length, pkt)) {
            send_response(sock, pkt, RCODE_SERVER_FAILURE);
        }
    }
}
```

### Return Codes

- 0 on success
- 1 on any error

## Tests

### Manual Tests

- DNS query with allowed domain over IPv4:

    ```bash
    - dig @127.0.0.1 -p 5300 google.com A +short +tries=1 +retry=0
    ```

    ![AllowedIPv4Query](docs/AllowedIPv4Query.png)

- DNS query with allowed domain over IPv6:

    ```bash
    - dig @[::1] -p 5300 google.com A +short +tries=1 +retry=0
    ```

    ![AllowedIPv6Query](docs/AllowedIPv6Query.png)

- DNS query with blocked domain over IPv4:

    ```bash
    - dig @[::1] -p 5300 ads.cybersales.cz A +short +tries=1 +retry=0
    ```

    ![BlockedIPv4Query](docs/BlockedIPv4Query.png)

### Automatic Tests

## Known Problems

- Using of global variables in this project
- Used blocking sockets
- Refresh rate - active waiting for `Ctrl+C` interrupt set to 250 milliseconds
- This is caused because author think portable code mean portable across Linux distributions, MacOS and Windows. New knowledge is that mean portable between BSD based operating systems which are usually some of linux distributions

## Bibliography

[ISA 2025 BASICS] Matoušek, P. _Architektura sítí, adresování, testování_. September 2025. [cited 17.11.2025]. Available at: <https://moodle.vut.cz/pluginfile.php/1174611/mod_resource/content/7/isa-architektura.pdf>

[ISA 2025 DNS] Matoušek, P. _Systém DNS_. September 2025. [cited 17.11.2025]. Available at: <https://moodle.vut.cz/pluginfile.php/1174654/mod_resource/content/10/isa-dns.pdf>

[RFC1035] _Domain Names - Implementation and Specification_ available at: <https://datatracker.ietf.org/doc/html/rfc1035>

[RFC791] _Internet Protocol_ available at: <https://datatracker.ietf.org/doc/html/rfc791>

[RFC2460] _Internet Protocol, Version 6 (IPv6) Specification_ available at: <https://datatracker.ietf.org/doc/html/rfc2460>

[RFC768] _User Datagram Protocol (UDP)_ available at: <https://datatracker.ietf.org/doc/html/rfc768>

## Notes

- Program was developed with support of ChatGPT and GithubCopilot for better understanding a C++ syntax, not for direct solving core of the project
