#pragma once

#include <mls/common.h>
#include <mls/core_types.h>
#include <mls/credential.h>
#include <mls/crypto.h>


#include <hpke/digest.h>
#include <hpke/hpke.h>
#include <hpke/random.h>
#include <hpke/signature.h>



namespace mls {
    struct ReverseFirewall{
        CipherSuite cipher_suite;

        ReverseFirewall(CipherSuite);

        bool check_encryption( bytes&, HPKEPublicKey);

        void re_randomize(bytes&, HPKEPublicKey);
    };
} // namespace mls
