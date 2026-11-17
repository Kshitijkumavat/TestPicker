#pragma once
// ─────────────────────────────────────────────────────────────────
// hashtable.hpp – Custom separate-chaining hash table (template)
//
// Requirements (PRD §10):
//   • Template HashTable<K,V> supporting string and int keys
//   • Separate chaining, dynamic resize at load factor 0.75
//   • insert, find, erase, size, loadFactor, iteration, probe counter
// ─────────────────────────────────────────────────────────────────
#include <vector>
#include <string>
#include <utility>
#include <cstddef>
#include <algorithm>

// ── Custom hash functors ────────────────────────────────────────
template<typename K> struct CustomHash;

template<>
struct CustomHash<int> {
    std::size_t operator()(int key) const noexcept {
        // Knuth multiplicative hash
        return static_cast<std::size_t>(key) * 2654435761u;
    }
};

template<>
struct CustomHash<std::string> {
    std::size_t operator()(const std::string& key) const noexcept {
#if defined(__SIZEOF_SIZE_T__) && __SIZEOF_SIZE_T__ >= 8
        uint64_t h = 14695981039346656037ull;
        for (unsigned char c : key) {
            h ^= c;
            h *= 1099511628211ull;
        }
        return static_cast<std::size_t>(h);
#else
        uint32_t h = 2166136261u;
        for (unsigned char c : key) {
            h ^= c;
            h *= 16777619u;
        }
        return static_cast<std::size_t>(h);
#endif
    }
};

// ── HashTable ───────────────────────────────────────────────────
template<typename K, typename V, typename Hash = CustomHash<K>>
class HashTable {
    // ── Node (separate chaining) ────────────────────────────────
    struct Node {
        K      key;
        V      value;
        Node*  next;
        Node(const K& k, const V& v, Node* n = nullptr)
            : key(k), value(v), next(n) {}
    };

    std::vector<Node*> buckets_;
    std::size_t        size_;
    mutable std::size_t probeCount_;  // comparisons/probes for perf reporting
    Hash               hasher_;

    static constexpr double MAX_LOAD = 0.75;

    // Bucket index for a key.
    std::size_t idx(const K& key) const {
        return hasher_(key) % buckets_.size();
    }

    // Double the bucket count and re-insert every entry.
    void rehash() {
        std::size_t newCap = buckets_.size() * 2;
        std::vector<Node*> nb(newCap, nullptr);
        for (auto& b : buckets_) {
            Node* cur = b;
            while (cur) {
                Node* nxt  = cur->next;
                std::size_t i = hasher_(cur->key) % newCap;
                cur->next  = nb[i];
                nb[i]      = cur;
                cur        = nxt;
            }
            b = nullptr;
        }
        buckets_ = std::move(nb);
    }

public:
    // Delete every node.
    void clear() {
        for (auto& b : buckets_) {
            Node* cur = b;
            while (cur) { Node* n = cur->next; delete cur; cur = n; }
            b = nullptr;
        }
        size_ = 0;
    }
    // ── Construction / destruction ──────────────────────────────
    explicit HashTable(std::size_t initialBuckets = 16)
        : buckets_(std::max<std::size_t>(initialBuckets, 1), nullptr),
          size_(0), probeCount_(0) {}

    ~HashTable() { clear(); }

    // Copy
    HashTable(const HashTable& o)
        : buckets_(o.buckets_.size(), nullptr), size_(0), probeCount_(0) {
        for (std::size_t i = 0; i < o.buckets_.size(); ++i) {
            for (Node* c = o.buckets_[i]; c; c = c->next) {
                std::size_t bi = hasher_(c->key) % buckets_.size();
                buckets_[bi] = new Node(c->key, c->value, buckets_[bi]);
                ++size_;
            }
        }
    }
    HashTable& operator=(const HashTable& o) {
        if (this != &o) { HashTable tmp(o); swap(tmp); }
        return *this;
    }

    // Move
    HashTable(HashTable&& o) noexcept
        : buckets_(std::move(o.buckets_)), size_(o.size_), probeCount_(o.probeCount_) {
        o.size_ = 0; o.probeCount_ = 0;
    }
    HashTable& operator=(HashTable&& o) noexcept {
        if (this != &o) {
            clear();
            buckets_     = std::move(o.buckets_);
            size_        = o.size_;
            probeCount_  = o.probeCount_;
            o.size_      = 0;
            o.probeCount_= 0;
        }
        return *this;
    }

    void swap(HashTable& o) noexcept {
        buckets_.swap(o.buckets_);
        std::swap(size_, o.size_);
        std::swap(probeCount_, o.probeCount_);
    }

    // ── Core operations ─────────────────────────────────────────

    /// Insert or update an entry.
    void insert(const K& key, const V& value) {
        if (loadFactor() >= MAX_LOAD) rehash();
        std::size_t i = idx(key);
        ++probeCount_;
        for (Node* c = buckets_[i]; c; c = c->next) {
            ++probeCount_;
            if (c->key == key) { c->value = value; return; }
        }
        buckets_[i] = new Node(key, value, buckets_[i]);
        ++size_;
    }

    /// Find a key – returns pointer to value or nullptr.
    V* find(const K& key) {
        std::size_t i = idx(key);
        ++probeCount_;
        for (Node* c = buckets_[i]; c; c = c->next) {
            ++probeCount_;
            if (c->key == key) return &c->value;
        }
        return nullptr;
    }
    const V* find(const K& key) const {
        std::size_t i = idx(key);
        ++probeCount_;
        for (Node* c = buckets_[i]; c; c = c->next) {
            ++probeCount_;
            if (c->key == key) return &c->value;
        }
        return nullptr;
    }

    /// Erase a key – returns true if the key existed.
    bool erase(const K& key) {
        std::size_t i = idx(key);
        Node* prev = nullptr;
        for (Node* c = buckets_[i]; c; c = c->next) {
            ++probeCount_;
            if (c->key == key) {
                if (prev) prev->next = c->next;
                else      buckets_[i] = c->next;
                delete c;
                --size_;
                return true;
            }
            prev = c;
        }
        return false;
    }

    // ── Accessors ───────────────────────────────────────────────

    V& operator[](const K& key) {
        V* p = find(key);
        if (p) return *p;
        insert(key, V{});
        return *find(key);
    }

    bool        contains(const K& key) const { return find(key) != nullptr; }
    std::size_t size()        const { return size_; }
    bool        empty()       const { return size_ == 0; }
    double      loadFactor()  const {
        return buckets_.empty() ? 0.0 : static_cast<double>(size_) / buckets_.size();
    }
    std::size_t bucketCount() const { return buckets_.size(); }

    // ── Performance counters ────────────────────────────────────
    std::size_t getProbeCount()  const { return probeCount_; }
    void        resetProbeCount()      { probeCount_ = 0; }

    // ── Iteration ───────────────────────────────────────────────

    /// Call f(key, value) for every entry.
    template<typename Func>
    void forEach(Func&& f) const {
        for (const auto& b : buckets_)
            for (Node* c = b; c; c = c->next)
                f(c->key, c->value);
    }

    /// Collect all keys.
    std::vector<K> keys() const {
        std::vector<K> v; v.reserve(size_);
        forEach([&](const K& k, const V&){ v.push_back(k); });
        return v;
    }

    /// Collect all (key, value) pairs.
    std::vector<std::pair<K,V>> entries() const {
        std::vector<std::pair<K,V>> v; v.reserve(size_);
        forEach([&](const K& k, const V& val){ v.emplace_back(k, val); });
        return v;
    }
};
