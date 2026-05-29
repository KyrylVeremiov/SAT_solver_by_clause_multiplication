from sympy.logic.utilities import dimacs
from sympy.logic.boolalg import to_dnf


# Строка в формате DIMACS (пример)
dimacs_string = """
c This is an example comment
+p cnf 3 2
+1 -2 0
+-3 2 0
+"""

# Загрузка формулы из файла DIMACS (по умолчанию используем big_unsat.cnf)
# expr = dimacs.load(dimacs_string)
# expr = dimacs.load_file("random.cnf")
expr = dimacs.load_file("big_unsat.cnf")

def process_expression(expr1):
    # Используем переданный аргумент (не внешнюю переменную)
    dnf_expr = to_dnf(expr1, simplify=True)
    print(dnf_expr)
    return dnf_expr


dnf_expr = process_expression(expr)
print(dnf_expr)
