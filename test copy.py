# import dd.cudd as _bdd  # Требует компиляции CUDD, либо используйте чистый dd.bdd
from dd.autoref import BDD
# from dd.bdd import BDD
from pysat.formula import CNF
import time


def cnf_to_dnf_via_bdd(file_path):
    # 1. Reading DIMACS and quick SAT check (to avoid expensive BDD building for UNSAT)
    print(f"[*] Reading file {file_path}...")
    formula = CNF(from_file=file_path)

    # # Быстрая SAT-проверка через PySAT
    # try:
    #     from pysat.solvers import Solver
    # except Exception:
    #     Solver = None

    # if Solver is not None:
    #     print("[*] Выполняю быструю SAT-проверку...")
    #     with Solver(bootstrap_with=formula.clauses) as s:
    #         sat = s.solve()
    #     if not sat:
    #         print("[*] Формула UNSAT — прерываю, ДНФ пустая.")
    #         return
    #     else:
    #         print("[*] Формула SAT — продолжаю построение BDD.")
    # else:
    #     print("[*] PySAT Solver не доступен — пропускаю предварительную SAT-проверку.")

    # Инициализируем менеджер BDD и включаем авто-переупорядочение
    bdd = BDD()
    bdd.configure(reordering=True)

    # 2. Register all variables in the BDD manager
    print(f"[*] Registering {formula.nv} variables...")
    var_names = [f"x{i}" for i in range(1, formula.nv + 1)]
    bdd.declare(*var_names)

    # Кэшируем узлы переменных для ускорения доступа
    var_nodes = {i: bdd.var(f"x{i}") for i in range(1, formula.nv + 1)}

    # Изначально BDD равен константе True (база для перемножения КНФ)
    bdd_root = bdd.true

    print(f"[*] Variables found: {formula.nv}")
    print(f"[*] Clauses found: {len(formula.clauses)}")
    print("[*] Building BDD graph...")

    start_time = time.time()

    # Параметры оптимизации
    reorder_threshold = 10000   # порог числа узлов для вызова bdd.reorder()
    progress_interval = 20      # как часто печатать прогресс
    max_paths = 1000            # лимит извлекаемых путей (чтобы не взорвать память)

    # 3. Последовательно перемножаем все клаузы КНФ
    for idx, clause in enumerate(formula.clauses):
        # быстрое создание клаузного BDD
        clause_bdd = bdd.false

        # Если клауза содержит и x и ~x — она тавтология и равна True
        clause_set = set(clause)
        if any((-lit in clause_set) for lit in clause_set):
            clause_bdd = bdd.true
        else:
            for literal in clause:
                v = abs(literal)
                node = var_nodes[v]
                if literal < 0:
                    node = ~node
                clause_bdd = clause_bdd | node

        # Умножаем (AND) в общий BDD
        bdd_root = bdd_root & clause_bdd

        # Ранняя остановка при противоречии
        if bdd_root == bdd.false:
            print(f"[*] Contradiction found at clause {idx+1}. Stopping.")
            return

        # Печать прогресса
        if (idx + 1) % progress_interval == 0:
            print(f"    Processed clauses: {idx+1}/{len(formula.clauses)}; BDD nodes: {len(bdd)}")

        # Триггер реордеринга по росту числа узлов
        if len(bdd) > reorder_threshold:
            print(f"    Node threshold {reorder_threshold} exceeded ({len(bdd)}). Running reorder()...")
            bdd.reorder()

    elapsed = time.time() - start_time
    print(f"[*] Граф построен за {elapsed:.2f}с; узлов BDD: {len(bdd)}")

    # ===== ФИНАЛЬНАЯ ОПТИМИЗАЦИЯ =====
    print("[*] Performing garbage collection...")
    bdd.collect_garbage()
    print("[*] Final variable reorder...")
    bdd.reorder()
    
    # 4. Проверка результата
    if bdd_root == bdd.false:
        print("\n[DNF Result]: Formula is UNSAT (identically false).")
        print("DNF contains no satisfying assignments (empty expression).")
        return

    # 5. Если формула выполнима, извлекаем ДНФ (пути к терминальной единице)
    print("\n[DNF Result]: Formula is satisfiable (SAT). Extracting paths...")
    print(f"[*] Extracting up to {max_paths} paths for DNF...")

    readable_dnf = []
    count = 0
    for cube in bdd.pick_iter(bdd_root):
        conj = " & ".join([f"{k}" if v else f"~{k}" for k, v in cube.items()])
        readable_dnf.append(f"({conj})")
        count += 1
        if count >= max_paths:
            print(f"[*] Reached path limit ({max_paths}), stopping extraction.")
            break

    final_dnf_string = " | \n".join(readable_dnf)
    print("Final DNF (truncated):")
    print(final_dnf_string)

# Запуск скрипта
if __name__ == "__main__":
    cnf_to_dnf_via_bdd("big_unsat.cnf")
    cnf_to_dnf_via_bdd("small.cnf")
