import random

# Size parameters
n_vars = 5000      # number of variables
n_clauses = 20000  # number of clauses

filename = "big_unsat.cnf"

with open(filename, "w") as f:
    # f.write("c Large unsatisfiable CNF formula\n")
    # f.write("c Contradiction core: (x1) and (¬x1)\n")
    f.write(f"p cnf {n_vars} {n_clauses}\n")

    # 1) Explicit contradiction — ensures UNSAT
    f.write("1 0\n")
    f.write("-1 0\n")

    # 2) Remaining clauses are "noise" that does not remove the contradiction
    for _ in range(n_clauses - 2):
        k = random.randint(2, 5)  # clause length 2–5 literals
        clause = set()
        while len(clause) < k:
            v = random.randint(1, n_vars)
            sign = random.choice([1, -1])
            clause.add(sign * v)
        f.write(" ".join(map(str, clause)) + " 0\n")

print(f"Done: file {filename} created.")
