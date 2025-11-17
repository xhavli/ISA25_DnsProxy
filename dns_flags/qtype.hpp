#pragma once

// DNS Question Type (QTYPE) defined in RFC 1035
enum QTYPE {
    QTYPE_A       = 1,    // a host address (IPv4)
    QTYPE_NS      = 2,    // an authoritative name server
    QTYPE_MD      = 3,    // a mail destination (obsolete - use MX)
    QTYPE_MF      = 4,    // a mail forwarder (obsolete - use MX)
    QTYPE_CNAME   = 5,    // the canonical name for an alias
    QTYPE_SOA     = 6,    // start of a zone of authority
    QTYPE_MB      = 7,    // a mailbox domain name (experimental)
    QTYPE_MG      = 8,    // a mail group member (experimental)
    QTYPE_MR      = 9,    // a mail rename domain name (experimental)
    QTYPE_NULL    = 10,   // a null RR (experimental)
    QTYPE_WKS     = 11,   // well known service description
    QTYPE_PTR     = 12,   // a domain name pointer
    QTYPE_HINFO   = 13,   // host information
    QTYPE_MINFO   = 14,   // mailbox or mail list information
    QTYPE_MX      = 15,   // mail exchange
    QTYPE_TXT     = 16,   // text strings
    QTYPE_AAAA    = 28,   // IPv6 address
    QTYPE_SRV     = 33,   // service locator (RFC 2782)
    QTYPE_OPT     = 41,   // EDNS0 option (RFC 6891)
    QTYPE_AXFR    = 252,  // request for a transfer of an entire zone
    QTYPE_MAILB   = 253,  // request for mailbox-related records (MB, MG, MR)
    QTYPE_MAILA   = 254,  // request for mail agent records (obsolete)
    QTYPE_ANY     = 255   // request for all records
};

const char* QTYPE_to_string(enum QTYPE qtype);