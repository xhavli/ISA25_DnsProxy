/**
 * Project ISA25 Filter Resolver
 * Author: Adam Havlík (xhavli59)
 * Date: 17.11.2025
 */

#include "qclass.hpp"

const char* QCLASS_to_string(enum QCLASS qclass) {
    switch (qclass) {
        case QCLASS_IN:   return "IN";
        case QCLASS_CS:   return "CS";
        case QCLASS_CH:   return "CH";
        case QCLASS_HS:   return "HS";
        default:          return "UNKNOWN";
    }
}
