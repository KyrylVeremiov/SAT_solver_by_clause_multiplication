#include <bits/stdc++.h>
using namespace std;

const string dir = "test_cases/";
const string filename = "test_sat.cnf";

struct Node {
    int var;
    Node* parent = nullptr;
    Node* childF = nullptr;
    Node* childT = nullptr;

    Node(int v) : var(v) {}
};

Node* root = nullptr;

// -------------------- Utility -----------------------------

void delete_subtree(Node* node) {
    if (!node) return;

    if (!node->parent && node == root)
        root = nullptr;

    if (node->parent) {
        if (node->parent->childF == node) node->parent->childF = nullptr;
        if (node->parent->childT == node) node->parent->childT = nullptr;
    }

    if (node->childF) {
        node->childF->parent = nullptr;
        delete_subtree(node->childF);
    }
    if (node->childT) {
        node->childT->parent = nullptr;
        delete_subtree(node->childT);
    }

    delete node;
}

Node* clone_subtree(Node* node, Node* parent) {
    if (!node) return nullptr;

    Node* copy = new Node(node->var);
    copy->parent = parent;

    copy->childF = clone_subtree(node->childF, copy);
    copy->childT = clone_subtree(node->childT, copy);

    return copy;
}

void backtrack_delete_until_split(Node* start) {
    Node* cur = start;

    while (cur) {
        Node* p = cur->parent;

        if (!p) {
            delete_subtree(cur);
            return;
        }

        bool hasTwo =
            (p->childF && p->childT) ||
            (p->childF && !p->childT) ||
            (!p->childF && p->childT);

        if (p->childF && p->childT) {
            delete_subtree(cur);
            return;
        }

        delete_subtree(cur);
        cur = p;
    }
}

// -------------------- Clause insertion -----------------------------

bool edge_value(bool positive, bool isLast) {
    if (isLast) return positive;
    return !positive;
}

void ensure_two_branches(Node* node) {
    if (!node->childF && !node->childT) return;
    if (!node->childF) node->childF = nullptr;
    if (!node->childT) node->childT = nullptr;
}

void dfs_insert_clause(Node* node,
                       Node* parent,
                       bool parentEdge,
                       const vector<int>& lits,
                       size_t pos)
{
    if (pos >= lits.size()) return;

    int lit = lits[pos];
    int v = abs(lit);
    bool positive = lit > 0;
    bool isLast = (pos + 1 == lits.size());
    bool val_here = edge_value(positive, isLast);

    // ---------------- node == nullptr ----------------
    if (!node) {
        Node* cur = new Node(v);
        cur->parent = parent;

        if (parent) {
            if (val_here == false) parent->childF = cur;
            else parent->childT = cur;

            if (val_here == false && !parent->childT) parent->childT = nullptr;
            if (val_here == true && !parent->childF) parent->childF = nullptr;
        } else {
            root = cur;
        }

        Node* current = cur;

        for (size_t i = pos + 1; i < lits.size(); ++i) {
            int lit2 = lits[i];
            int v2 = abs(lit2);
            bool pos2 = lit2 > 0;
            bool last2 = (i + 1 == lits.size());
            bool val2 = edge_value(pos2, last2);

            Node* nxt = new Node(v2);
            nxt->parent = current;

            if (val2 == false) current->childF = nxt;
            else current->childT = nxt;

            if (!last2) {
                if (val2 == false && !current->childT) current->childT = nullptr;
                if (val2 == true && !current->childF) current->childF = nullptr;
            }

            current = nxt;
        }
        return;
    }

    // ---------------- insert before node ----------------
    if (node->var > v) {
        Node* newNode = new Node(v);
        newNode->parent = parent;

        if (parent) {
            if (parentEdge == false) parent->childF = newNode;
            else parent->childT = newNode;
        } else {
            root = newNode;
        }

        if (isLast) {
            if (val_here == false) newNode->childF = node;
            else newNode->childT = node;
            node->parent = newNode;

            if (val_here == false && !newNode->childT) newNode->childT = nullptr;
            if (val_here == true && !newNode->childF) newNode->childF = nullptr;
        } else {
            Node* copy = clone_subtree(node, newNode);
            if (val_here == false) newNode->childF = copy;
            else newNode->childT = copy;

            if (val_here == false && !newNode->childT) newNode->childT = nullptr;
            if (val_here == true && !newNode->childF) newNode->childF = nullptr;
        }

        if (!isLast) {
            Node* child = (val_here == false ? newNode->childF : newNode->childT);
            dfs_insert_clause(child, newNode, val_here, lits, pos + 1);
        }
        return;
    }

    // ---------------- node->var < v ----------------
    if (node->var < v) {
        ensure_two_branches(node);

        dfs_insert_clause(node->childF, node, false, lits, pos);
        dfs_insert_clause(node->childT, node, true, lits, pos);
        return;
    }

    // ---------------- node->var == v ----------------
    ensure_two_branches(node);

    if (isLast) {
        Node*& need = (val_here == false ? node->childF : node->childT);
        Node*& other = (val_here == false ? node->childT : node->childF);

        if (need) return;

        if (other) {
            delete_subtree(other);
            other = nullptr;
            return;
        }

        backtrack_delete_until_split(node);
        return;
    }

    Node*& next = (val_here == false ? node->childF : node->childT);

    if (!next) {
        Node* base = node->childF ? node->childF : node->childT;
        Node* child = base ? clone_subtree(base, node) : nullptr;
        next = child;
    }

    dfs_insert_clause(next, node, val_here, lits, pos + 1);
}

// -------------------- Printing -----------------------------

void print_tree(Node* node, string indent = "", string edge = "") {
    if (!node) {
        cout << indent << edge << "null\n";
        return;
    }

    bool isLeaf = (!node->childF && !node->childT);

    if (isLeaf) {
        int leafValue = (edge == "T-> ") ? 1 : 0;
        cout << indent << edge << "x" << node->var
             << " (leaf=" << leafValue << ")\n";
        return;
    }

    cout << indent << edge << "x" << node->var << "\n";

    print_tree(node->childF, indent + "  ", "F-> ");
    print_tree(node->childT, indent + "  ", "T-> ");
}

// -------------------- MAIN -----------------------------

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    ifstream fin(dir + filename);
    if (!fin) {
        cerr << "Cannot open file\n";
        return 1;
    }

    string line;
    int nVars, nClauses;

    while (getline(fin, line)) {
        if (line.size() && line[0] == 'p') {
            string tmp, fmt;
            stringstream ss(line);
            ss >> tmp >> fmt >> nVars >> nClauses;
            break;
        }
    }

    root = new Node(1);

    for (int c = 0; c < nClauses; ++c) {
        getline(fin, line);
        if (line.empty()) { c--; continue; }

        stringstream ss(line);
        vector<int> lits;
        int x;
        while (ss >> x && x != 0) lits.push_back(x);

        dfs_insert_clause(root, nullptr, false, lits, 0);
    }

    cout << "Pseudo-tree:\n";
    print_tree(root);

    return 0;
}
