#pragma once

#include "common.h"
#include "credential.h"
#include "crypto.h"
#include "tree_math.h"
#include "tls_syntax.h"
#include <optional>
#include <list>
#include <iostream>

namespace mls {

struct KeyPackage;

class RatchetTreeNode
{
public:
  RatchetTreeNode() = default;
  RatchetTreeNode(CipherSuite suite, const bytes& secret);
  RatchetTreeNode(const HPKEPrivateKey& priv);
  RatchetTreeNode(const HPKEPublicKey& pub);

  bool public_equal(const RatchetTreeNode& other) const;
  const std::optional<bytes>& secret() const;
  const std::optional<HPKEPrivateKey>& private_key() const;
  const HPKEPublicKey& public_key() const;
  const std::vector<LeafIndex>& unmerged_leaves() const;
  const std::optional<Credential>& credential() const;

  void merge(const RatchetTreeNode& other);
  void set_credential(const Credential& cred);
  void set_cipher_suite();
  void add_unmerged(LeafIndex index);

  TLS_SERIALIZABLE(_pub, _unmerged_leaves, _cred)
  TLS_TRAITS(tls::pass, tls::vector<4>, tls::pass)

private:
  std::optional<bytes> _secret;
  std::optional<HPKEPrivateKey> _priv;
  HPKEPublicKey _pub;

  // Unmerged leaves to be included in resolution
  std::vector<LeafIndex> _unmerged_leaves;

  // A credential is populated iff this is a leaf node
  std::optional<Credential> _cred;
};

// XXX(rlb@ipv.sx): We have to subclass optional<T> in order to
// ensure that nodes are populated with blank values on unmarshal.
// Otherwise, `*opt` will access uninitialized memory.
struct OptionalRatchetTreeNode
  : public std::optional<RatchetTreeNode>
{
  using parent = std::optional<RatchetTreeNode>;
  using parent::parent;

  bool has_private() const;
  const bytes& hash() const;

  void merge(const RatchetTreeNode& other);
  void set_leaf_hash(CipherSuite suite);
  void set_hash(CipherSuite suite,
                const OptionalRatchetTreeNode& left,
                const OptionalRatchetTreeNode& right);

private:
  bytes _hash;
};

struct RatchetTreeNodeVector
  : public std::vector<OptionalRatchetTreeNode>
{
  using parent = std::vector<OptionalRatchetTreeNode>;
  using parent::parent;
  using parent::operator[];

  OptionalRatchetTreeNode& operator[](const NodeIndex index);
  const OptionalRatchetTreeNode& operator[](const NodeIndex index) const;
};

struct RatchetNode;
struct DirectPath;

class RatchetTree
{
public:
  RatchetTree(CipherSuite suite);
  RatchetTree(CipherSuite suite, const HPKEPrivateKey& priv, const Credential& cred);

  // KEM encap/decap, including updates to the tree
  std::tuple<DirectPath, bytes> encap(LeafIndex from, const bytes& context, const bytes& leaf);
  bytes decap(LeafIndex from, const bytes& context, const DirectPath& path);

  // Blank a path through the tree
  void blank_path(LeafIndex index, bool include_leaf);

  // Add a leaf node to the tree (without affecting its parents)
  void add_leaf(LeafIndex index, const HPKEPublicKey& leaf_key, const Credential& credential);

  // Merge a new key into a leaf (without affecting its credential)
  void merge(LeafIndex index, const HPKEPublicKey& leaf_key);
  void merge(LeafIndex index, const HPKEPrivateKey& leaf_priv);
  void merge(LeafIndex index, const bytes& leaf_secret);

  bool occupied(LeafIndex index) const;
  LeafIndex leftmost_free() const;
  std::optional<LeafIndex> find(const KeyPackage& kp) const;
  const Credential& get_credential(LeafIndex index) const;

  std::optional<bytes> ancestor_secret(LeafIndex from, LeafIndex to) const;
  bytes implant(NodeIndex index, const bytes& path_secret);

  LeafCount leaf_span() const;
  void truncate(LeafCount leaves);

  uint32_t size() const;
  bytes root_hash() const;

protected:
  RatchetTreeNodeVector _nodes;
  CipherSuite _suite;
  size_t _secret_size;

  NodeIndex root_index() const;
  NodeCount node_size() const;
  RatchetTreeNode new_node(const bytes& path_secret) const;
  bytes path_step(const bytes& path_secret) const;
  std::list<NodeIndex> resolve(NodeIndex index);

  void set_hash(NodeIndex index);
  void set_hash_path(LeafIndex index);
  void set_hash_all(NodeIndex index);

  // XXX(rlb): These are still necessary because operator>> triggers the
  // computation of the tree hash
  friend bool operator==(const RatchetTree& lhs, const RatchetTree& rhs);
  friend std::ostream& operator<<(std::ostream& out, const RatchetTree& obj);
  friend tls::ostream& operator<<(tls::ostream& out, const RatchetTree& obj);
  friend tls::istream& operator>>(tls::istream& in, RatchetTree& obj);
};

} // namespace mls
