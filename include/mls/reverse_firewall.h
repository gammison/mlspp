#pragma once

#include <mls/common.h>
#include <mls/core_types.h>
#include <mls/credential.h>
#include <mls/crypto.h>

namespace mls {
    struct ReverseFirewall{
        CipherSuite cipher_suite;

        ReverseFirewall(CipherSuite suite);

        bool check_encryption( bytes& secret, HPKEPublicKey pk);

        void re_randomize(bytes& secret, HPKEPublicKey pk);
    };
} // namespace mls
