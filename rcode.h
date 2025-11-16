#pragma once

// DNS Response Code (RCODE) definitions — RFC 1035
enum RCODE {
    RCODE_NO_ERROR        = 0,  // No error condition
    RCODE_FORMAT_ERROR    = 1,  // Unable to interpret the query
    RCODE_SERVER_FAILURE  = 2,  // Problem with the name server
    RCODE_NAME_ERROR      = 3,  // Domain name does not exist (authoritative only)
    RCODE_NOT_IMPLEMENTED = 4,  // Query type not supported
    RCODE_REFUSED         = 5,  // Refused for policy reasons
};

// Function prototype
const char* RCODE_to_string(enum RCODE code);
