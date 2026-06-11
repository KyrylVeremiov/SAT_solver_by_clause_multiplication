#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <map>
#include <algorithm>
#include <filesystem>
#include "../sorting_clauses_min_index.h"
#include "filling_sorting.h"

using namespace std;
namespace fs = std::filesystem;

struct Literal {
    int var;
    bool val;
};

struct Node {
    int var;
    map<int, Node*> childTrue;
    map<int, Node*> childFalse;
};

using Clause = vector<Literal>;

Node* anyNode = nullptr;
Node* root    = nullptr;

bool hasEmptyPattern = true;

map<int,bool> toMap(const Clause& c) {
    map<int,bool> m;
    for (auto& lit : c)
        m[lit.var] = lit.val;
    return m;
}

bool containsVar(const Clause& pat, int var) {
    for (auto& lit : pat)
        if (lit.var == var)
            return true;
    return false;
}

Clause addLiteralSorted(const Clause& pat, Literal lit) {
    Clause res = pat;
    res.push_back(lit);
    sort(res.begin(), res.end(), [](const Literal& a, const Literal& b) {
        return a.var < b.var;
    });
    return res;
}

void insertPattern(Node* root, const Clause& pat) {
    if (pat.empty()) {
        hasEmptyPattern = true;
        return;
    }

    Node* cur = root;
    for (auto& lit : pat) {
        auto& mp = lit.val ? cur->childTrue : cur->childFalse;
        auto it = mp.find(lit.var);
        if (it == mp.end()) {
            Node* nxt = new Node{lit.var};
            mp[lit.var] = nxt;
            cur = nxt;
        } else {
            cur = it->second;
        }
    }

    auto& mpLeaf = pat.back().val ? cur->childTrue : cur->childFalse;
    if (!mpLeaf.count(-1))
        mpLeaf[-1] = anyNode;
}

void removePattern(Node* root, const Clause& pat) {
    if (pat.empty()) {
        hasEmptyPattern = false;
        return;
    }

    Node* cur = root;
    for (auto& lit : pat) {
        auto& mp = lit.val ? cur->childTrue : cur->childFalse;
        auto it = mp.find(lit.var);
        if (it == mp.end())
            return;
        cur = it->second;
    }

    auto& mpLeaf = pat.back().val ? cur->childTrue : cur->childFalse;
    auto itLeaf = mpLeaf.find(-1);
    if (itLeaf != mpLeaf.end())
        mpLeaf.erase(itLeaf);
}

void collectPatterns(Node* node, vector<Literal>& cur, vector<Clause>& out) {
    for (auto& [var, child] : node->childTrue) {
        if (var == -1 && child == anyNode) {
            out.push_back(cur);
        } else {
            cur.push_back({var, true});
            collectPatterns(child, cur, out);
            cur.pop_back();
        }
    }

    for (auto& [var, child] : node->childFalse) {
        if (var == -1 && child == anyNode) {
            out.push_back(cur);
        } else {
            cur.push_back({var, false});
            collectPatterns(child, cur, out);
            cur.pop_back();
        }
    }
}

vector<Clause> getAllPatterns() {
    vector<Clause> patterns;
    vector<Literal> cur;
    if (hasEmptyPattern)
        patterns.push_back(Clause{});
    collectPatterns(root, cur, patterns);
    return patterns;
}

bool containsSameLiteral(const Clause& T, const Clause& C) {
    auto mT = toMap(T);
    for (auto& lit : C) {
        auto it = mT.find(lit.var);
        if (it != mT.end() && it->second == lit.val)
            return true;
    }
    return false;
}

bool containsOppositeLiteral(const Clause& T, int var, bool val) {
    for (auto& lit : T) {
        if (lit.var == var && lit.val != val)
            return true;
    }
    return false;
}

void updateTree(Node* root, const Clause& clause) {
    vector<Clause> patterns = getAllPatterns();

    for (auto& T : patterns) {
        if (T.empty()) {
            removePattern(root, T);
            for (auto& lit : clause)
                insertPattern(root, Clause{lit});
            continue;
        }

        bool satisfied = containsSameLiteral(T, clause);
        if (satisfied)
            continue;

        bool inserted = false;
        for (auto& lit : clause) {
            if (containsVar(T, lit.var))
                continue;
            if (containsOppositeLiteral(T, lit.var, lit.val))
                continue;

            Clause newPat = addLiteralSorted(T, lit);
            insertPattern(root, newPat);
            inserted = true;
        }

        removePattern(root, T);
    }
}

Clause readClause(istringstream& iss) {
    Clause c;
    int x;
    while (iss >> x) {
        if (x == 0)
            break;
        if (x > 0)
            c.push_back({x, true});
        else
            c.push_back({-x, false});
    }
    return c;
}

void freeTree(Node* node) {
    if (!node || node == anyNode)
        return;
    for (auto& [_, child] : node->childTrue)
        freeTree(child);
    for (auto& [_, child] : node->childFalse)
        freeTree(child);
    delete node;
}

fs::path resolveInputPath(int argc, char* argv[]) {
    vector<fs::path> candidates;
    if (argc > 1)
        candidates.emplace_back(argv[1]);

    // candidates.emplace_back("../test_cases/test_test.cnf");
    candidates.emplace_back("../test_cases/uf20-01.cnf");
    // candidates.emplace_back("../test_cases/uf20-05.cnf");
    // candidates.emplace_back("../test_cases/uuf50-01.cnf");
    // candidates.emplace_back("../test_cases/uuf50-01.cnf");
    // candidates.emplace_back("../test_cases/uuf50-01.cnf");
    

    // candidates.emplace_back("../../NewFolder/test_cases/test_test.cnf");
    // candidates.emplace_back("../NewFolder/test_cases/test_test.cnf");

    for (auto const& candidate : candidates) {
        if (fs::exists(candidate))
            return candidate;
    }
    return fs::path();
}

int main(int argc, char* argv[]) {
    fs::path inputPath = resolveInputPath(argc, argv);
    if (inputPath.empty()) {
        cerr << "Cannot locate input CNF file\n";
        return 1;
    }

    fs::path folderPath = inputPath.parent_path();
    string folderName = folderPath.string();
    if (folderName.empty())
        folderName = "./";
    else if (folderName.back() != '/' && folderName.back() != '\\')
        folderName += fs::path::preferred_separator;

    string filename = inputPath.filename().string();

    // Alternative sorting method using filling_sorting.h
    
    // int sortResult = sortFileClauses(folderName, filename, "max") ? 0 : 1;
    // Для альтернативы можно раскомментировать следующую строку и закомментировать вызов sortFileClauses:
    int sortResult = sort_clauses(folderName, filename);
    
    if (sortResult != 0) {
        return 1;
    }

    fs::path sortedPath = folderPath / ("sorted_" + filename);
    ifstream fin(sortedPath);
    if (!fin) {
        cerr << "Cannot open sorted file: " << sortedPath.string() << "\n";
        return 1;
    }

    anyNode = new Node{-1};
    root    = new Node{0};

    int totalClauses = 0;
    string line;
    while (getline(fin, line)) {
        if (line.empty())
            continue;
        if (line[0] == 'c' || line[0] == 'p')
            continue;
        ++totalClauses;
    }

    fin.clear();
    fin.seekg(0);

    int clauseIndex = 0;
    while (getline(fin, line)) {
        if (line.empty())
            continue;
        if (line[0] == 'c' || line[0] == 'p')
            continue;

        ++clauseIndex;
        cout << "Processing clause " << clauseIndex << " / " << totalClauses
             << ": " << line << "\n";

        istringstream iss(line);
        Clause c = readClause(iss);
        if (!c.empty())
            updateTree(root, c);
    }

    vector<Clause> patterns = getAllPatterns();

    auto isSubset = [&](const Clause& small, const Clause& big) {
        if (small.size() >= big.size()) return false;
        size_t i = 0, j = 0;
        while (i < small.size() && j < big.size()) {
            if (small[i].var == big[j].var && small[i].val == big[j].val) {
                i++; j++;
            } else if (small[i].var > big[j].var) {
                j++;
            } else {
                return false;
            }
        }
        return i == small.size();
    };

    vector<Clause> minimal;
    for (size_t i = 0; i < patterns.size(); ++i) {
        bool keep = true;
        for (size_t j = 0; j < patterns.size(); ++j) {
            if (i == j) continue;
            if (isSubset(patterns[j], patterns[i])) {
                keep = false;
                break;
            }
        }
        if (keep && !patterns[i].empty())
            minimal.push_back(patterns[i]);
    }

    sort(minimal.begin(), minimal.end(), [](const Clause& a, const Clause& b) {
        size_t n = min(a.size(), b.size());
        for (size_t i = 0; i < n; ++i) {
            if (a[i].var != b[i].var)
                return a[i].var < b[i].var;
            if (a[i].val != b[i].val)
                return b[i].val; // false before true
        }
        return a.size() < b.size();
    });

    cout << "Final patterns:\n";
    for (auto& pat : minimal) {
        for (auto& lit : pat)
            cout << (lit.val ? "" : "-") << lit.var << " ";
        cout << "0\n";
    }

    freeTree(root);
    delete anyNode;
    return 0;
}
