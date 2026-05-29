from sympy.logic.utilities import dimacs
from sympy.logic.boolalg import to_dnf
from numba import jit


# Строка в формате DIMACS
dimacs_string = """
c This is an example comment
p cnf 3 2
1 -2 0
-3 2 0
"""

# expr = dimacs.load(dimacs_string)
# expr = dimacs.load_file("random.cnf")
expr = dimacs.load_file("big_unsat.cnf")

@jit(nopython=False,parallel=True, forceobj=True,fastmath=True,cache=True)
def process_expression(expr1):
    dnf_expr = to_dnf(expr, simplify=True,force=True)
    print(dnf_expr)
    # return to_dnf(expr, simplify=True,force=True)


dnf_expr = process_expression(expr)
# print(expr)
print(dnf_expr)