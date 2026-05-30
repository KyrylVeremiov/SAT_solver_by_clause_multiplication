#include <bits/stdc++.h>
using namespace std;

struct Node {
    int var;
    Node* parent;
    Node* childF;
    Node* childT;

    Node(int v) : var(v), parent(nullptr), childF(nullptr), childT(nullptr) {}
};

Node* root = nullptr;

struct Result {
    Node* node;
    bool stop_here;
};

long long CALL_ID = 0;

// ------------------------------------------------------------
// Clone subtree
// ------------------------------------------------------------
Node* clone_subtree(Node* node, Node* parent) {
    if (!node) return nullptr;
    Node* c = new Node(node->var);
    c->parent = parent;
    c->childF = clone_subtree(node->childF, c);
    c->childT = clone_subtree(node->childT, c);
    return c;
}

// ------------------------------------------------------------
// Delete subtree
// ------------------------------------------------------------
void delete_subtree(Node* node) {
    if (!node) return;
    delete_subtree(node->childF);
    delete_subtree(node->childT);
    delete node;
}

// ------------------------------------------------------------
// Branch value for literal
// ------------------------------------------------------------
bool branch_value(bool positive, bool isLast) {
    if (isLast) return positive;
    return !positive;
}

// ------------------------------------------------------------
// Create new node M
// ------------------------------------------------------------
Node* create_node_M(int M, bool val_here, bool isLast, Node* parent) {
    Node* n = new Node(M);
    n->parent = parent;

    n->childF = nullptr;
    n->childT = nullptr;

    return n;
}

// ------------------------------------------------------------
// Insert M between parent and node
// ------------------------------------------------------------
Node* insert_between(Node* parent, Node* node, int M, bool val_here, bool isLast) {
    Node* mid = new Node(M);
    mid->parent = parent;

    if (parent) {
        if (parent->childF == node) parent->childF = mid;
        if (parent->childT == node) parent->childT = mid;
    } else {
        root = mid;
    }

    if (isLast) {
        if (!val_here) mid->childF = node;
        else mid->childT = node;
        node->parent = mid;
    } else {
        if (!val_here) {
            mid->childF = node;
            mid->childT = clone_subtree(node, mid);
        } else {
            mid->childT = node;
            mid->childF = clone_subtree(node, mid);
        }
        node->parent = mid;
    }

    return mid;
}

// ------------------------------------------------------------
// Backtracking
// ------------------------------------------------------------
void backtrack(Node* node) {
    while (node) {
        Node* p = node->parent;
        if (!p) {
            delete_subtree(node);
            root = nullptr;
            return;
        }

        if (p->childF && p->childT) {
            if (p->childF == node) p->childF = nullptr;
            if (p->childT == node) p->childT = nullptr;
            delete_subtree(node);
            return;
        }

        if (p->childF == node) p->childF = nullptr;
        if (p->childT == node) p->childT = nullptr;

        delete_subtree(node);
        node = p;
    }
}

// ------------------------------------------------------------
// Insert literal xM
// ------------------------------------------------------------
Result insert_literal(Node* node, Node* parent,
                      int M, bool val_here, bool isLast) {

    CALL_ID++;

    static int depth = 0;
    depth++;

    if (depth > 2000) {
        cerr << "!!! RECURSION LIMIT REACHED !!!\n";
        cerr << "node=" << (node ? node->var : -1)
             << " M=" << M
             << " val=" << val_here
             << " last=" << isLast << "\n";
        exit(1);
    }

    cerr << "[CALL " << CALL_ID << " | depth " << depth << "] "
         << "insert_literal(node=" << (node ? node->var : -1)
         << ", parent=" << (parent ? parent->var : -1)
         << ", M=" << M
         << ", val=" << val_here
         << ", last=" << isLast << ")\n";

    // Case: empty
    if (!node) {
        Node* n = create_node_M(M, val_here, isLast, parent);
        cerr << "  -> created node M=" << M << "\n";
        depth--;
        return {n, isLast};
    }

    int K = node->var;

    // Case: K == M
    if (K == M) {
        if (!isLast) {
            cerr << "  -> existing M, intermediate, stop_here\n";
            depth--;
            return {node, true};
        }

        Node*& need = (val_here ? node->childT : node->childF);
        Node*& other = (val_here ? node->childF : node->childT);

        if (need) {
            if (other) {
                delete_subtree(other);
                other = nullptr;
            }
            cerr << "  -> last literal, need exists, stop_here\n";
            depth--;
            return {node, true};
        }

        if (!need && !other) {
            cerr << "  -> last literal, both empty, backtrack\n";
            backtrack(node);
            depth--;
            return {node, true};
        }

        if (other) {
            delete_subtree(other);
            other = nullptr;
        }

        need = nullptr;
        cerr << "  -> last literal, created need branch\n";
        depth--;
        return {node, true};
    }

    // Case: K < M
    if (K < M) {
        Node*& child = (val_here ? node->childT : node->childF);

        if (!child) {
            child = create_node_M(M, val_here, isLast, node);
            cerr << "  -> K<M, created child M\n";
            depth--;
            return {node, false};
        }

        if (child->var == M) {
            cerr << "  -> K<M, child==M, stop_here\n";
            depth--;
            return {node, true};
        }

        if (child->var > M || child->var < M) {
            child = insert_between(node, child, M, val_here, isLast);
            cerr << "  -> K<M, inserted between\n";
            depth--;
            return {node, false};
        }
    }

    // Case: K > M
    Node* mid = insert_between(parent, node, M, val_here, isLast);
    cerr << "  -> K>M, inserted between parent and node\n";
    depth--;
    return {mid, false};
}

// ------------------------------------------------------------
// Insert clause
// ------------------------------------------------------------
void insert_clause(const vector<int>& lits) {
    if (!root) root = new Node(1);

    Node* node = root;
    Node* parent = nullptr;

    for (int i = 0; i < (int)lits.size(); i++) {
        int L = lits[i];
        int M = abs(L);
        bool positive = (L > 0);
        bool isLast = (i + 1 == (int)lits.size());
        bool val_here = branch_value(positive, isLast);

        cerr << "\n=== INSERT LITERAL " << L << " (M=" << M << ") ===\n";

        Result r = insert_literal(node, parent, M, val_here, isLast);
        node = r.node;

        if (!r.stop_here) {
            Node*& next = (val_here ? node->childT : node->childF);
            parent = node;
            node = next;
            cerr << "  -> descending to child: "
                 << (node ? node->var : -1) << "\n";
        } else {
            parent = node;
            cerr << "  -> stop_here, staying at node " << node->var << "\n";
        }
    }
}

// ------------------------------------------------------------
// Print tree
// ------------------------------------------------------------
void print_tree(Node* node, string indent = "", string edge = "") {
    if (!node) {
        cout << indent << edge << "null\n";
        return;
    }

    cout << indent << edge << "x" << node->var << "\n";

    print_tree(node->childF, indent + "  ", "F-> ");
    print_tree(node->childT, indent + "  ", "T-> ");
}

// ------------------------------------------------------------
// MAIN
// ------------------------------------------------------------
int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    string line;
    int nVars, nClauses;

    getline(cin, line);
    {
        string tmp, fmt;
        stringstream ss(line);
        ss >> tmp >> fmt >> nVars >> nClauses;
    }

    root = new Node(1);

    for (int c = 0; c < nClauses; c++) {
        getline(cin, line);
        stringstream ss2(line);
        vector<int> lits;
        int x;
        while (ss2 >> x && x != 0) lits.push_back(x);
        insert_clause(lits);
    }

    cout << "\nPseudo-tree:\n";
    print_tree(root);
}
