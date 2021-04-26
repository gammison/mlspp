#include <mls/crypto.h>
#include <mls/log.h>
#include <mls/reverse_firewall.h>

using hpke::AEAD;      // NOLINT(misc-unused-using-decls)
using hpke::Digest;    // NOLINT(misc-unused-using-decls)
using hpke::HPKE;      // NOLINT(misc-unused-using-decls)
using hpke::KDF;       // NOLINT(misc-unused-using-decls)
using hpke::KEM;       // NOLINT(misc-unused-using-decls)
using hpke::Signature; // NOLINT(misc-unused-using-decls)

namespace mls{

ReverseFirewall::ReverseFirewall(crypto::CipherSuite in)
{
    cipher_suite = in;
}

ReverseFirewall
ReverseFirewall::check_encryption(bytes& secret, crypto::HPKEPublicKey pk)
{
    return false;
}

ReverseFirewall
ReverseFirewall::re_randomize(bytes& secret, crypto::HKPREPublicKey pk)
{
    return;
}

}
