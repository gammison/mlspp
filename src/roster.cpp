#include "roster.h"

namespace mls {

///
/// CredentialType
///

tls::ostream&
operator<<(tls::ostream& out, CredentialType type)
{
  return out << uint8_t(type);
}

tls::istream&
operator>>(tls::istream& in, CredentialType& type)
{
  uint8_t temp;
  in >> temp;
  type = CredentialType(temp);
  return in;
}

///
/// BasicCredential
///

AbstractCredential*
BasicCredential::dup() const
{
  return new BasicCredential(_identity, _public_key);
}

bytes
BasicCredential::identity() const
{
  return _identity;
}

SignaturePublicKey
BasicCredential::public_key() const
{
  return _public_key;
}

void
BasicCredential::read(tls::istream& in)
{
  SignatureScheme scheme;
  in >> _identity >> scheme;

  _public_key = SignaturePublicKey(scheme);
  in >> _public_key;
}

void
BasicCredential::write(tls::ostream& out) const
{
  out << _identity << _public_key.signature_scheme() << _public_key;
}

bool
BasicCredential::equal(const AbstractCredential* other) const
{
  auto basic_other = dynamic_cast<const BasicCredential*>(other);
  return (_identity == basic_other->_identity) &&
         (_public_key == basic_other->_public_key);
}

///
/// Credential
///

bytes
Credential::identity() const
{
  return _cred->identity();
}

SignaturePublicKey
Credential::public_key() const
{
  return _cred->public_key();
}

bool
Credential::valid_for(const SignaturePrivateKey& priv) const
{
  return priv.public_key() == public_key();
}

AbstractCredential*
Credential::create(CredentialType type)
{
  switch (type) {
    case CredentialType::basic:
      return new BasicCredential();

    case CredentialType::x509:
      throw NotImplementedError();

    default:
      throw InvalidParameterError("Unknown credential type");
  }
}

bool
operator==(const Credential& lhs, const Credential& rhs)
{
  return (lhs._type == rhs._type) && lhs._cred->equal(rhs._cred.get());
}

bool
operator!=(const Credential& lhs, const Credential& rhs)
{
  return !(lhs == rhs);
}

tls::ostream&
operator<<(tls::ostream& out, const Credential& obj)
{
  out << obj._type;
  obj._cred->write(out);
  return out;
}

tls::istream&
operator>>(tls::istream& in, Credential& obj)
{
  in >> obj._type;
  obj._cred.reset(Credential::create(obj._type));
  obj._cred->read(in);
  return in;
}

///
/// Roster
///

void
Roster::add(const Credential& cred)
{
  _credentials.push_back(cred);
}

void
Roster::remove(uint32_t index)
{
  if (index > _credentials.size()) {
    throw InvalidParameterError("Unknown credential index");
  }

  _credentials[index] = nullopt;
}

Credential
Roster::get(uint32_t index) const
{
  if (!_credentials[index]) {
    throw InvalidParameterError("No credential available");
  }

  return *_credentials[index];
}

size_t
Roster::size() const
{
  return _credentials.size();
}

bool
operator==(const Roster& lhs, const Roster& rhs)
{
  return lhs._credentials == rhs._credentials;
}

tls::ostream&
operator<<(tls::ostream& out, const Roster& obj)
{
  return out << obj._credentials;
}

tls::istream&
operator>>(tls::istream& in, Roster& obj)
{
  return in >> obj._credentials;
}

} // namespace mls
