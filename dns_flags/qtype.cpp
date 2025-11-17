#include "qtype.hpp"

const char* QTYPE_to_string(enum QTYPE qtype) {
    switch (qtype) {
        case QTYPE_A:       return "A";
        case QTYPE_NS:      return "NS";
        case QTYPE_MD:      return "MD";
        case QTYPE_MF:      return "MF";
        case QTYPE_CNAME:   return "CNAME";
        case QTYPE_SOA:     return "SOA";
        case QTYPE_MB:      return "MB";
        case QTYPE_MG:      return "MG";
        case QTYPE_MR:      return "MR";
        case QTYPE_NULL:    return "NULL";
        case QTYPE_WKS:     return "WKS";
        case QTYPE_PTR:     return "PTR";
        case QTYPE_HINFO:   return "HINFO";
        case QTYPE_MINFO:   return "MINFO";
        case QTYPE_MX:      return "MX";
        case QTYPE_TXT:     return "TXT";
        case QTYPE_AAAA:    return "AAAA";
        case QTYPE_SRV:     return "SRV";
        case QTYPE_OPT:     return "OPT";
        case QTYPE_AXFR:    return "AXFR";
        case QTYPE_MAILB:   return "MAILB";
        case QTYPE_MAILA:   return "MAILA";
        case QTYPE_ANY:     return "ANY";
        default:            return "UNKNOWN";
    }
}

const char* QTYPE_to_string(enum QTYPE qtype);
