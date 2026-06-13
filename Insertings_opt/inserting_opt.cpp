#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <functional>

#include "sorting_max_min.h"
#include "../sorting_clauses_min_index.h"
#include "../filling_based/filling_sorting.h"
#include "sort_clauses_min_range.h"

struct Node {
    int varIndex;
    Node* parent;
    Node* childTrue;
    Node* childFalse;

    Node(int v = 0)
        : varIndex(v), parent(nullptr), childTrue(nullptr), childFalse(nullptr) {}
};

static Node dummyNodeObj(-1);
Node* dummyNode = &dummyNodeObj;

struct Literal {
    int var;
    bool isNeg;
    bool isLast;
};

inline bool isDummy(Node* node) {
    return node == nullptr || node == dummyNode;
}

inline bool isReal(Node* node) {
    return node && node != dummyNode;
}

inline Node*& branchRef(Node* node, bool takeTrue) {
    return takeTrue ? node->childTrue : node->childFalse;
}

inline Node*& branchRef(Node* node, const Literal& lit) {
    return lit.isNeg ? node->childTrue : node->childFalse;
}

void deleteSubtree(Node* root);

inline void deleteChild(Node*& child) {
    if (isReal(child)) {
        deleteSubtree(child);
        child = dummyNode;
    }
}

Node* deepCopy(Node* root, Node* parent = nullptr) {
    if (!isReal(root))
        return root;

    Node* copy = new Node(root->varIndex);
    copy->parent = parent;

    copy->childTrue  = deepCopy(root->childTrue,  copy);
    copy->childFalse = deepCopy(root->childFalse, copy);

    return copy;
}

void deleteSubtree(Node* root) {
    if (!isReal(root)) return;

    Node* t = root->childTrue;
    Node* f = root->childFalse;

    root->childTrue  = nullptr;
    root->childFalse = nullptr;

    if (isReal(t)) deleteSubtree(t);
    if (isReal(f)) deleteSubtree(f);

    delete root;
}

// -----------------------------------------------------------------------------
// pruneUp: removes a node, prunes up while the parent is also empty
// -----------------------------------------------------------------------------
void pruneUp(Node* node, Node*& root) {
    while (node) {
        bool bothDummy = (node->childTrue == dummyNode && node->childFalse == dummyNode);

        if (!bothDummy)
            return;

        Node* parent = node->parent;

        if (!parent) {
            deleteSubtree(node);
            root = dummyNode;
            return;
        }

        bool isTrue = (parent->childTrue == node);

        deleteSubtree(node);

        if (isTrue) parent->childTrue = dummyNode;
        else        parent->childFalse = dummyNode;

        node = parent;
    }
}

void printTree(Node* root) {
    std::cout << "Pseudo-tree:\n";
    if (!root || root == dummyNode) {
        std::cout << "dummyNode\n";
        return;
    }

    std::function<void(Node*, int)> printChildren = [&](Node* node, int indent) {
        if (!node || node == dummyNode) return;

        auto printChild = [&](Node* child, char branch, int indentLevel) {
            for (int i = 0; i < indentLevel; ++i) std::cout << ' ';
            std::cout << branch << "-> ";
            if (!child) {
                std::cout << "null\n";
            } else if (child == dummyNode) {
                std::cout << "dummy\n";
            } else {
                std::cout << "x" << child->varIndex << "\n";
                printChildren(child, indentLevel + 2);
            }
        };

        printChild(node->childFalse, 'F', indent);
        printChild(node->childTrue,  'T', indent);
    };

    std::cout << "x" << root->varIndex << "\n";
    printChildren(root, 2);
}

void resolveConflictLastLiteral(Node* node, bool isNeg, Node*& root) {
    if (!isReal(node)) return;

    Node*& branch = isNeg ? node->childTrue : node->childFalse;
    deleteChild(branch);
    branch = dummyNode;
    pruneUp(node, root);
}

void insertSequenceFrom(Node*& current,
                        Node* parent,
                        const std::vector<Literal>& clause,
                        int idx,
                        Node*& root) {
    if (root == dummyNode) return;
    if (idx >= (int)clause.size()) return;
    if (current == dummyNode) return;

    const Literal& lit = clause[idx];
    bool isLast = lit.isLast;

    if (!current) {
        Node* newNode = new Node(lit.var);
        newNode->parent = parent;

        if (isLast) {
            branchRef(newNode, lit.isNeg) = dummyNode;
            branchRef(newNode, !lit.isNeg) = nullptr;
        } else {
            newNode->childTrue  = nullptr;
            newNode->childFalse = nullptr;
        }

        current = newNode;

        if (!isLast) {
            Node*& nextBranch = branchRef(newNode, lit.isNeg);
            insertSequenceFrom(nextBranch, newNode, clause, idx + 1, root);
        }

        return;
    }

    if (lit.var == current->varIndex) {
        if (isLast) {
            Node*& branch = lit.isNeg ? current->childTrue : current->childFalse;
            Node*& opposite = lit.isNeg ? current->childFalse : current->childTrue;

            if (opposite == dummyNode) {
                resolveConflictLastLiteral(current, lit.isNeg, root);
            } else {
                deleteChild(branch);
                branch = dummyNode;
                pruneUp(current, root);
            }
        } else {
            Node*& nextBranch = branchRef(current, lit.isNeg);
            if (nextBranch == dummyNode) return;
            insertSequenceFrom(nextBranch, current, clause, idx + 1, root);
        }
        return;
    }

    if (lit.var < current->varIndex) {
        Node* old = current;
        Node* newNode = new Node(lit.var);
        newNode->parent = parent;

        if (isLast) {
            branchRef(newNode, !lit.isNeg) = old;
            branchRef(newNode, lit.isNeg) = dummyNode;
            old->parent = newNode;
        } else {
            branchRef(newNode, !lit.isNeg) = old;
            branchRef(newNode, lit.isNeg) = deepCopy(old, newNode);
            old->parent = newNode;
        }

        if (parent) {
            if (parent->childTrue == old) parent->childTrue = newNode;
            else if (parent->childFalse == old) parent->childFalse = newNode;
        }

        current = newNode;

        if (!isLast) {
            Node*& nextBranch = branchRef(newNode, lit.isNeg);
            insertSequenceFrom(nextBranch, newNode, clause, idx + 1, root);
        }

        return;
    }

    bool canTrue  = (current->childTrue  != dummyNode);
    bool canFalse = (current->childFalse != dummyNode);

    if (canTrue && canFalse) {
        insertSequenceFrom(current->childFalse, current, clause, idx, root);
        insertSequenceFrom(current->childTrue,  current, clause, idx, root);
        return;
    }

    if (canFalse && (!current->childTrue || current->childTrue == dummyNode)) {
        insertSequenceFrom(current->childFalse, current, clause, idx, root);
        return;
    }

    if (canTrue && (!current->childFalse || current->childFalse == dummyNode)) {
        insertSequenceFrom(current->childTrue, current, clause, idx, root);
        return;
    }

    return;
}

void insertClause(Node*& root, const std::vector<Literal>& clause) {
    if (clause.empty()) return;
    insertSequenceFrom(root, nullptr, clause, 0, root);
}

bool parseCNF(const std::string& filename,
              const std::function<bool(const std::vector<Literal>&)>& callback) {
    std::ifstream in(filename);
    if (!in) {
        std::cerr << "Error: cannot open file " << filename << "\n";
        return false;
    }

    std::string line;
    std::vector<Literal> clause;
    clause.reserve(64);

    while (std::getline(in, line)) {
        if (line.empty() || line[0] == 'c' || line[0] == 'p')
            continue;

        std::istringstream iss(line);
        int x;
        clause.clear();

        while (iss >> x) {
            if (x == 0) break;
            clause.push_back({std::abs(x), x < 0, false});
        }

        if (clause.empty()) continue;
        clause.back().isLast = true;

        if (!callback(clause))
            return false;
    }

    return true;
}

int countClauses(const std::string& filename) {
    std::ifstream in(filename);
    if (!in) return -1;

    std::string line;
    int count = 0;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == 'c' || line[0] == 'p')
            continue;
        count++;
    }
    return count;
}

int main() {
    Node* root = nullptr;

    // std::string filename    = "test_test.cnf";
    // std::string filename    = "test_sat.cnf";
    // std::string filename    = "uf20-01.cnf";
    // std::string filename    = "uf20-05.cnf";
    // std::string filename    = "uuf50-01.cnf";
    // std::string filename    = "uf75-098.cnf";
    std::string filename    = "uuf75-097.cnf";

    std::string folder_name = "../test_cases/";

    sort_clauses_max_min_index(folder_name, filename); // best memory usage?
    // sort_clauses(folder_name, filename);

    // sort_clauses_min_range(folder_name, filename);
    // sort_clauses_max_min_index(folder_name, filename);
    // sort_clauses_min_index(folder_name, filename);
    // sort_clauses(folder_name, filename);
    // sortFileClauses(folder_name, filename, "min");

    const std::string sortedFilename = folder_name + "sorted_" + filename;
    int total = countClauses(sortedFilename);
    if (total < 0) {
        std::cerr << "Error: cannot open file " << sortedFilename << "\n";
        return 1;
    }

    long long index = 1;
    if (!parseCNF(sortedFilename, [&](const std::vector<Literal>& clause) {
            std::cout << "Clause " << index << "/" << total << "    ";
            insertClause(root, clause);
            std::cout << "Inserted clause: ";
            for (const auto& lit : clause) {
                std::cout << (lit.isNeg ? "-" : "") << "x" << lit.var << " ";
            }
            std::cout << "\n";
            ++index;
            return true;
        })) {
        return 1;
    }

    std::cout << "Constructed pseudo-tree:\n\n";
    printTree(root);

    if (root != dummyNode)
        deleteSubtree(root);

    return 0;
}
