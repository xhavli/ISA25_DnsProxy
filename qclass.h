#pragma once

// DNS QCLASS codes as defined in RFC 1035 and common extensions.
// Used in the Question section of a DNS message and equals TYPE in answer section.

enum QCLASS {
    QCLASS_IN     = 1,    // the Internet
    // QCLASS_CS     = 2,    // the CSNET class (obsolete)
    // QCLASS_CH     = 3,    // the CHAOS class
    // QCLASS_HS     = 4,    // Hesiod
};
