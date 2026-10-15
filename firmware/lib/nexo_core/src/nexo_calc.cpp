// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Apiwish Anutravanichkul
#include "nexo_calc.h"

#include <cctype>
#include <cmath>

namespace nexo {

namespace {
struct Parser {
    const char* p;
    bool ok = true;

    void skip() { while (*p == ' ' || *p == '\t') ++p; }

    double parseExpr();
    double parseTerm();
    double parseFactor();
};

double Parser::parseFactor()
{
    skip();
    bool neg = false;
    if (*p == '-') {
        neg = true;
        ++p;
    } else if (*p == '+') {
        ++p;
    }
    skip();
    double v = 0;
    if (*p == '(') {
        ++p;
        v = parseExpr();
        skip();
        if (*p == ')') {
            ++p;
        } else {
            ok = false;
            return 0;
        }
    } else if (std::isdigit(static_cast<unsigned char>(*p)) || *p == '.') {
        const char* start = p;
        while (std::isdigit(static_cast<unsigned char>(*p)) || *p == '.') ++p;
        std::string num(start, p - start);
        try {
            v = std::stod(num);
        } catch (...) {
            ok = false;
            return 0;
        }
    } else {
        ok = false;
        return 0;
    }
    return neg ? -v : v;
}

double Parser::parseTerm()
{
    double v = parseFactor();
    for (;;) {
        skip();
        if (*p == '*') {
            ++p;
            v *= parseFactor();
        } else if (*p == '/') {
            ++p;
            double d = parseFactor();
            if (d == 0) {
                ok = false;
                return 0;
            }
            v /= d;
        } else {
            break;
        }
    }
    return v;
}

double Parser::parseExpr()
{
    double v = parseTerm();
    for (;;) {
        skip();
        if (*p == '+') {
            ++p;
            v += parseTerm();
        } else if (*p == '-') {
            ++p;
            v -= parseTerm();
        } else {
            break;
        }
    }
    return v;
}
}  // namespace

bool evalExpression(const std::string& expr, double& out)
{
    Parser parser{expr.c_str()};
    double v = parser.parseExpr();
    parser.skip();
    if (!parser.ok || *parser.p != '\0') return false;
    out = v;
    return true;
}

}  // namespace nexo
