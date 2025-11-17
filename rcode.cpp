#include "rcode.hpp"

const char* RCODE_to_string(enum RCODE code) {
    switch (code) {
        case RCODE_NO_ERROR:        return "RCODE_NO_ERROR";
        case RCODE_FORMAT_ERROR:    return "RCODE_FORMAT_ERROR";
        case RCODE_SERVER_FAILURE:  return "RCODE_SERVER_FAILURE";
        case RCODE_NAME_ERROR:      return "RCODE_NAME_ERROR";
        case RCODE_NOT_IMPLEMENTED: return "RCODE_NOT_IMPLEMENTED";
        case RCODE_REFUSED:         return "RCODE_REFUSED";
        default:                    return "UNKNOWN";
    }
}
