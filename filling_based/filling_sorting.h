#ifndef FILLING_SORTING_H
#define FILLING_SORTING_H

#include <algorithm>
#include <fstream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

inline void normalize_clause(std::vector<int>& clause) {
    std::sort(clause.begin(), clause.end());
    clause.erase(std::unique(clause.begin(), clause.end()), clause.end());
}

inline void sort_clauses(std::vector<std::vector<int>>& clauses, const std::string& criterion) {
    for (auto& clause : clauses) {
        normalize_clause(clause);
    }

    std::string key = criterion;
    std::transform(key.begin(), key.end(), key.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    if (key == "max") {
        std::stable_sort(clauses.begin(), clauses.end(), [](auto const& a, auto const& b) {
            int maxA = a.empty() ? std::numeric_limits<int>::min() : a.back();
            int maxB = b.empty() ? std::numeric_limits<int>::min() : b.back();
            if (maxA != maxB) {
                return maxA > maxB;
            }
            if (a.size() != b.size()) {
                return a.size() > b.size();
            }
            return a > b;
        });
    } else {
        std::stable_sort(clauses.begin(), clauses.end(), [](auto const& a, auto const& b) {
            int minA = a.empty() ? std::numeric_limits<int>::max() : a.front();
            int minB = b.empty() ? std::numeric_limits<int>::max() : b.front();
            if (minA != minB) {
                return minA < minB;
            }
            if (a.size() != b.size()) {
                return a.size() > b.size();
            }
            return a < b;
        });
    }
}

inline bool sortFileClauses(const std::string& folder_name, const std::string& filename, const std::string& criterion) {
    std::string inputPath = folder_name + filename;
    std::string outputPath = folder_name + "sorted_" + filename;

    std::ifstream fin(inputPath);
    if (!fin) {
        std::cerr << "Cannot open input file for sorting: " << inputPath << "\n";
        return false;
    }

    std::vector<std::string> header;
    std::vector<std::vector<int>> clauses;
    std::string line;
    while (std::getline(fin, line)) {
        if (line.empty())
            continue;
        if (line[0] == 'c' || line[0] == 'p') {
            header.push_back(line);
            continue;
        }
        std::istringstream ss(line);
        std::vector<int> clause;
        int x;
        while (ss >> x) {
            if (x == 0)
                break;
            clause.push_back(x);
        }
        if (!clause.empty())
            clauses.push_back(clause);
    }
    fin.close();

    sort_clauses(clauses, criterion);

    std::ofstream fout(outputPath);
    if (!fout) {
        std::cerr << "Cannot open sorted output file: " << outputPath << "\n";
        return false;
    }
    for (auto& h : header)
        fout << h << "\n";
    for (auto& cl : clauses) {
        for (int x : cl)
            fout << x << " ";
        fout << "0\n";
    }
    fout.close();
    return true;
}

#endif // FILLING_SORTING_H
