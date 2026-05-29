'''
Generate a large SAT formula
python cnf_generator.py --vars 5000 --clauses 20000 --sat -o big_sat.cnf

Generate a large UNSAT formula
python cnf_generator.py --vars 5000 --clauses 20000 --unsat -o big_unsat.cnf

Reproducible output
python cnf_generator.py --vars 1000 --clauses 5000 --sat --seed 123 -o sat_123.cnf

'''


#!/usr/bin/env python3
import argparse
import random
from typing import List, Set


def random_clause(n_vars: int, min_len: int = 2, max_len: int = 5) -> List[int]:
    """Generate one random clause with length between min_len and max_len."""
    k = random.randint(min_len, max_len)
    lits: Set[int] = set()
    while len(lits) < k:
        v = random.randint(1, n_vars)
        sign = random.choice([1, -1])
        lits.add(sign * v)
    return list(lits)


def generate_sat(n_vars: int, n_clauses: int) -> List[List[int]]:
    """
    Generate a SAT formula by:
    1) Creating a random satisfying assignment.
    2) Generating only clauses that are satisfied by this assignment.
    """
    model = {v: random.choice([True, False]) for v in range(1, n_vars + 1)}
    clauses = []

    # Add some unit clauses consistent with the model
    for v in range(1, min(n_vars, 10) + 1):
        lit = v if model[v] else -v
        clauses.append([lit])

    # Generate remaining clauses
    while len(clauses) < n_clauses:
        cl = random_clause(n_vars)
        # Check if the clause is satisfied by the model
        if any((lit > 0 and model[abs(lit)]) or (lit < 0 and not model[abs(lit)]) for lit in cl):
            clauses.append(cl)

    return clauses

def generate_unsat(n_vars: int, n_clauses: int) -> List[List[int]]:
    """
    Generate a non-trivial UNSAT formula with a hidden UNSAT core.
    The UNSAT core is NOT placed at the beginning; instead, it is
    inserted at random positions inside the clause list.

    Hidden UNSAT core (3 clauses):
        (x1 ∨ x2)
        (¬x1 ∨ x3)
        (¬x2 ∨ ¬x3)

    This core is UNSAT but not obvious. The solver must search.
    """
    if n_vars < 3 or n_clauses < 3:
        raise ValueError("UNSAT mode requires at least 3 variables and 3 clauses.")

    # Hidden UNSAT core
    core = [
        [1, 2],     # (x1 ∨ x2)
        [-1, 3],    # (¬x1 ∨ x3)
        [-2, -3]    # (¬x2 ∨ ¬x3)
    ]

    # Start with random clauses
    clauses = [random_clause(n_vars) for _ in range(n_clauses)]

    # Choose 3 random distinct positions to insert the core
    positions = random.sample(range(n_clauses), 3)

    # Insert core clauses into random positions
    for pos, core_clause in zip(positions, core):
        clauses[pos] = core_clause

    return clauses


def write_dimacs(n_vars: int, clauses: List[List[int]], filename: str):
    """Write clauses to a DIMACS CNF file."""
    with open(filename, "w") as f:
        # f.write("c Generated CNF formula\n")
        f.write(f"p cnf {n_vars} {len(clauses)}\n")
        for cl in clauses:
            f.write(" ".join(map(str, cl)) + " 0\n")


def main():
    parser = argparse.ArgumentParser(description="Generator of large SAT/UNSAT CNF formulas (DIMACS).")
    parser.add_argument("--vars", type=int, required=True, help="number of variables")
    parser.add_argument("--clauses", type=int, required=True, help="number of clauses")
    parser.add_argument("--sat", action="store_true", help="generate a SAT formula")
    parser.add_argument("--unsat", action="store_true", help="generate an UNSAT formula")
    parser.add_argument("-o", "--out", type=str, default="formula.cnf", help="output filename")
    parser.add_argument("--seed", type=int, default=None, help="random seed")

    args = parser.parse_args()

    if args.seed is not None:
        random.seed(args.seed)

    if args.sat == args.unsat:
        raise SystemExit("You must choose exactly one mode: --sat OR --unsat")

    if args.sat:
        clauses = generate_sat(args.vars, args.clauses)
    else:
        clauses = generate_unsat(args.vars, args.clauses)

    write_dimacs(args.vars, clauses, args.out)
    print(f"Done. Output written to {args.out}")
    print(f"Variables: {args.vars}, Clauses: {len(clauses)}")


if __name__ == "__main__":
    main()
