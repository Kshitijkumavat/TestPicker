#include "io.hpp"
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <unordered_set>

static std::vector<int> parseSemicolonIntList(const std::string& s) {
    std::vector<int> res;
    if (s.empty()) return res;
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, ';')) {
        if (!item.empty()) {
            res.push_back(std::stoi(item));
        }
    }
    return res;
}

Project loadProject(const std::string& filesCSV, const std::string& testsCSV) {
    Project proj;

    // 1. Load files.csv: file_id,name,dep_ids
    {
        std::ifstream in(filesCSV);
        if (!in.is_open()) {
            throw std::runtime_error("Could not open files CSV: " + filesCSV);
        }
        std::string line;
        bool isHeader = true;
        std::unordered_set<int> seenIds;
        std::unordered_set<std::string> seenNames;
        int lineNum = 0;

        while (std::getline(in, line)) {
            lineNum++;
            if (line.empty()) continue;
            if (isHeader) {
                isHeader = false;
                continue;
            }

            std::stringstream ss(line);
            std::string idStr, name, depsStr;
            if (!std::getline(ss, idStr, ',') || !std::getline(ss, name, ',')) {
                throw std::runtime_error("Malformed line " + std::to_string(lineNum) + " in " + filesCSV);
            }
            std::getline(ss, depsStr, ',');

            int fid = std::stoi(idStr);
            if (seenIds.count(fid)) {
                throw std::runtime_error("Duplicate file ID " + std::to_string(fid) + " in " + filesCSV);
            }
            if (seenNames.count(name)) {
                throw std::runtime_error("Duplicate file name " + name + " in " + filesCSV);
            }
            seenIds.insert(fid);
            seenNames.insert(name);

            SourceFile sf;
            sf.id = fid;
            sf.name = name;
            sf.deps = parseSemicolonIntList(depsStr);
            proj.files.push_back(std::move(sf));
        }
    }

    // 2. Load tests.csv: test_id,name,time_sec,file_ids,prereq_ids
    {
        std::ifstream in(testsCSV);
        if (!in.is_open()) {
            throw std::runtime_error("Could not open tests CSV: " + testsCSV);
        }
        std::string line;
        bool isHeader = true;
        std::unordered_set<int> seenIds;
        std::unordered_set<std::string> seenNames;
        int lineNum = 0;

        while (std::getline(in, line)) {
            lineNum++;
            if (line.empty()) continue;
            if (isHeader) {
                isHeader = false;
                continue;
            }

            std::stringstream ss(line);
            std::string idStr, name, timeStr, filesStr, prereqsStr;
            if (!std::getline(ss, idStr, ',') ||
                !std::getline(ss, name, ',') ||
                !std::getline(ss, timeStr, ',') ||
                !std::getline(ss, filesStr, ',')) {
                throw std::runtime_error("Malformed line " + std::to_string(lineNum) + " in " + testsCSV);
            }
            std::getline(ss, prereqsStr, ',');

            int tid = std::stoi(idStr);
            if (seenIds.count(tid)) {
                throw std::runtime_error("Duplicate test ID " + std::to_string(tid) + " in " + testsCSV);
            }
            if (seenNames.count(name)) {
                throw std::runtime_error("Duplicate test name " + name + " in " + testsCSV);
            }
            seenIds.insert(tid);
            seenNames.insert(name);

            Test t;
            t.id = tid;
            t.name = name;
            t.timeSec = std::stod(timeStr);
            t.files = parseSemicolonIntList(filesStr);
            t.prereqs = parseSemicolonIntList(prereqsStr);
            proj.tests.push_back(std::move(t));
        }
    }

    return proj;
}

Commit loadCommit(const std::string& filename) {
    std::ifstream in(filename);
    if (!in.is_open()) {
        throw std::runtime_error("Could not open commit file: " + filename);
    }
    std::string line;
    Commit c;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        if (line.find("changed_file_ids") != std::string::npos) continue;
        c.changedFiles = parseSemicolonIntList(line);
        break;
    }
    return c;
}

void saveProject(const Project& proj,
                 const std::string& filesCSV, const std::string& testsCSV)
{
    {
        std::ofstream out(filesCSV);
        if (!out.is_open()) {
            throw std::runtime_error("Could not write to: " + filesCSV);
        }
        out << "file_id,name,dep_ids\n";
        for (const auto& f : proj.files) {
            out << f.id << "," << f.name << ",";
            for (size_t i = 0; i < f.deps.size(); ++i) {
                if (i > 0) out << ";";
                out << f.deps[i];
            }
            out << "\n";
        }
    }

    {
        std::ofstream out(testsCSV);
        if (!out.is_open()) {
            throw std::runtime_error("Could not write to: " + testsCSV);
        }
        out << "test_id,name,time_sec,file_ids,prereq_ids\n";
        for (const auto& t : proj.tests) {
            out << t.id << "," << t.name << "," << t.timeSec << ",";
            for (size_t i = 0; i < t.files.size(); ++i) {
                if (i > 0) out << ";";
                out << t.files[i];
            }
            out << ",";
            for (size_t i = 0; i < t.prereqs.size(); ++i) {
                if (i > 0) out << ";";
                out << t.prereqs[i];
            }
            out << "\n";
        }
    }
}
