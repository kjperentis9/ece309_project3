// src/tools/calculator_eval.cpp — YOU IMPLEMENT (spec §4.6).
//
// calc_evaluate: evaluate a well-formed token list with two stacks
// (one for numbers, one for operators and '('). See spec §4.6 and the
// worked trace in Appendix C. std::stack or std::vector are both fine.

#include "tools/calculator.h"
#include <stack>

// Multiplication and division happen before addition and subtraction.
static int precedence(CalcToken::Kind operation) {
    if (operation == CalcToken::Kind::Times ||
        operation == CalcToken::Kind::Divide) {
        return 2;
    }

    if (operation == CalcToken::Kind::Plus ||
        operation == CalcToken::Kind::Minus) {
        return 1;
    }

    return 0;
}

// Apply the top operator to the last two numbers.
static bool apply_operation(std::stack<double>& values,
                            std::stack<CalcToken::Kind>& operators,
                            std::string* error) {
    if (operators.empty() || values.size() < 2) {
        *error = "Missing operator or number";
        return false;
    }

    CalcToken::Kind operation = operators.top();
    operators.pop();

    // The first number removed is the right operand.
    double right = values.top();
    values.pop();

    double left = values.top();
    values.pop();

    double result = 0.0;

    switch (operation) {
        case CalcToken::Kind::Plus:
            result = left + right;
            break;

        case CalcToken::Kind::Minus:
            result = left - right;
            break;

        case CalcToken::Kind::Times:
            result = left * right;
            break;

        case CalcToken::Kind::Divide:
            if (right == 0.0) {
                *error = "division by zero";
                return false;
            }
            result = left / right;
            break;

        default:
            *error = "Invalid operator";
            return false;
    }

    values.push(result);
    return true;
}

bool calc_evaluate(const std::vector<CalcToken>& tokens, double* out,
                   std::string* error) {
    std::stack<double> values;
    std::stack<CalcToken::Kind> operators;
    error->clear();

    for (const CalcToken& token : tokens) {
        if (token.kind == CalcToken::Kind::Number) {
            values.push(token.value);
        } else if (token.kind == CalcToken::Kind::LParen) {
            operators.push(token.kind);
        } else if (token.kind == CalcToken::Kind::RParen) {
            // Finish the operations inside these parentheses.
            while (!operators.empty() &&
                   operators.top() != CalcToken::Kind::LParen) {
                if (!apply_operation(values, operators, error)) {
                    return false;
                }
            }

            if (operators.empty()) {
                *error = "Missing opening parenthesis";
                return false;
            }

            operators.pop();
        } else {
            int current_precedence = precedence(token.kind);

            if (current_precedence == 0) {
                *error = "Invalid operator";
                return false;
            }

            // Equal precedence is evaluated from left to right.
            while (!operators.empty() &&
                   operators.top() != CalcToken::Kind::LParen &&
                   precedence(operators.top()) >= current_precedence) {
                if (!apply_operation(values, operators, error)) {
                    return false;
                }
            }

            operators.push(token.kind);
        }
    }

    // Finish any operations still waiting.
    while (!operators.empty()) {
        if (operators.top() == CalcToken::Kind::LParen) {
            *error = "Missing closing parenthesis";
            return false;
        }

        if (!apply_operation(values, operators, error)) {
            return false;
        }
    }

    if (values.size() != 1) {
        *error = "Expression did not produce one result";
        return false;
    }

    *out = values.top();
    return true;
}
