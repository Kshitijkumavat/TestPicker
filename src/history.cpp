#include "history.hpp"
#include <fstream>
#include <sstream>
#include <stdexcept>

void HistoryStore::update(int testId, bool failed, double actualTime) {
    History* h = table_.find(testId);
    if (h) {
        h->runs++;
        if (failed) h->failures++;
        h->avgTime = (h->avgTime * (h->runs - 1) + actualTime) / static_cast<double>(h->runs);
    } else {
        History newH;
        newH.runs = 1;
        newH.failures = failed ? 1 : 0;
        newH.avgTime = actualTime;
        table_.insert(testId, newH);
    }
}

History HistoryStore::get(int testId) const {
    const History* h = table_.find(testId);
    return h ? *h : History{};
}

double HistoryStore::failureProbability(int testId) const {
    History h = get(testId);
    return static_cast<double>(h.failures + 1) / static_cast<double>(h.runs + 2);
}

void HistoryStore::loadFromCSV(const std::string& filename) {
    std::ifstream in(filename);
    if (!in.is_open()) {
        throw std::runtime_error("Could not open history file: " + filename);
    }

    table_.clear();
    std::string line;
    bool isHeader = true;

    while (std::getline(in, line)) {
        if (line.empty()) continue;
        if (isHeader) {
            isHeader = false;
            continue;
        }

        std::stringstream ss(line);
        std::string token;
        int testId = 0, runs = 0, failures = 0;
        double avgTime = 0.0;

        if (std::getline(ss, token, ',')) testId = std::stoi(token);
        if (std::getline(ss, token, ',')) runs = std::stoi(token);
        if (std::getline(ss, token, ',')) failures = std::stoi(token);
        if (std::getline(ss, token, ',')) avgTime = std::stod(token);

        History h;
        h.runs = runs;
        h.failures = failures;
        h.avgTime = avgTime;
        table_.insert(testId, h);
    }
}

void HistoryStore::saveToCSV(const std::string& filename) const {
    std::ofstream out(filename);
    if (!out.is_open()) {
        throw std::runtime_error("Could not open history file for writing: " + filename);
    }

    out << "test_id,runs,failures,avg_time_sec\n";
    table_.forEach([&out](int key, const History& val) {
        out << key << "," << val.runs << "," << val.failures << "," << val.avgTime << "\n";
    });
}
