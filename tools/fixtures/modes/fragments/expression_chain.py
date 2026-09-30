"""Rows for the 0.99 right-to-left expression-chain fixture."""

EXPRESSION_CHAIN_ACCOUNT = "ExpressionChainProbe"
EXPRESSION_CHAIN_PASSWORD = "expression-chain-pw"
EXPRESSION_CHAIN_MARKER = "SPHERE_EXPR_CHAIN"

# The values are the calibrated 0.99 grammar: no precedence and a chain
# associated from the right.  Keep this table in one place so the script and
# checker cannot silently disagree.
EXPRESSION_CHAIN_ORACLE_ROWS = (
    ("subtraction", "10-5-2", "7"),
    ("bitwise", "1|2&3", "3"),
    ("comparison_chain", "1<2<3", "0"),
    ("multiply_add", "2*3+4", "14"),
    ("subtract_compare", "5-3<1", "5"),
    ("negative_add", "-3+5", "2"),
    ("add_multiply", "2+3*4", "14"),
    ("parenthesized", "(10-5)-2", "3"),
    ("subtract_multiply", "10-2*3", "4"),
    ("zero_subtract_compare", "0<0-1", "0"),
    ("equal_subtract", "3==3-0", "1"),
    ("divide_subtract", "20/5-1", "5"),
    ("add_compare", "1+2<4", "2"),
    ("not_equal_add", "4!=2+2", "0"),
    ("divide_chain", "8/2/2", "8"),
    ("add_multiply_subtract", "2+3*4-1", "11"),
    ("zero_and_or", "0&&0||1", "0"),
    ("mod_add", "7%4+1", "2"),
)

EXPRESSION_CHAIN_QUIRK_EXPECTED = {
    # The space form is a one-token legacy call; parentheses keep the whole
    # expression together.
    "eval_space": "10",
    "eval_parenthesized_space": "7",
    "leading_zero_hex": "16",
    "arg_compare_raw": "3==3-0",
    "arg_arithmetic_value": "3",
    "arg_eval_value": "7",
    "arg_zero_literal": "00",
    "if_zero_subtract_compare": "0",
    "while_empty": "00",
    "while_three": "2",
    "argvcount_empty": "0",
    "argvcount_parenthesized": "3",
    "argvcount_space": "0",
}


def expression_chain_scripts() -> tuple[str, str]:
    """Return login lines and function/resource sections for the probe."""

    marker = EXPRESSION_CHAIN_MARKER
    login = ["F_EXPR_CHAIN_ORACLE", "F_EXPR_CHAIN_PATHS", f"SYSMESSAGE {marker}_END"]

    oracle_lines = []
    function_sections = []
    for key, expression, _expected in EXPRESSION_CHAIN_ORACLE_ROWS:
        function_name = f"f_expr_chain_{key}"
        oracle_lines.append(f"SYSMESSAGE {marker} {key}|[<{function_name}>]")
        function_sections.append(f"[FUNCTION {function_name}]\nRETURN {expression}\n")

    # The two EVAL forms, ARG(<eval>), and the IF/WHILE paths exercise the
    # callers that do not enter through a function RETURN expression.
    paths = f"""
[FUNCTION F_EXPR_CHAIN_ORACLE]
{chr(10).join(oracle_lines)}
SYSMESSAGE {marker} eval_space|[<EVAL 10 - 5 - 2>]
SYSMESSAGE {marker} eval_parenthesized_space|[<EVAL(10 - 5 - 2)>]
SYSMESSAGE {marker} leading_zero_hex|[<EVAL 010>]

[FUNCTION F_EXPR_CHAIN_PATHS]
ARG(eval_value,<EVAL(10-5-2)>)
SYSMESSAGE {marker} arg_eval_value|[<ARG.eval_value>]
ARG(zero_literal,0)
SYSMESSAGE {marker} arg_zero_literal|[<ARG.zero_literal>]
ARG(raw_compare,3==3-0)
ARG(arithmetic,1+2)
SYSMESSAGE {marker} arg_compare_raw|[<ARG.raw_compare>]
SYSMESSAGE {marker} arg_arithmetic_value|[<ARG.arithmetic>]
IF (0<0-1)
SYSMESSAGE {marker} if_zero_subtract_compare|[1]
ELSE
SYSMESSAGE {marker} if_zero_subtract_compare|[0]
ENDIF
F_EXPR_CHAIN_WHILE_EMPTY
F_EXPR_CHAIN_WHILE_THREE(1,2,3)
F_EXPR_CHAIN_ARGCOUNT_EMPTY
F_EXPR_CHAIN_ARGCOUNT_PAREN(1,2,3)
F_EXPR_CHAIN_ARGCOUNT_SPACE 1,2,3
"""
    function_sections.append(paths)
    function_sections.extend(
        (
            f"""[FUNCTION F_EXPR_CHAIN_WHILE_EMPTY]
ARG(i,0)
WHILE (i < ARGVCOUNT-1)
ARG(i,#+1)
ENDWHILE
SYSMESSAGE {marker} while_empty|[<ARG.i>]
""",
            f"""[FUNCTION F_EXPR_CHAIN_WHILE_THREE]
ARG(i,0)
WHILE (i < ARGVCOUNT-1)
ARG(i,#+1)
ENDWHILE
SYSMESSAGE {marker} while_three|[<ARG.i>]
""",
            f"""[FUNCTION F_EXPR_CHAIN_ARGCOUNT_EMPTY]
SYSMESSAGE {marker} argvcount_empty|[<ARGVCOUNT>]
""",
            f"""[FUNCTION F_EXPR_CHAIN_ARGCOUNT_PAREN]
SYSMESSAGE {marker} argvcount_parenthesized|[<ARGVCOUNT>]
""",
            f"""[FUNCTION F_EXPR_CHAIN_ARGCOUNT_SPACE]
SYSMESSAGE {marker} argvcount_space|[<ARGVCOUNT>]
""",
        )
    )
    return "\n".join(login) + "\n", "\n".join(function_sections)
