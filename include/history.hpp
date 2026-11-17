#pragma once
// ─────────────────────────────────────────────────────────────────
// history.hpp – History store backed by the custom hash table
// ─────────────────────────────────────────────────────────────────
#include "types.hpp"
#include "hashtable.hpp"
#include <string>

class HistoryStore {
public:
    /// Record a test outcome (pass/fail) and its actual runtime.
    void update(int testId, bool failed, double actualTime);

    /// Retrieve history for a test (returns default if unseen).
    History get(int testId) const;

    /// Laplace-smoothed failure probability: (failures+1)/(runs+2).
    double failureProbability(int testId) const;

    void loadFromCSV(const std::string& filename);
    void saveToCSV(const std::string& filename) const;

    HashTable<int, History>&       table()       { return table_; }
    const HashTable<int, History>& table() const { return table_; }

private:
    HashTable<int, History> table_;
};
