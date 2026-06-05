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

// Global dummy node: represents impossible branch
Node* dummyNode = new Node(-1);

struct Literal {
    int var;
    bool isNeg;
    bool isLast;
};

// -----------------------------------------------------------------------------
// Deep copy of subtree. dummyNode is shared singleton and never copied.
// -----------------------------------------------------------------------------
Node* deepCopy(Node* root, Node* parent = nullptr) {
    if (!root || root == dummyNode)
        return root; // keep nullptr or dummyNode as is

    Node* copy = new Node(root->varIndex);
    copy->parent = parent;

    copy->childTrue  = deepCopy(root->childTrue,  copy);
    copy->childFalse = deepCopy(root->childFalse, copy);

    return copy;
}

// -----------------------------------------------------------------------------
// Delete subtree safely. Never delete dummyNode.
// Используется для финальной очистки и при локальном удалении узла.
// -----------------------------------------------------------------------------
void deleteSubtree(Node* root) {
    if (!root || root == dummyNode) return;

    Node* t = root->childTrue;
    Node* f = root->childFalse;

    root->childTrue  = nullptr;
    root->childFalse = nullptr;

    if (t && t != dummyNode) {
        deleteSubtree(t);
    }
    if (f && f != dummyNode) {
        deleteSubtree(f);
    }

    delete root;
}

// -----------------------------------------------------------------------------
// Поднимаемся вверх и удаляем узлы, у которых обе ветки dummyNode.
// Останавливаемся на первом родителе, у которого обе ветки не dummyNode.
// У него ветку, по которой пришли, заменяем на dummyNode.
// Если дошли до корня — заменяем корень на dummyNode.
// -----------------------------------------------------------------------------
void pruneUp(Node* node, Node*& root) {
    while (node) {
        // Если у узла хотя бы одна ветка не dummyNode — дальше не удаляем
        if (!(node->childTrue == dummyNode && node->childFalse == dummyNode)) {
            break;
        }

        Node* parent = node->parent;

        if (parent) {
            // Заменяем ссылку у родителя на dummyNode
            if (parent->childTrue == node) {
                parent->childTrue = dummyNode;
            } else if (parent->childFalse == node) {
                parent->childFalse = dummyNode;
            }
        } else {
            // Это корень
            root = dummyNode;
        }

        // Удаляем сам узел
        deleteSubtree(node);

        // Поднимаемся выше
        node = parent;
    }
}

// -----------------------------------------------------------------------------
// Print pseudo-tree in required format
// -----------------------------------------------------------------------------
void printTree(Node* root) {
    std::cout << "Pseudo-tree:\n";
    if (!root || root == dummyNode) {
        std::cout << "null\n";
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

// -----------------------------------------------------------------------------
// Resolve conflict for last literal: удаляем конфликтующую ветку,
// помечаем её dummyNode, затем запускаем pruneUp.
// -----------------------------------------------------------------------------
void resolveConflictLastLiteral(Node* node, bool isNeg, Node*& root) {
    if (!node || node == dummyNode) return;

    if (isNeg) {
        // Конфликт по отрицательному литералу: ветка childTrue невозможна
        if (node->childTrue && node->childTrue != dummyNode) {
            deleteSubtree(node->childTrue);
        }
        node->childTrue = dummyNode;
    } else {
        // Конфликт по положительному литералу: ветка childFalse невозможна
        if (node->childFalse && node->childFalse != dummyNode) {
            deleteSubtree(node->childFalse);
        }
        node->childFalse = dummyNode;
    }

    // Если обе ветки стали dummyNode — запускаем удаление вверх
    pruneUp(node, root);
}

// -----------------------------------------------------------------------------
// Insert sequence of literals starting from given node
// -----------------------------------------------------------------------------
void insertSequenceFrom(Node*& current,
                        Node* parent,
                        const std::vector<Literal>& clause,
                        int idx,
                        Node*& root) {
    if (idx >= (int)clause.size()) return;
    if (current == dummyNode) return;

    const Literal& lit = clause[idx];
    bool isLast = lit.isLast;

    // Если текущий узел отсутствует: создаём новый
    if (!current) {
        Node* newNode = new Node(lit.var);
        newNode->parent = parent;

        if (isLast) {
            // Последний литерал: ветки по знаку
            if (lit.isNeg) {
                newNode->childFalse = nullptr;   // лист, путь возможен
                newNode->childTrue  = dummyNode; // путь невозможен
            } else {
                newNode->childTrue  = nullptr;   // лист, путь возможен
                newNode->childFalse = dummyNode; // путь невозможен
            }
        } else {
            // Непоследний: обе ветки существуют, но пока пустые
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

    // Тот же индекс переменной
    if (lit.var == current->varIndex) {
        if (isLast) {
            // Последний литерал: проверка конфликтов
            if (lit.isNeg) {
                if (current->childFalse == dummyNode) {
                    // Конфликт: отрицательный литерал, но false-ветка невозможна
                    resolveConflictLastLiteral(current, true, root);
                } else {
                    // Запрещаем true-ветку
                    if (current->childTrue && current->childTrue != dummyNode) {
                        deleteSubtree(current->childTrue);
                    }
                    current->childTrue = dummyNode;
                    // Если обе ветки dummyNode — поднимаемся вверх
                    pruneUp(current, root);
                }
            } else {
                if (current->childTrue == dummyNode) {
                    // Конфликт: положительный литерал, но true-ветка невозможна
                    resolveConflictLastLiteral(current, false, root);
                } else {
                    // Запрещаем false-ветку
                    if (current->childFalse && current->childFalse != dummyNode) {
                        deleteSubtree(current->childFalse);
                    }
                    current->childFalse = dummyNode;
                    // Если обе ветки dummyNode — поднимаемся вверх
                    pruneUp(current, root);
                }
            }
        } else {
            // Непоследний: идём по соответствующей ветке
            Node*& nextBranch = lit.isNeg ? current->childTrue : current->childFalse;
            if (nextBranch == dummyNode) return; // путь невозможен, прекращаем обработку клаузы
            insertSequenceFrom(nextBranch, current, clause, idx + 1, root);
        }
        return;
    }

    // Вставка нового узла перед текущим (lit.var < current->varIndex)
    if (lit.var < current->varIndex) {
        Node* old = current;
        Node* newNode = new Node(lit.var);
        newNode->parent = parent;

        if (isLast) {
            // Последний литерал: одна ветка указывает на старое поддерево, другая на dummy
            if (lit.isNeg) {
                newNode->childFalse = old;
                newNode->childTrue  = dummyNode;
            } else {
                newNode->childTrue  = old;
                newNode->childFalse = dummyNode;
            }
            old->parent = newNode;
        } else {
            // Непоследний: обе ветки существуют,
            // одна — оригинальное поддерево, другая — его глубокая копия
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

    // lit.var > current->varIndex → поиск дальше по дереву
    bool canTrue  = (current->childTrue  != dummyNode);
    bool canFalse = (current->childFalse != dummyNode);

    // Если обе ветки возможны, вставляем в обе
    if (canTrue && canFalse) {
        insertSequenceFrom(current->childFalse, current, clause, idx, root);
        insertSequenceFrom(current->childTrue,  current, clause, idx, root);
        return;
    }

    // Только false-ветка возможна
    if (canFalse && (!current->childTrue || current->childTrue == dummyNode)) {
        insertSequenceFrom(current->childFalse, current, clause, idx, root);
        return;
    }

    // Только true-ветка возможна
    if (canTrue && (!current->childFalse || current->childFalse == dummyNode)) {
        insertSequenceFrom(current->childTrue, current, clause, idx, root);
        return;
    }

    // Обе ветки невозможны → прекращаем
    return;
}

// -----------------------------------------------------------------------------
// Insert whole clause starting from root
// -----------------------------------------------------------------------------
void insertClause(Node*& root, const std::vector<Literal>& clause) {
    if (clause.empty()) return;
    insertSequenceFrom(root, nullptr, clause, 0, root);
}

// -----------------------------------------------------------------------------
// Parse CNF file into clauses of Literals
// -----------------------------------------------------------------------------
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

// -----------------------------------------------------------------------------
// Main
// -----------------------------------------------------------------------------
int main() {
    Node* root = nullptr;

    // std::string filename = "test_sat.cnf";
    std::string filename = "uuf50-01.cnf";
    // std::string filename    = "uf20-01.cnf";
    // std::string filename    = "uuf75-097.cnf";
    std::string folder_name = "test_cases/";

    sort_clauses(folder_name, filename);

    std::vector<std::vector<Literal>> clauses;
    if (!parseCNF(folder_name + "sorted_" + filename, clauses)) {
        return 1;
    }

    for (const auto& clause : clauses) {
        insertClause(root, clause);
        std::cout << "Inserted clause: ";
        for (const auto& lit : clause) {
            std::cout << (lit.isNeg ? "-" : "") << "x" << lit.var << " ";
        }
        std::cout << "\n";
    }

    std::cout << "Constructed pseudo-tree:\n\n";
    printTree(root);

    deleteSubtree(root);
    delete dummyNode;

    return 0;
}
