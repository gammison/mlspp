#pragma once

#include <mls/common.h>
#include <mls/core_types.h>
#include <mls/credential.h>
#include <mls/crypto.h>

namespace mls {
    //   ProtocolVersion version = mls10;
    //   CipherSuite cipher_suite;
    //   EncryptedGroupSecrets group_secretss<1..2^32-1>;
    //   opaque encrypted_group_info<1..2^32-1>;
    struct RevFirewall{
        ProtocolVersion version;
        CipherSuite cipher_suite;
        std::vector<EncryptedGroupSecrets> secrets
        bytes encrypted_group_info;

        RevFirewall();
        RevFirewall(CipherSuite suite,
                    const bytes& jointer_secret,
                    const bytes& psk_secret,
                    const GroupInfo& group_info);

        bool check_encryption(const bytes& jointer_secret, const bytes& psk_secret);

        void re_randomize(const KeyPackages& kp, const std::optional<bytes>& path_secret);

        TLS_SERIALIZABLE(version, cipher_suite, secrets, encrypted_group_info)
        TLS_TRAITS(tls::pass, tls::pass, tls::vector<4>, tls::vector<4>)
    
        private:
            bytes _joiner_secret;
            static KeyAndNonce group_info_key_nonce(CipherSuite suite,
                                                    const bytes& jointer_secret,
                                                    const bytes& psk_secret);


    
    
    };
} // namespace mls
