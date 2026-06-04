#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <sstream>

// Node of pseudo-tree
struct Node {
    int varIndex;      // variable index x_k
    Node* parent;
    Node* childTrue;
    Node* childFalse;

    Node(int idx = -1)
        : varIndex(idx), parent(nullptr), childTrue(nullptr), childFalse(nullptr) {}
};

// Global dummy node (represents impossible value)
Node* DUMMY = new Node(-1);

// Root of the pseudo-tree
Node* root = nullptr;

// ---------- Utility functions ----------

// Deep copy of a subtree (excluding dummy)
Node* cloneSubtree(Node* node, Node* parent = nullptr) {
    if (!node || node == DUMMY) return node;
    Node* copy = new Node(node->varIndex);
    copy->parent = parent;
    copy->childTrue = cloneSubtree(node->childTrue, copy);
    copy->childFalse = cloneSubtree(node->childFalse, copy);
    return copy;
}

// Delete subtree (excluding dummy)
void deleteSubtree(Node* node) {
    if (!node || node == DUMMY) return;
    deleteSubtree(node->childTrue);
    deleteSubtree(node->childFalse);
    delete node;
}

// Conflict handling: delete one branch below node and prune upwards
void handleConflict(Node* node, bool deleteTrueBranch) {
    if (!node) return;

    // Delete conflicting branch below this node
    if (deleteTrueBranch) {
        deleteSubtree(node->childTrue);
        node->childTrue = DUMMY;
    } else {
        deleteSubtree(node->childFalse);
        node->childFalse = DUMMY;
    }

    // Climb up until a node where both children are not DUMMY
    Node* cur = node;
    Node* parent = cur->parent;
    bool cameFromTrue = false;

    if (parent) {
        cameFromTrue = (parent->childTrue == cur);
    }

    while (parent) {
        bool tDummy = (parent->childTrue == DUMMY);
        bool fDummy = (parent->childFalse == DUMMY);

        // If both children are not DUMMY, stop
        if (!tDummy && !fDummy) break;

        // Replace branch we came from with DUMMY
        if (cameFromTrue) {
            parent->childTrue = DUMMY;
        } else {
            parent->childFalse = DUMMY;
        }

        cur = parent;
        parent = cur->parent;
        if (parent) {
            cameFromTrue = (parent->childTrue == cur);
        }
    }

    // If we reached root and it became degenerate, we leave it as is
}

// Pretty-print pseudo-tree
void printTree(Node* node, int depth = 0, bool isRoot = false) {
    if (!node) {
        std::cout << "null\n";
        return;
    }

    if (isRoot) {
        std::cout << "Pseudo-tree:\n";
        std::cout << "x" << node->varIndex << "\n";
    }

    auto printChild = [&](const char* label, Node* child, int depth) {
        for (int i = 0; i < depth; ++i) std::cout << "  ";
        std::cout << label << " ";

        if (!child) {
            std::cout << "null\n";
        } else if (child == DUMMY) {
            std::cout << "null (dummy)\n";
        } else if (!child->childTrue && !child->childFalse) {
            std::cout << "x" << child->varIndex << " (leaf)\n";
        } else {
            std::cout << "x" << child->varIndex << "\n";
            printTree(child, depth + 1, false);
        }
    };

    printChild("F->", node->childFalse, depth + 1);
    printChild("T->", node->childTrue, depth + 1);
}

// ---------- Core insertion logic ----------

struct Literal {
    int var;       // variable index
    bool positive; // true if literal is positive, false if negated
};

// Forward declaration
void insertClauseFromPos(Node*& subtreeRoot,
                         Node* parent,
                         const std::vector<Literal>& clause,
                         size_t pos,
                         bool cameFromTrueBranch);

// Insert literal at current position (and possibly following literals)
void insertSingleLiteral(Node*& subtreeRoot,
                         Node* parent,
                         const std::vector<Literal>& clause,
                         size_t pos,
                         bool cameFromTrueBranch) {
    const Literal& lit = clause[pos];
    int insertIdx = lit.var;
    bool isLast = (pos + 1 == clause.size());

    // If subtree is empty: insert here
    if (!subtreeRoot) {
        Node* newNode = new Node(insertIdx);
        newNode->parent = parent;

        if (isLast) {
            // Last literal: children depend on sign, subtreeBelow is null here
            Node* subtreeBelow = nullptr;
            if (lit.positive) {
                newNode->childTrue  = subtreeBelow; // leaf
                newNode->childFalse = DUMMY;
            } else {
                newNode->childFalse = subtreeBelow; // leaf
                newNode->childTrue  = DUMMY;
            }
        } else {
            // Non-last: both children exist and point to same subtree (null here)
            Node* subtreeBelow = nullptr;
            newNode->childTrue  = subtreeBelow;
            newNode->childFalse = cloneSubtree(subtreeBelow, newNode);
        }

        subtreeRoot = newNode;

        // If not last, insert following literals down appropriate branch
        if (!isLast) {
            bool goDownFalse = lit.positive; // positive -> next literals go to childFalse
            Node*& nextBranch = goDownFalse ? newNode->childFalse : newNode->childTrue;
            insertClauseFromPos(nextBranch, newNode, clause, pos + 1, !goDownFalse);
        }
        return;
    }

    // If branching node: both children exist and are not DUMMY -> insert into both
    bool hasTrue = (subtreeRoot->childTrue && subtreeRoot->childTrue != DUMMY);
    bool hasFalse = (subtreeRoot->childFalse && subtreeRoot->childFalse != DUMMY);
    if (hasTrue && hasFalse) {
        insertClauseFromPos(subtreeRoot->childTrue, subtreeRoot, clause, pos, true);
        insertClauseFromPos(subtreeRoot->childFalse, subtreeRoot, clause, pos, false);
        return;
    }

    // Single-path node
    if (insertIdx == subtreeRoot->varIndex) {
        // Node with this literal already exists
        if (isLast) {
            // Last literal: check conflict rules
            if (!lit.positive) {
                // Literal is negated in clause -> check childFalse
                if (subtreeRoot->childFalse == DUMMY) {
                    // Conflict: delete subtree below childTrue and prune upwards
                    handleConflict(subtreeRoot, true);
                }
            } else {
                // Positive literal -> check childTrue (symmetrical)
                if (subtreeRoot->childTrue == DUMMY) {
                    handleConflict(subtreeRoot, false);
                }
            }
        } else {
            // Non-last literal already exists: ensure both children exist, then go down
            if (!subtreeRoot->childTrue && !subtreeRoot->childFalse) {
                Node* baseSubtree = nullptr;
                subtreeRoot->childTrue  = baseSubtree;
                subtreeRoot->childFalse = cloneSubtree(baseSubtree, subtreeRoot);
            }

            bool goDownFalse = lit.positive;
            Node*& nextBranch = goDownFalse ? subtreeRoot->childFalse : subtreeRoot->childTrue;
            insertClauseFromPos(nextBranch, subtreeRoot, clause, pos + 1, !goDownFalse);
        }
        return;
    }

    // Need to insert before or after current node, respecting sorted order
    if (insertIdx < subtreeRoot->varIndex) {
        // Insert new node before current subtreeRoot
        Node* newNode = new Node(insertIdx);
        newNode->parent = parent;

        Node* subtreeBelow = subtreeRoot;

        if (isLast) {
            if (lit.positive) {
                newNode->childTrue  = subtreeBelow;
                newNode->childFalse = DUMMY;
            } else {
                newNode->childFalse = subtreeBelow;
                newNode->childTrue  = DUMMY;
            }
            subtreeBelow->parent = newNode;
        } else {
            newNode->childTrue  = subtreeBelow;
            subtreeBelow->parent = newNode;
            newNode->childFalse = cloneSubtree(subtreeBelow, newNode);
        }

        subtreeRoot = newNode;

        if (!isLast) {
            bool goDownFalse = lit.positive;
            Node*& nextBranch = goDownFalse ? newNode->childFalse : newNode->childTrue;
            insertClauseFromPos(nextBranch, newNode, clause, pos + 1, !goDownFalse);
        }
        return;
    }

    // insertIdx > subtreeRoot->varIndex: continue search down the path
    Node*& next = cameFromTrueBranch ? subtreeRoot->childTrue : subtreeRoot->childFalse;
    insertSingleLiteral(next, subtreeRoot, clause, pos, cameFromTrueBranch);
}

// Wrapper: insert literal and all following literals starting at pos
void insertClauseFromPos(Node*& subtreeRoot,
                         Node* parent,
                         const std::vector<Literal>& clause,
                         size_t pos,
                         bool cameFromTrueBranch) {
    if (pos >= clause.size()) return;
    insertSingleLiteral(subtreeRoot, parent, clause, pos, cameFromTrueBranch);
}

// Process one clause
void processClause(const std::vector<Literal>& clause) {
    if (clause.empty()) return;
    insertClauseFromPos(root, nullptr, clause, 0, false);
}

// ---------- DIMACS CNF parsing ----------

bool parseCNF(const std::string& filename,
              int& numVars,
              int& numClauses,
              std::vector<std::vector<Literal>>& clauses) {
    std::ifstream in(filename);
    if (!in) {
        std::cerr << "Error: cannot open file " << filename << "\n";
        return false;
    }

    std::string line;
    numVars = 0;
    numClauses = 0;

    // Read header
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        if (line[0] == 'c') continue;
        if (line[0] == 'p') {
            std::stringstream ss(line);
            std::string tmp;
            ss >> tmp; // p
            ss >> tmp; // cnf
            ss >> numVars >> numClauses;
            break;
        }
    }

    // Read clauses
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == 'c') continue;
        std::stringstream ss(line);
        int lit;
        std::vector<Literal> clause;
        while (ss >> lit) {
            if (lit == 0) break;
            Literal L;
            L.positive = (lit > 0);
            L.var = std::abs(lit);
            clause.push_back(L);
        }
        if (!clause.empty()) {
            clauses.push_back(clause);
        }
    }

    return true;
}

// ---------- main ----------

int main() {
    int numVars = 0, numClauses = 0;
    std::vector<std::vector<Literal>> clauses;

    if (!parseCNF("test_cases/test_sat.cnf", numVars, numClauses, clauses)) {
        return 1;
    }

    // Initially, root is null
    root = nullptr;

    // Process each clause
    for (const auto& clause : clauses) {
        processClause(clause);
    }

    // Print resulting pseudo-tree
    if (!root) {
        std::cout << "Pseudo-tree:\nnull\n";
    } else {
        printTree(root, 0, true);
    }

    // Cleanup
    deleteSubtree(root);
    delete DUMMY;

    return 0;
}
