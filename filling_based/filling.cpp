#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <map>
#include <algorithm>
#include "../sorting_clauses_min_index.h"

using namespace std;

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

// Empty pattern exists and participates in updates
bool hasEmptyPattern = true;

// ---------- Helper functions ----------

map<int,bool> toMap(const Clause& c) {
    map<int,bool> m;
    for (auto& lit : c) m[lit.var] = lit.val;
    return m;
}

bool containsVar(const Clause& pat, int var) {
    for (auto& lit : pat)
        if (lit.var == var) return true;
    return false;
}

Clause addLiteralSorted(const Clause& pat, Literal lit) {
    Clause res = pat;
    res.push_back(lit);
    sort(res.begin(), res.end(),
         [](const Literal& a, const Literal& b) {
             return a.var < b.var;
         });
    return res;
}

// ---------- Free subtree ----------

void freeSubtree(Node* node) {
    if (!node || node == anyNode) return;
    for (auto& [_, child] : node->childTrue) freeSubtree(child);
    for (auto& [_, child] : node->childFalse) freeSubtree(child);
    delete node;
}

// ---------- Insert pattern ----------

void insertPattern(Node* root, const Clause& pat) {
    if (pat.empty()) {
        hasEmptyPattern = true;
        return;
    }

    Node* cur = root;

    for (auto& lit : pat) {

        auto& mp = lit.val ? cur->childTrue : cur->childFalse;

        // If THIS branch already has anyNode → insertion forbidden
        if (mp.count(-1)) {
            return;
        }

        auto it = mp.find(lit.var);
        if (it == mp.end()) {
            Node* nxt = new Node{lit.var};
            mp[lit.var] = nxt;
            cur = nxt;
        } else {
            cur = it->second;
        }
    }

    // Set ANYNODE ONLY in the correct branch
    auto& mpLeaf = pat.back().val ? cur->childTrue : cur->childFalse;

    // If this branch already has anyNode — nothing to do
    if (mpLeaf.count(-1)) return;

    // Otherwise set leaf marker
    mpLeaf[-1] = anyNode;
}

// ---------- Remove pattern ----------

void removePattern(Node* root, const Clause& pat) {
    if (pat.empty()) {
        hasEmptyPattern = false;
        return;
    }

    Node* cur = root;
    for (size_t i = 0; i < pat.size(); ++i) {
        const auto& lit = pat[i];
        auto& mp = lit.val ? cur->childTrue : cur->childFalse;
        auto it = mp.find(lit.var);
        if (it == mp.end()) return;
        cur = it->second;
    }

    auto& mpLeaf = pat.back().val ? cur->childTrue : cur->childFalse;
    auto itLeaf = mpLeaf.find(-1);
    if (itLeaf != mpLeaf.end()) mpLeaf.erase(itLeaf);
}

// ---------- Collect patterns ----------

void collectPatterns(Node* node,
                     vector<Literal>& cur,
                     vector<Clause>& out)
{
    for (auto& [var, child] : node->childTrue) {
        if (var == -1 && child == anyNode)
            out.push_back(cur);
        else {
            cur.push_back({var, true});
            collectPatterns(child, cur, out);
            cur.pop_back();
        }
    }

    for (auto& [var, child] : node->childFalse) {
        if (var == -1 && child == anyNode)
            out.push_back(cur);
        else {
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

// ---------- Checks ----------

bool isFullNegation(const Clause& T, const Clause& C) {
    if (T.size() != C.size()) return false;
    auto mT = toMap(T);
    auto mC = toMap(C);
    for (auto& [v, valC] : mC) {
        auto it = mT.find(v);
        if (it == mT.end()) return false;
        if (it->second == valC) return false;
    }
    return true;
}

bool isSubsetCompatible(const Clause& T, const Clause& C) {
    if (T.empty()) return false;
    auto mC = toMap(C);
    for (auto& lit : T) {
        auto it = mC.find(lit.var);
        if (it == mC.end()) return false;
        if (it->second != lit.val) return false;
    }
    return true;
}

// ---------- Update tree ----------

void updateTree(Node* root, const Clause& clause) {
    vector<Clause> patterns = getAllPatterns();

    for (const auto& T : patterns) {

        if (isFullNegation(T, clause)) {
            removePattern(root, T);
            continue;
        }

        if (isSubsetCompatible(T, clause)) {
            continue;
        }

        removePattern(root, T);

        auto mC = toMap(clause);

        for (auto& [v, valC] : mC) {
            if (!containsVar(T, v)) {
                Clause newPat = addLiteralSorted(T, Literal{v, valC});
                insertPattern(root, newPat);
            }
        }
    }
}

// ---------- Read clause ----------

Clause readClause(istringstream& iss) {
    Clause c;
    int x;
    while (iss >> x) {
        if (x == 0) break;
        if (x > 0) c.push_back({x, true});
        else       c.push_back({-x, false});
    }
    return c;
}

// ---------- Free whole tree ----------

void freeTree(Node* node) {
    if (!node || node == anyNode) return;
    for (auto& [_, child] : node->childTrue) freeTree(child);
    for (auto& [_, child] : node->childFalse) freeTree(child);
    delete node;
}

// ---------- MAIN ----------

int main() {
    std::string filename    = "test_test.cnf";
    // std::string filename    = "uf20-01.cnf";
    std::string folder_name = "../test_cases/";

    sort_clauses(folder_name, filename);

    anyNode = new Node{-1};
    root    = new Node{0};

    int totalClauses = 0;
    {
        ifstream countFile(folder_name + "sorted_" + filename);
        string tmp;
        while (getline(countFile, tmp)) {
            if (tmp.empty()) continue;
            if (tmp[0] == 'c') continue;
            if (tmp[0] == 'p') continue;
            totalClauses++;
        }
    }

    ifstream fin(folder_name +  filename);
    if (!fin) {
        cerr << "Cannot open file\n";
        return 1;
    }

    string line;
    int clauseIndex = 0;

    while (getline(fin, line)) {
        if (line.empty()) continue;
        if (line[0] == 'c') continue;
        if (line[0] == 'p') continue;

        clauseIndex++;
        cout << "Processing clause " << clauseIndex << " / " << totalClauses
             << ": " << line << "\n";

        istringstream iss(line);
        Clause c = readClause(iss);
        if (!c.empty()) updateTree(root, c);
    }

    vector<Clause> patterns = getAllPatterns();

    cout << "Final patterns:\n";
    for (const auto& pat : patterns) {
        if (pat.empty()) continue;
        for (auto& lit : pat)
            cout << (lit.val ? "" : "-") << lit.var << " ";
        cout << "0\n";
    }

    freeTree(root);
    delete anyNode;

    return 0;
}
