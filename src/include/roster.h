#pragma once

#include "common.h"
#include "crypto.h"
#include "tls_syntax.h"
#include <iosfwd>

namespace mls {

#define DUMMY_SIG_SCHEME SignatureScheme::P256_SHA256

class RawKeyCredential
{
public:
  RawKeyCredential()
    : _key(DUMMY_SIG_SCHEME)
  {}

  RawKeyCredential(const SignaturePublicKey& key)
    : _key(key)
  {}

  bytes identity() const { return _key.to_bytes(); }

  SignaturePublicKey public_key() const { return _key; }

private:
  SignaturePublicKey _key;

  friend bool operator==(const RawKeyCredential& lhs,
                         const RawKeyCredential& rhs);
  friend tls::ostream& operator<<(tls::ostream& out,
                                  const RawKeyCredential& roster);
  friend tls::istream& operator>>(tls::istream& in, RawKeyCredential& roster);
};

// XXX(rlb@ipv.sx): We have to subclass optional<T> in order to
// ensure that credentials are populated with blank values on
// unmarshal.  Otherwise, `*opt` will access uninitialized memory.
class OptionalRawKeyCredential : public optional<RawKeyCredential>
{
public:
  typedef optional<RawKeyCredential> parent;
  using parent::parent;

  OptionalRawKeyCredential()
    : parent(RawKeyCredential())
  {}
};

// TODO(rlb@ipv.sx): Figure out how to generalize to more types of
// credential
class Roster
{
public:
  void add(const RawKeyCredential& public_key);
  void remove(uint32_t index);
  RawKeyCredential get(uint32_t index) const;
  size_t size() const;

private:
  tls::vector<OptionalRawKeyCredential, 4> _credentials;

  friend bool operator==(const Roster& lhs, const Roster& rhs);
  friend tls::ostream& operator<<(tls::ostream& out, const Roster& roster);
  friend tls::istream& operator>>(tls::istream& in, Roster& roster);
};

} // namespace mls