#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <functional>
#include "sorting_clauses.h"

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
    if (!root || root == dummyNode) return root;
    Node* copy = new Node(root->varIndex);
    copy->parent = parent;
    copy->childTrue = deepCopy(root->childTrue, copy);
    copy->childFalse = deepCopy(root->childFalse, copy);
    return copy;
}

void deleteSubtree(Node* root) {
    if (!root || root == dummyNode) return;
    deleteSubtree(root->childTrue);
    deleteSubtree(root->childFalse);
    delete root;
}

void printTree(Node* root) {
    std::cout << "Pseudo-tree:\n";
    if (!root || root == dummyNode) {
        std::cout << "null\n";
        return;
    }

    std::cout << "x" << root->varIndex << "\n";

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
        printChild(node->childTrue, 'T', indent);
    };

    printChildren(root, 2);
}

void resolveConflictLastLiteral(Node* node, bool isNeg) {
    if (!node || node == dummyNode) return;

    if (isNeg) {
        deleteSubtree(node->childTrue);
        node->childTrue = dummyNode;
    } else {
        deleteSubtree(node->childFalse);
        node->childFalse = dummyNode;
    }

    Node* current = node;
    Node* parent = current->parent;
    Node* child = current;

    while (parent) {
        bool parentCanTrue = (parent->childTrue != dummyNode);
        bool parentCanFalse = (parent->childFalse != dummyNode);

        if (parentCanTrue && parentCanFalse) {
            if (parent->childTrue == child) {
                deleteSubtree(parent->childTrue);
                parent->childTrue = dummyNode;
            } else if (parent->childFalse == child) {
                deleteSubtree(parent->childFalse);
                parent->childFalse = dummyNode;
            }
            break;
        }

        Node* grand = parent->parent;

        if (grand) {
            if (grand->childTrue == parent) {
                grand->childTrue = dummyNode;
            } else if (grand->childFalse == parent) {
                grand->childFalse = dummyNode;
            }
        }

        deleteSubtree(parent);

        child = parent;
        parent = grand;
    }
}

void insertSequenceFrom(Node*& current,
                        Node* parent,
                        const std::vector<Literal>& clause,
                        int idx) {
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
                newNode->childTrue = dummyNode;
            } else {
                newNode->childTrue = nullptr;
                newNode->childFalse = dummyNode;
            }
        } else {
            newNode->childTrue = nullptr;
            newNode->childFalse = nullptr;
        }

        current = newNode;

        if (!isLast) {
            Node*& nextBranch = lit.isNeg ? newNode->childTrue : newNode->childFalse;
            insertSequenceFrom(nextBranch, newNode, clause, idx + 1);
        }

        return;
    }

    if (lit.var == current->varIndex) {
        if (isLast) {
            if (lit.isNeg) {
                // Last literal is negated: satisfying branch is childFalse
                if (current->childFalse == dummyNode) {
                    // Conflict: satisfying branch already impossible
                    resolveConflictLastLiteral(current, true);
                } else {
                    // Normal case: forbid True, delete subtree below childTrue
                    deleteSubtree(current->childTrue);
                    current->childTrue = dummyNode;
                }
            } else {
                // Last literal is positive: satisfying branch is childTrue
                if (current->childTrue == dummyNode) {
                    // Conflict: satisfying branch already impossible
                    resolveConflictLastLiteral(current, false);
                } else {
                    // Normal case: forbid False, delete subtree below childFalse
                    deleteSubtree(current->childFalse);
                    current->childFalse = dummyNode;
                }
            }
        } else {
            Node*& nextBranch = lit.isNeg ? current->childTrue : current->childFalse;
            if (nextBranch == dummyNode) return;
            insertSequenceFrom(nextBranch, current, clause, idx + 1);
        }
        return;
    }

    if (lit.var < current->varIndex) {
        Node* newNode = new Node(lit.var);
        newNode->parent = parent;

        if (isLast) {
            if (lit.isNeg) {
                newNode->childFalse = current;
                newNode->childTrue = dummyNode;
            } else {
                newNode->childTrue = current;
                newNode->childFalse = dummyNode;
            }
        } else {
            newNode->childTrue = current;
            newNode->childFalse = deepCopy(current, newNode);
            current->parent = newNode;
        }

        current = newNode;

        if (!isLast) {
            Node*& nextBranch = lit.isNeg ? newNode->childTrue : newNode->childFalse;
            insertSequenceFrom(nextBranch, newNode, clause, idx + 1);
        }

        return;
    }

    bool canTrue = (current->childTrue != dummyNode);
    bool canFalse = (current->childFalse != dummyNode);

    if (canTrue && canFalse) {
        insertSequenceFrom(current->childFalse, current, clause, idx);
        insertSequenceFrom(current->childTrue, current, clause, idx);
        return;
    }

    if (canFalse && (!current->childTrue || current->childTrue == dummyNode)) {
        insertSequenceFrom(current->childFalse, current, clause, idx);
        return;
    }
    if (canTrue && (!current->childFalse || current->childFalse == dummyNode)) {
        insertSequenceFrom(current->childTrue, current, clause, idx);
        return;
    }

    return;
}

void insertClause(Node*& root, const std::vector<Literal>& clause) {
    if (clause.empty()) return;
    insertSequenceFrom(root, nullptr, clause, 0);
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
            lit.var = std::abs(v);
            lit.isNeg = (v < 0);
            lit.isLast = (i == raw.size() - 1);
            clause.push_back(lit);
        }
        clauses.push_back(clause);
    }

    return true;
}

int main() {
    Node* root = nullptr;

    // string filename = "test_sat.cnf";
    string filename = "uuf75-097.cnf";
    // string filename = "uf75-098.cnf";
    string folder_name = "test_cases/";

    sort_clauses(folder_name, filename);

    std::vector<std::vector<Literal>> clauses;
    if (!parseCNF(folder_name +"sorted_" + filename, clauses)) {
        return 1;
    }

    for (const auto& clause : clauses) {
        insertClause(root, clause);
    }

    printTree(root);

    deleteSubtree(root);
    delete dummyNode;

    return 0;
}
