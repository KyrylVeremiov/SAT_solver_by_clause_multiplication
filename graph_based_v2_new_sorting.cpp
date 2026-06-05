#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <functional>
#include "sorting_clauses_min_index.h"

struct Node {
    int varIndex;
    Node* parent;
    Node* childTrue;
    Node* childFalse;

    Node(int v = 0)
        : varIndex(v), parent(nullptr), childTrue(nullptr), childFalse(nullptr) {}
};

Node* dummyNode = new Node(-1);

struct Literal {
    int var;
    bool isNeg;
    bool isLast;
};

Node* deepCopy(Node* root, Node* parent = nullptr) {
    if (!root || root == dummyNode)
        return root;

    Node* copy = new Node(root->varIndex);
    copy->parent = parent;

    copy->childTrue  = deepCopy(root->childTrue,  copy);
    copy->childFalse = deepCopy(root->childFalse, copy);

    return copy;
}

void deleteSubtree(Node* root) {
    if (!root || root == dummyNode) return;

    Node* t = root->childTrue;
    Node* f = root->childFalse;

    root->childTrue  = nullptr;
    root->childFalse = nullptr;

    if (t && t != dummyNode) deleteSubtree(t);
    if (f && f != dummyNode) deleteSubtree(f);

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
    if (!node || node == dummyNode) return;

    if (isNeg) {
        if (node->childTrue && node->childTrue != dummyNode)
            deleteSubtree(node->childTrue);
        node->childTrue = dummyNode;
    } else {
        if (node->childFalse && node->childFalse != dummyNode)
            deleteSubtree(node->childFalse);
        node->childFalse = dummyNode;
    }

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
            if (lit.isNeg) {
                newNode->childFalse = nullptr;
                newNode->childTrue  = dummyNode;
            } else {
                newNode->childTrue  = nullptr;
                newNode->childFalse = dummyNode;
            }
        } else {
            newNode->childTrue  = nullptr;
            newNode->childFalse = nullptr;
        }

        current = newNode;

        if (!isLast) {
            Node*& nextBranch = lit.isNeg ? newNode->childTrue : newNode->childFalse;
            insertSequenceFrom(nextBranch, newNode, clause, idx + 1, root);
        }

        return;
    }

    if (lit.var == current->varIndex) {
        if (isLast) {
            if (lit.isNeg) {
                if (current->childFalse == dummyNode)
                    resolveConflictLastLiteral(current, true, root);
                else {
                    if (current->childTrue && current->childTrue != dummyNode)
                        deleteSubtree(current->childTrue);
                    current->childTrue = dummyNode;
                    pruneUp(current, root);
                }
            } else {
                if (current->childTrue == dummyNode)
                    resolveConflictLastLiteral(current, false, root);
                else {
                    if (current->childFalse && current->childFalse != dummyNode)
                        deleteSubtree(current->childFalse);
                    current->childFalse = dummyNode;
                    pruneUp(current, root);
                }
            }
        } else {
            Node*& nextBranch = lit.isNeg ? current->childTrue : current->childFalse;
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
            if (lit.isNeg) {
                newNode->childFalse = old;
                newNode->childTrue  = dummyNode;
            } else {
                newNode->childTrue  = old;
                newNode->childFalse = dummyNode;
            }
            old->parent = newNode;
        } else {
            newNode->childTrue  = old;
            newNode->childFalse = deepCopy(old, newNode);
            old->parent = newNode;
        }

        if (parent) {
            if (parent->childTrue == old) parent->childTrue = newNode;
            else if (parent->childFalse == old) parent->childFalse = newNode;
        }

        current = newNode;

        if (!isLast) {
            Node*& nextBranch = lit.isNeg ? newNode->childTrue : newNode->childFalse;
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
              std::vector<std::vector<Literal>>& clauses) {
    std::ifstream in(filename);
    if (!in) {
        std::cerr << "Error: cannot open file " << filename << "\n";
        return false;
    }

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        if (line[0] == 'c') continue;
        if (line[0] == 'p') continue;

        std::istringstream iss(line);
        int x;
        std::vector<int> raw;
        while (iss >> x) {
            if (x == 0) break;
            raw.push_back(x);
        }
        if (raw.empty()) continue;

        std::vector<Literal> clause;
        for (size_t i = 0; i < raw.size(); ++i) {
            int v = raw[i];
            Literal lit;
            lit.var    = std::abs(v);
            lit.isNeg  = (v < 0);
            lit.isLast = (i == raw.size() - 1);
            clause.push_back(lit);
        }
        clauses.push_back(clause);
    }

    return true;
}

int main() {
    Node* root = nullptr;

    // std::string filename    = "test_sat.cnf";
    // std::string filename    = "uf20-01.cnf";
    // std::string filename    = "uf20-05.cnf";
    std::string filename    = "uuf50-01.cnf";
    // std::string filename    = "uf75-098.cnf";
    // std::string filename    = "uuf75-097.cnf";

    
    std::string folder_name = "test_cases/";

    sort_clauses(folder_name, filename);

    std::vector<std::vector<Literal>> clauses;
    if (!parseCNF(folder_name + "sorted_" + filename, clauses)) {
        return 1;
    }


    long long total = clauses.size();
    long long index = 1;

    for (const auto& clause : clauses) {

        std::cout << "Clause " << index << "/" << total << "    ";
        index++;

        insertClause(root, clause);
        std::cout << "Inserted clause: ";
        for (const auto& lit : clause) {
            std::cout << (lit.isNeg ? "-" : "") << "x" << lit.var << " ";
        }
        std::cout << "\n";
    }

    std::cout << "Constructed pseudo-tree:\n\n";
    printTree(root);

    if (root != dummyNode)
        deleteSubtree(root);

    delete dummyNode;

    return 0;
}
