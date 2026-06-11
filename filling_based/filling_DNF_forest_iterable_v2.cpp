//Iterative version of filling DNF forest, with some optimizations. 
// The first option is to take a clause with the most amount of confilcts with all patterns and insert it.
// The second opion to peack at each step a clause with the highest average conflict fraction,
//  then insert it and update the tree.
//  

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <algorithm>
#include <filesystem>
#include <cctype>
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
    vector<pair<int, Node*>> childTrue;
    vector<pair<int, Node*>> childFalse;
};

using Clause = vector<Literal>;

void freeTree(Node* node);

#include "iterative_sorting.h"

Node* anyNode = nullptr;
Node* root    = nullptr;

bool hasEmptyPattern = true;

inline bool literalLess(const Literal& a, const Literal& b) {
    return a.var < b.var;
}

inline bool containsVar(const Clause& pat, int var) {
    auto it = lower_bound(pat.begin(), pat.end(), var,
                          [](const Literal& lit, int value) {
                              return lit.var < value;
                          });
    return it != pat.end() && it->var == var;
}

Clause addLiteralSorted(const Clause& pat, Literal lit) {
    Clause res;
    res.reserve(pat.size() + 1);
    auto it = lower_bound(pat.begin(), pat.end(), lit,
                          [](const Literal& a, const Literal& b) {
                              return a.var < b.var;
                          });
    res.insert(res.end(), pat.begin(), it);
    res.push_back(lit);
    res.insert(res.end(), it, pat.end());
    return res;
}

static Node* findChild(Node* node, int var, bool val) {
    auto& children = val ? node->childTrue : node->childFalse;
    for (auto& [k, child] : children) {
        if (k == var)
            return child;
    }
    return nullptr;
}

static Node* insertChild(Node* node, int var, bool val) {
    auto& children = val ? node->childTrue : node->childFalse;
    for (auto& [k, child] : children) {
        if (k == var)
            return child;
    }
    Node* nxt = new Node{var};
    children.emplace_back(var, nxt);
    return nxt;
}

void insertPattern(Node* root, const Clause& pat) {
    if (pat.empty()) {
        hasEmptyPattern = true;
        return;
    }

    Node* cur = root;
    for (auto& lit : pat) {
        cur = insertChild(cur, lit.var, lit.val);
    }

    auto& children = pat.back().val ? cur->childTrue : cur->childFalse;
    for (auto& [k, child] : children) {
        if (k == -1)
            return;
    }
    children.emplace_back(-1, anyNode);
}

bool removePatternRec(Node* node, const Clause& pat, size_t pos) {
    if (pos == pat.size()) {
        auto& children = pat.back().val ? node->childTrue : node->childFalse;
        for (size_t i = 0; i < children.size(); ++i) {
            if (children[i].first == -1) {
                children.erase(children.begin() + i);
                break;
            }
        }
        return node->childTrue.empty() && node->childFalse.empty();
    }

    auto& children = pat[pos].val ? node->childTrue : node->childFalse;
    for (size_t i = 0; i < children.size(); ++i) {
        if (children[i].first == pat[pos].var) {
            Node* child = children[i].second;
            if (removePatternRec(child, pat, pos + 1)) {
                freeTree(child);
                children.erase(children.begin() + i);
            }
            break;
        }
    }
    return node->childTrue.empty() && node->childFalse.empty();
}

void removePattern(Node* root, const Clause& pat) {
    if (pat.empty()) {
        hasEmptyPattern = false;
        return;
    }

    removePatternRec(root, pat, 0);
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
    size_t i = 0, j = 0;
    while (i < T.size() && j < C.size()) {
        if (T[i].var < C[j].var) {
            ++i;
        } else if (T[i].var > C[j].var) {
            ++j;
        } else {
            if (T[i].val == C[j].val)
                return true;
            ++i;
            ++j;
        }
    }
    return false;
}

bool containsOppositeLiteral(const Clause& T, int var, bool val) {
    auto it = lower_bound(T.begin(), T.end(), var,
                          [](const Literal& lit, int value) {
                              return lit.var < value;
                          });
    return it != T.end() && it->var == var && it->val != val;
}

void updateTree(Node* root, const Clause& clause, const vector<Clause>& patterns) {
    for (auto const& T : patterns) {
        if (T.empty()) {
            removePattern(root, T);
            for (auto const& lit : clause)
                insertPattern(root, Clause{lit});
            continue;
        }

        bool satisfied = containsSameLiteral(T, clause);
        if (satisfied)
            continue;

        bool inserted = false;
        for (auto const& lit : clause) {
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

void updateTree(Node* root, const Clause& clause) {
    updateTree(root, clause, getAllPatterns());
}

Clause readClause(const string& line) {
    Clause c;
    c.reserve(16);
    const char* p = line.c_str();
    while (*p) {
        while (*p && isspace(static_cast<unsigned char>(*p)))
            ++p;
        if (!*p)
            break;

        int sign = 1;
        if (*p == '-') {
            sign = -1;
            ++p;
        } else if (*p == '+') {
            ++p;
        }

        if (!isdigit(static_cast<unsigned char>(*p))) {
            while (*p && !isspace(static_cast<unsigned char>(*p)))
                ++p;
            continue;
        }

        int x = 0;
        while (isdigit(static_cast<unsigned char>(*p))) {
            x = x * 10 + (*p - '0');
            ++p;
        }
        x *= sign;
        if (x == 0)
            break;
        c.push_back({x > 0 ? x : -x, x > 0});
    }
    sort(c.begin(), c.end(), literalLess);
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
    // candidates.emplace_back("../test_cases/uf20-01.cnf");
    // candidates.emplace_back("../test_cases/uf20-05.cnf");
    candidates.emplace_back("../test_cases/uuf50-01.cnf");
    // candidates.emplace_back("../test_cases/uf75-098.cnf");
    // candidates.emplace_back("../test_cases/uuf75-097.cnf");
    

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

    vector<Clause> remainingClauses;
    string line;
    while (getline(fin, line)) {
        if (line.empty())
            continue;
        if (line[0] == 'c' || line[0] == 'p')
            continue;

        Clause c = readClause(line);
        if (!c.empty())
            remainingClauses.emplace_back(std::move(c));
    }

    int totalClauses = static_cast<int>(remainingClauses.size());
    int clauseIndex = 0;
    while (!remainingClauses.empty()) {
        vector<Clause> patterns = getAllPatterns();
        
        size_t next = selectHighestAverageConflictClauseIndexParallel(remainingClauses, patterns, 8);
        // Для использования прошлой версии раскомментируйте строку ниже:
        // size_t next = selectMostConflictingClauseIndexParallel(remainingClauses, patterns, 8);

        Clause c = std::move(remainingClauses[next]);
        if (next + 1 < remainingClauses.size())
            remainingClauses[next] = std::move(remainingClauses.back());
        remainingClauses.pop_back();

        ++clauseIndex;
        cout << "Processing clause " << clauseIndex << " / " << totalClauses << ": ";
        for (auto& lit : c)
            cout << (lit.val ? "" : "-") << lit.var << " ";
        cout << "0\n";

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
