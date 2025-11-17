#pragma once

// DNS Question Class (QCLASS) defined in RFC 1035
enum QCLASS {
    QCLASS_IN     = 1,    // the Internet
    QCLASS_CS     = 2,    // the CSNET class (obsolete)
    QCLASS_CH     = 3,    // the CHAOS class
    QCLASS_HS     = 4,    // Hesiod
};

const char* QCLASS_to_string(enum QCLASS qclass);
