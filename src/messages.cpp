#include "messages.h"

#define DUMMY_CIPHERSUITE CipherSuite::P256_SHA256_AES128GCM

namespace mls {

// RatchetNode

RatchetNode::RatchetNode(CipherSuite suite)
  : CipherAware(suite)
  , public_key(suite)
  , node_secrets(suite)
{}

RatchetNode::RatchetNode(DHPublicKey public_key_in,
                         const std::vector<HPKECiphertext>& node_secrets_in)
  : CipherAware(public_key_in.cipher_suite())
  , public_key(std::move(public_key_in))
  , node_secrets(node_secrets_in)
{}

// DirectPath

DirectPath::DirectPath(CipherSuite suite)
  : CipherAware(suite)
  , nodes(suite)
{}

// ClientInitKey

ClientInitKey::ClientInitKey()
  : version(ProtocolVersion::mls10)
  , cipher_suite(DUMMY_CIPHERSUITE)
  , init_key(DUMMY_CIPHERSUITE)
{}

ClientInitKey::ClientInitKey(const HPKEPrivateKey& init_key_in,
                             Credential credential_in)
  : version(ProtocolVersion::mls10)
  , cipher_suite(init_key_in.cipher_suite())
  , init_key(init_key_in.public_key())
  , credential(std::move(credential_in))
  , _private_key(init_key_in)
{
  if (!credential.private_key().has_value()) {
    throw InvalidParameterError("Credential must have a private key");
  }

  auto tbs = to_be_signed();
  signature = credential.private_key().value().sign(tbs);
}

const std::optional<HPKEPrivateKey>&
ClientInitKey::private_key() const
{
  return _private_key;
}

bytes
ClientInitKey::hash() const
{
  auto marshaled = tls::marshal(*this);
  return Digest(cipher_suite).write(marshaled).digest();
}

bool
ClientInitKey::verify() const
{
  auto tbs = to_be_signed();
  auto identity_key = credential.public_key();
  return identity_key.verify(tbs, signature);
}

bytes
ClientInitKey::to_be_signed() const
{
  tls::ostream out;
  out << version << cipher_suite << init_key << credential;
  return out.bytes();
}

bool
operator==(const ClientInitKey& lhs, const ClientInitKey& rhs)
{
  return (lhs.version == rhs.version) &&
         (lhs.cipher_suite == rhs.cipher_suite) &&
         (lhs.init_key == rhs.init_key) && (lhs.credential == rhs.credential) &&
         (lhs.signature == rhs.signature);
}

bool
operator!=(const ClientInitKey& lhs, const ClientInitKey& rhs)
{
  return !(lhs == rhs);
}

tls::ostream&
operator<<(tls::ostream& str, const ClientInitKey& obj)
{
  return str << obj.version << obj.cipher_suite << obj.init_key
             << obj.credential << obj.signature;
}

tls::istream&
operator>>(tls::istream& str, ClientInitKey& obj)
{
  str >> obj.version >> obj.cipher_suite;
  obj.init_key = HPKEPublicKey(obj.cipher_suite);
  str >> obj.init_key >> obj.credential >> obj.signature;
  return str;
}

// GroupInfo

GroupInfo::GroupInfo(CipherSuite suite)
  : epoch(0)
  , tree(suite)
  , path(suite)
{}

GroupInfo::GroupInfo(bytes group_id_in,
                     epoch_t epoch_in,
                     RatchetTree tree_in,
                     bytes confirmed_transcript_hash_in,
                     bytes interim_transcript_hash_in,
                     DirectPath path_in,
                     bytes confirmation_in)
  : group_id(std::move(group_id_in))
  , epoch(epoch_in)
  , tree(std::move(tree_in))
  , confirmed_transcript_hash(std::move(confirmed_transcript_hash_in))
  , interim_transcript_hash(std::move(interim_transcript_hash_in))
  , path(std::move(path_in))
  , confirmation(std::move(confirmation_in))
{}

bytes
GroupInfo::to_be_signed() const
{
  tls::ostream w;
  w << group_id << epoch << tree << confirmed_transcript_hash
    << interim_transcript_hash << path << confirmation << signer_index;
  return w.bytes();
}

void
GroupInfo::sign(LeafIndex index, const SignaturePrivateKey& priv)
{
  auto cred = tree.get_credential(LeafIndex{ index });
  if (cred.public_key() != priv.public_key()) {
    throw InvalidParameterError("Bad key for index");
  }

  signer_index = index;
  signature = priv.sign(to_be_signed());
}

bool
GroupInfo::verify() const
{
  auto cred = tree.get_credential(LeafIndex{ signer_index });
  return cred.public_key().verify(to_be_signed(), signature);
}

// EncryptedKeyPackage

EncryptedKeyPackage::EncryptedKeyPackage(CipherSuite suite)
  : encrypted_key_package(suite)
{}

EncryptedKeyPackage::EncryptedKeyPackage(bytes hash, HPKECiphertext package)
  : client_init_key_hash(std::move(hash))
  , encrypted_key_package(std::move(package))
{}

// Welcome

Welcome::Welcome()
  : version(ProtocolVersion::mls10)
  , cipher_suite(DUMMY_CIPHERSUITE)
  , key_packages(DUMMY_CIPHERSUITE)
{}

Welcome::Welcome(CipherSuite suite,
                 bytes init_secret,
                 const GroupInfo& group_info)
  : version(ProtocolVersion::mls10)
  , cipher_suite(suite)
  , key_packages(suite)
  , _init_secret(std::move(init_secret))
{
  auto [key, nonce] = group_info_keymat(_init_secret);
  auto group_info_data = tls::marshal(group_info);
  encrypted_group_info = AESGCM(key, nonce).encrypt(group_info_data);
}

std::tuple<bytes, bytes>
Welcome::group_info_keymat(const bytes& init_secret) const
{
  auto secret_size = Digest(cipher_suite).output_size();
  auto key_size = AESGCM::key_size(cipher_suite);
  auto nonce_size = AESGCM::nonce_size;

  auto secret =
    hkdf_expand_label(cipher_suite, init_secret, "group info", {}, secret_size);
  auto key = hkdf_expand_label(cipher_suite, secret, "key", {}, key_size);
  auto nonce = hkdf_expand_label(cipher_suite, secret, "nonce", {}, nonce_size);
  return std::make_tuple(key, nonce);
}

void
Welcome::encrypt(const ClientInitKey& cik)
{
  auto key_pkg = KeyPackage{ _init_secret };
  auto key_pkg_data = tls::marshal(key_pkg);
  auto enc_pkg = cik.init_key.encrypt(key_pkg_data);
  key_packages.emplace_back(cik.hash(), enc_pkg);
}

bool
operator==(const Welcome& lhs, const Welcome& rhs)
{
  return (lhs.version == rhs.version) &&
         (lhs.cipher_suite == rhs.cipher_suite) &&
         (lhs.key_packages == rhs.key_packages) &&
         (lhs.encrypted_group_info == rhs.encrypted_group_info);
}

tls::ostream&
operator<<(tls::ostream& str, const Welcome& obj)
{
  return str << obj.version << obj.cipher_suite << obj.key_packages
             << obj.encrypted_group_info;
}

tls::istream&
operator>>(tls::istream& str, Welcome& obj)
{
  str >> obj.version >> obj.cipher_suite;
  obj.key_packages.set_arg(obj.cipher_suite);
  str >> obj.key_packages >> obj.encrypted_group_info;
  return str;
}

// Proposals

// NOLINTNEXTLINE(misc-unused-parameters)
Add::Add(CipherSuite suite) {}

Add::Add(ClientInitKey client_init_key_in)
  : client_init_key(std::move(client_init_key_in))
{}

const ProposalType Add::type = ProposalType::add;

Update::Update(CipherSuite suite)
  : leaf_key(suite)
{}

Update::Update(HPKEPublicKey leaf_key_in)
  : leaf_key(std::move(leaf_key_in))
{}

const ProposalType Update::type = ProposalType::update;

// NOLINTNEXTLINE(misc-unused-parameters)
Remove::Remove(CipherSuite suite) {}

Remove::Remove(LeafIndex removed_in)
  : removed(removed_in)
{}

const ProposalType Remove::type = ProposalType::remove;

Proposal::Proposal(CipherSuite suite)
  : parent(suite, Remove{ LeafIndex{ 0 } })
{}

// Commit

Commit::Commit(CipherSuite suite)
  : path(suite)
{}

Commit::Commit(tls::vector<ProposalID, 2> updates_in,
               tls::vector<ProposalID, 2> removes_in,
               tls::vector<ProposalID, 2> adds_in,
               tls::vector<ProposalID, 2> ignored_in,
               DirectPath path_in)
  : updates(std::move(updates_in))
  , removes(std::move(removes_in))
  , adds(std::move(adds_in))
  , ignored(std::move(ignored_in))
  , path(std::move(path_in))
{}

// MLSPlaintext

const ContentType ApplicationData::type = ContentType::application;
const ContentType Proposal::type = ContentType::proposal;
const ContentType CommitData::type = ContentType::commit;

MLSPlaintext::MLSPlaintext(CipherSuite suite)
  : CipherAware(suite)
  , epoch(0)
  , sender(0)
  , content(suite, ApplicationData{ suite })
{}

MLSPlaintext::MLSPlaintext(CipherSuite suite,
                           const bytes& group_id_in,
                           epoch_t epoch_in,
                           LeafIndex sender_in,
                           ContentType content_type_in,
                           bytes content_in)
  : CipherAware(suite)
  , group_id(group_id_in)
  , epoch(epoch_in)
  , sender(sender_in)
  , content(suite, ApplicationData{ suite })
{
  int cut = content_in.size() - 1;
  for (; content_in[cut] == 0 && cut >= 0; cut -= 1) {
  }
  if (content_in[cut] != 0x01) {
    throw ProtocolError("Invalid marker byte");
  }

  auto start = content_in.begin();
  auto sig_len_bytes = bytes(start + cut - 2, start + cut);
  auto sig_len = tls::get<uint16_t>(sig_len_bytes);

  cut -= 2;
  if (sig_len > cut) {
    throw ProtocolError("Invalid signature size");
  }

  signature = bytes(start + cut - sig_len, start + cut);
  auto content_data = bytes(start, start + cut - sig_len);

  switch (content_type_in) {
    case ContentType::application: {
      auto& application_data = content.emplace<ApplicationData>();
      tls::unmarshal(content_data, application_data);
      break;
    }

      // TODO(rlb) decode content for Proposal and Commit

    default:
      throw InvalidParameterError("Unknown content type");
  }
}

MLSPlaintext::MLSPlaintext(bytes group_id_in,
                           epoch_t epoch_in,
                           LeafIndex sender_in,
                           const ApplicationData& application_data_in)
  : CipherAware(DUMMY_CIPHERSUITE)
  , group_id(std::move(group_id_in))
  , epoch(epoch_in)
  , sender(sender_in)
  , content(DUMMY_CIPHERSUITE, application_data_in)
{}

MLSPlaintext::MLSPlaintext(bytes group_id_in,
                           epoch_t epoch_in,
                           LeafIndex sender_in,
                           const Proposal& proposal)
  : CipherAware(DUMMY_CIPHERSUITE)
  , group_id(std::move(group_id_in))
  , epoch(epoch_in)
  , sender(sender_in)
  , content(DUMMY_CIPHERSUITE, proposal)
{}

MLSPlaintext::MLSPlaintext(bytes group_id_in,
                           epoch_t epoch_in,
                           LeafIndex sender_in,
                           const Commit& commit)
  : CipherAware(DUMMY_CIPHERSUITE)
  , group_id(std::move(group_id_in))
  , epoch(epoch_in)
  , sender(sender_in)
  , content(DUMMY_CIPHERSUITE, CommitData{ commit, {} })
{}

// struct {
//     opaque content[MLSPlaintext.length];
//     uint8 signature[MLSInnerPlaintext.sig_len];
//     uint16 sig_len;
//     uint8  marker = 1;
//     uint8  zero\_padding[length\_of\_padding];
// } MLSContentPlaintext;
bytes
MLSPlaintext::marshal_content(size_t padding_size) const
{
  bytes marshaled;
  if (content.inner_type() == ContentType::application) {
    marshaled = tls::marshal(std::get<ApplicationData>(content));
  } else {
    // TODO(398) Marshal content for proposal / commit
    throw InvalidParameterError("Unknown content type");
  }

  uint16_t sig_len = signature.size();
  auto marker = bytes{ 0x01 };
  auto pad = zero_bytes(padding_size);
  marshaled = marshaled + signature + tls::marshal(sig_len) + marker + pad;
  return marshaled;
}

bytes
MLSPlaintext::commit_content() const
{
  auto& commit_data = std::get<CommitData>(content);
  tls::ostream w;
  w << group_id << epoch << sender << commit_data.commit;
  return w.bytes();
}

// struct {
//   opaque confirmation<0..255>;
//   opaque signature<0..2^16-1>;
// } MLSPlaintextOpAuthData;
bytes
MLSPlaintext::commit_auth_data() const
{
  auto& commit_data = std::get<CommitData>(content);
  tls::ostream w;
  w << commit_data.confirmation << signature;
  return w.bytes();
}

bytes
MLSPlaintext::to_be_signed() const
{
  tls::ostream w;
  w << group_id << epoch << sender << content;
  return w.bytes();
}

void
MLSPlaintext::sign(const SignaturePrivateKey& priv)
{
  auto tbs = to_be_signed();
  signature = priv.sign(tbs);
}

bool
MLSPlaintext::verify(const SignaturePublicKey& pub) const
{
  auto tbs = to_be_signed();
  return pub.verify(tbs, signature);
}

} // namespace mls
