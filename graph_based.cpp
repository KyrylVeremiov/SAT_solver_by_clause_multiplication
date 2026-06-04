#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <functional>

// -------------------- Data structures --------------------

struct Node {
    int varIndex;      // variable index (x1, x2, ...)
    Node* parent;
    Node* childTrue;   // branch where variable is true
    Node* childFalse;  // branch where variable is false

    Node(int v = 0)
        : varIndex(v), parent(nullptr), childTrue(nullptr), childFalse(nullptr) {}
};

// Global dummy node: represents impossible path
Node* dummyNode = new Node(-1);

struct Literal {
    int var;      // variable index
    bool isNeg;   // true if literal is negated
    bool isLast;  // true if this is the last literal in clause
};

// -------------------- Utilities --------------------

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

        // False branch first, then True
        printChild(node->childFalse, 'F', indent);
        printChild(node->childTrue, 'T', indent);
    };

    printChildren(root, 2);
}

// -------------------- Conflict handling --------------------

void resolveConflictLastLiteral(Node* node, bool isNeg) {
    if (!node || node == dummyNode) return;

    if (isNeg) {
        // Last literal is negated: conflict when childFalse == dummyNode
        deleteSubtree(node->childTrue);
        node->childTrue = dummyNode;
    } else {
        // Last literal is positive: conflict when childTrue == dummyNode
        deleteSubtree(node->childFalse);
        node->childFalse = dummyNode;
    }

    // Upward deletion of parents until a node with both children != dummyNode
    // can be added here if needed, using parent pointers.
}

// -------------------- Core insertion logic --------------------

// Insert sequence of literals from clause[idx...] starting at current node
void insertSequenceFrom(Node*& current,
                        Node* parent,
                        const std::vector<Literal>& clause,
                        int idx) {
    if (idx >= (int)clause.size()) return;
    if (current == dummyNode) return; // impossible path

    const Literal& lit = clause[idx];
    bool isLast = lit.isLast;

    // If we are at a leaf (nullptr), insert literal here
    if (!current) {
        Node* newNode = new Node(lit.var);
        newNode->parent = parent;

        if (isLast) {
            // Last literal: one branch points to subtree below (currently nullptr),
            // other branch is dummyNode, depending on sign.
            if (lit.isNeg) {
                newNode->childFalse = nullptr;
                newNode->childTrue = dummyNode;
            } else {
                newNode->childTrue = nullptr;
                newNode->childFalse = dummyNode;
            }
        } else {
            // Non-last literal: both branches exist, both point to subtree below (nullptr here).
            newNode->childTrue = nullptr;
            newNode->childFalse = nullptr;
        }

        current = newNode;

        // If non-last, continue sequence down one branch depending on sign
        if (!isLast) {
            Node*& nextBranch = lit.isNeg ? newNode->childTrue : newNode->childFalse;
            insertSequenceFrom(nextBranch, newNode, clause, idx + 1);
        }

        return;
    }

    // Compare indices
    if (lit.var == current->varIndex) {
        // Node with this literal already exists
        if (isLast) {
            // Last literal: check for conflict
            if (lit.isNeg) {
                if (current->childFalse == dummyNode) {
                    resolveConflictLastLiteral(current, true);
                }
            } else {
                if (current->childTrue == dummyNode) {
                    resolveConflictLastLiteral(current, false);
                }
            }
        } else {
            // Non-last literal: continue with next literals down one branch
            Node*& nextBranch = lit.isNeg ? current->childTrue : current->childFalse;
            if (nextBranch == dummyNode) return; // path impossible
            insertSequenceFrom(nextBranch, current, clause, idx + 1);
        }
        return;
    }

    if (lit.var < current->varIndex) {
        // Insert between parent and current (before current)
        Node* newNode = new Node(lit.var);
        newNode->parent = parent;

        if (isLast) {
            // Last literal: one branch points to subtree below (current),
            // other branch is dummyNode, depending on sign.
            if (lit.isNeg) {
                newNode->childFalse = current;
                newNode->childTrue = dummyNode;
            } else {
                newNode->childTrue = current;
                newNode->childFalse = dummyNode;
            }
        } else {
            // Non-last literal: both branches exist
            // childTrue -> original subtree (current)
            // childFalse -> deep copy of subtree
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

    // lit.var > current->varIndex: we must go down
    bool canTrue = (current->childTrue != dummyNode);
    bool canFalse = (current->childFalse != dummyNode);

    // If node has both children not dummyNode, this is a branching point:
    // we must insert the whole sequence clause[idx...] into both branches.
    if (canTrue && canFalse) {
        insertSequenceFrom(current->childFalse, current, clause, idx);
        insertSequenceFrom(current->childTrue, current, clause, idx);
        return;
    }

    // Only one branch is possible
    if (canFalse && (!current->childTrue || current->childTrue == dummyNode)) {
        insertSequenceFrom(current->childFalse, current, clause, idx);
        return;
    }
    if (canTrue && (!current->childFalse || current->childFalse == dummyNode)) {
        insertSequenceFrom(current->childTrue, current, clause, idx);
        return;
    }

    // Both branches are dummyNode: path impossible
    return;
}

// Process one clause: literals are inserted sequentially from root
void insertClause(Node*& root, const std::vector<Literal>& clause) {
    if (clause.empty()) return;
    insertSequenceFrom(root, nullptr, clause, 0);
}

// -------------------- DIMACS parsing --------------------

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
        if (line[0] == 'c') continue; // comment
        if (line[0] == 'p') continue; // problem line

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

// -------------------- Main --------------------

int main() {
    Node* root = nullptr;

    std::vector<std::vector<Literal>> clauses;
    if (!parseCNF("test_cases/test_sat.cnf", clauses)) {
        return 1;
    }

    // For each clause, insert its literals sequentially starting from root
    for (const auto& clause : clauses) {
        insertClause(root, clause);
    }

    // Print resulting pseudo-tree
    printTree(root);

    // Cleanup
    deleteSubtree(root);
    delete dummyNode;

    return 0;
}
