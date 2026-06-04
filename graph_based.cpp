#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>

using namespace std;

// ========================= NODE STRUCT =========================

struct Node {
    int varIndex;       // literal index
    bool isDummy;       // true for dummy node
    Node* parent;
    Node* childTrue;
    Node* childFalse;

    Node(int idx = 0, bool dummy = false)
        : varIndex(idx), isDummy(dummy),
          parent(nullptr), childTrue(nullptr), childFalse(nullptr) {}
};

Node* dummyNode = new Node(0, true);
Node* root = nullptr;

// ========================= TREE UTILITIES =========================

Node* deepCopy(Node* node, Node* parent = nullptr) {
    if (!node) return nullptr;
    if (node->isDummy) return dummyNode;

    Node* c = new Node(node->varIndex, false);
    c->parent = parent;
    c->childTrue = deepCopy(node->childTrue, c);
    c->childFalse = deepCopy(node->childFalse, c);
    return c;
}

void deleteSubtree(Node* node) {
    if (!node || node->isDummy) return;
    deleteSubtree(node->childTrue);
    deleteSubtree(node->childFalse);
    delete node;
}

// ========================= PRINTING =========================

void printNode(Node* node, const string& prefix) {
    if (!node) {
        cout << prefix << "null\n";
        return;
    }
    if (node->isDummy) {
        cout << prefix << "dummy\n";
        return;
    }

    cout << prefix << "x" << node->varIndex << "\n";

    cout << prefix << "  F-> ";
    if (!node->childFalse) {
        cout << "null\n";
    } else if (node->childFalse->isDummy) {
        cout << "dummy\n";
    } else {
        cout << "x" << node->childFalse->varIndex << "\n";
        printNode(node->childFalse, prefix + "    ");
    }

    cout << prefix << "  T-> ";
    if (!node->childTrue) {
        cout << "null\n";
    } else if (node->childTrue->isDummy) {
        cout << "dummy\n";
    } else {
        cout << "x" << node->childTrue->varIndex << "\n";
        printNode(node->childTrue, prefix + "    ");
    }
}

void printTree(Node* root) {
    cout << "Pseudo-tree:\n";
    if (!root) {
        cout << "null\n";
        return;
    }
    printNode(root, "");
}

// ========================= INSERTION LOGIC =========================

void insertLiteral(Node*& subtreeRoot,
                   int litIndex,
                   bool isPositive,
                   bool isLastLiteral,
                   const vector<int>& clause,
                   const vector<bool>& clauseSign,
                   size_t posInClause);

void insertLastLiteral(Node*& subtreeRoot, int litIndex, bool isPositive) {
    Node* n = new Node(litIndex, false);
    n->parent = nullptr;

    if (isPositive) {
        n->childTrue = nullptr;    // path possible
        n->childFalse = dummyNode; // impossible
    } else {
        n->childFalse = nullptr;
        n->childTrue = dummyNode;
    }

    subtreeRoot = n;
}

void insertNonLastLiteral(Node*& subtreeRoot,
                          int litIndex,
                          bool isPositive,
                          const vector<int>& clause,
                          const vector<bool>& clauseSign,
                          size_t posInClause) {
    Node* n = new Node(litIndex, false);
    n->parent = nullptr;

    // both branches exist; subtree below is currently null
    n->childTrue = nullptr;
    n->childFalse = nullptr;

    subtreeRoot = n;

    size_t nextPos = posInClause + 1;
    if (nextPos < clause.size()) {
        int nextLit = abs(clause[nextPos]);
        bool nextSign = clause[nextPos] > 0;
        bool nextIsLast = (nextPos == clause.size() - 1);

        // for positive literal: next literals go down childFalse
        // for negative literal: next literals go down childTrue
        if (isPositive) {
            insertLiteral(n->childFalse, nextLit, nextSign,
                          nextIsLast, clause, clauseSign, nextPos);
        } else {
            insertLiteral(n->childTrue, nextLit, nextSign,
                          nextIsLast, clause, clauseSign, nextPos);
        }
    }
}

void insertLiteral(Node*& subtreeRoot,
                   int litIndex,
                   bool isPositive,
                   bool isLastLiteral,
                   const vector<int>& clause,
                   const vector<bool>& clauseSign,
                   size_t posInClause) {
    // impossible path
    if (subtreeRoot && subtreeRoot->isDummy) return;

    // null -> insert here
    if (!subtreeRoot) {
        if (isLastLiteral)
            insertLastLiteral(subtreeRoot, litIndex, isPositive);
        else
            insertNonLastLiteral(subtreeRoot, litIndex, isPositive,
                                 clause, clauseSign, posInClause);
        return;
    }

    // insert before current node
    if (subtreeRoot->varIndex > litIndex) {
        Node* n = new Node(litIndex, false);
        n->parent = subtreeRoot->parent;

        if (isLastLiteral) {
            if (isPositive) {
                n->childTrue = subtreeRoot;
                n->childFalse = dummyNode;
            } else {
                n->childFalse = subtreeRoot;
                n->childTrue = dummyNode;
            }
            subtreeRoot->parent = n;
        } else {
            // non-last: both branches exist
            // childTrue -> original subtree
            n->childTrue = subtreeRoot;
            subtreeRoot->parent = n;
            // childFalse -> deep copy of subtree
            n->childFalse = deepCopy(subtreeRoot, n);

            size_t nextPos = posInClause + 1;
            if (nextPos < clause.size()) {
                int nextLit = abs(clause[nextPos]);
                bool nextSign = clause[nextPos] > 0;
                bool nextIsLast = (nextPos == clause.size() - 1);

                if (isPositive) {
                    insertLiteral(n->childFalse, nextLit, nextSign,
                                  nextIsLast, clause, clauseSign, nextPos);
                } else {
                    insertLiteral(n->childTrue, nextLit, nextSign,
                                  nextIsLast, clause, clauseSign, nextPos);
                }
            }
        }

        subtreeRoot = n;
        return;
    }

    // go down
    if (subtreeRoot->varIndex < litIndex) {
        bool hasBoth =
            subtreeRoot->childTrue &&
            !subtreeRoot->childTrue->isDummy &&
            subtreeRoot->childFalse &&
            !subtreeRoot->childFalse->isDummy;

        // branching: insert into both
        if (hasBoth) {
            insertLiteral(subtreeRoot->childTrue,
                          litIndex, isPositive, isLastLiteral,
                          clause, clauseSign, posInClause);
            insertLiteral(subtreeRoot->childFalse,
                          litIndex, isPositive, isLastLiteral,
                          clause, clauseSign, posInClause);
            return;
        } else {
            // go down existing non-dummy branches
            if (subtreeRoot->childTrue &&
                !subtreeRoot->childTrue->isDummy) {
                insertLiteral(subtreeRoot->childTrue,
                              litIndex, isPositive, isLastLiteral,
                              clause, clauseSign, posInClause);
            }
            if (subtreeRoot->childFalse &&
                !subtreeRoot->childFalse->isDummy) {
                insertLiteral(subtreeRoot->childFalse,
                              litIndex, isPositive, isLastLiteral,
                              clause, clauseSign, posInClause);
            }
            return;
        }
    }

    // same index
    if (subtreeRoot->varIndex == litIndex) {

        // last literal: conflict handling only for negative
        if (isLastLiteral) {
            if (!isPositive) {
                if (subtreeRoot->childFalse == dummyNode) {
                    // conflict: delete subtree below childTrue
                    deleteSubtree(subtreeRoot->childTrue);
                    subtreeRoot->childTrue = dummyNode;

                    // delete upwards until node with both children != dummy
                    Node* cur = subtreeRoot->parent;
                    Node* child = subtreeRoot;

                    while (cur) {
                        bool bothNotDummy =
                            (cur->childTrue != dummyNode) &&
                            (cur->childFalse != dummyNode);

                        if (bothNotDummy) {
                            if (cur->childTrue == child)
                                cur->childTrue = dummyNode;
                            else
                                cur->childFalse = dummyNode;
                            break;
                        } else {
                            if (cur->childTrue == child) {
                                deleteSubtree(cur->childTrue);
                                cur->childTrue = dummyNode;
                            } else {
                                deleteSubtree(cur->childFalse);
                                cur->childFalse = dummyNode;
                            }
                            child = cur;
                            cur = cur->parent;
                        }
                    }
                }
            }
            return;
        }

        // non-last literal: node exists -> continue with next literal
        size_t nextPos = posInClause + 1;
        if (nextPos < clause.size()) {
            int nextLit = abs(clause[nextPos]);
            bool nextSign = clause[nextPos] > 0;
            bool nextIsLast = (nextPos == clause.size() - 1);

            // for positive: next literals go down childFalse
            // for negative: next literals go down childTrue
            if (isPositive) {
                insertLiteral(subtreeRoot->childFalse,
                              nextLit, nextSign,
                              nextIsLast, clause, clauseSign, nextPos);
            } else {
                insertLiteral(subtreeRoot->childTrue,
                              nextLit, nextSign,
                              nextIsLast, clause, clauseSign, nextPos);
            }
        }
        return;
    }
}

// ========================= CNF PARSER =========================

bool readCNF(const string& filename,
             int& numVars,
             int& numClauses,
             vector<vector<int>>& clauses) {
    ifstream in(filename);
    if (!in.is_open()) {
        cerr << "Error: cannot open file " << filename << endl;
        return false;
    }

    string line;
    while (getline(in, line)) {
        if (line.empty()) continue;
        if (line[0] == 'c') continue;
        if (line[0] == 'p') {
            string tmp, fmt;
            stringstream ss(line);
            ss >> tmp >> fmt >> numVars >> numClauses;
            continue;
        }

        stringstream ss(line);
        int lit;
        vector<int> clause;
        while (ss >> lit) {
            if (lit == 0) break;
            clause.push_back(lit);
        }
        if (!clause.empty())
            clauses.push_back(clause);
    }
    return true;
}

// ========================= MAIN =========================

int main() {
    int numVars = 0, numClauses = 0;
    vector<vector<int>> clauses;

    if (!readCNF("test_cases/test_sat.cnf", numVars, numClauses, clauses))
        return 1;

    for (const auto& clause : clauses) {
        vector<bool> clauseSign(clause.size());
        for (size_t i = 0; i < clause.size(); ++i)
            clauseSign[i] = clause[i] > 0;

        for (size_t i = 0; i < clause.size(); ++i) {
            int litIndex = abs(clause[i]);
            bool isPositive = clause[i] > 0;
            bool isLast = (i == clause.size() - 1);

            insertLiteral(root, litIndex, isPositive,
                          isLast, clause, clauseSign, i);
        }
    }

    printTree(root);

    deleteSubtree(root);
    delete dummyNode;
    return 0;
}
