// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Apiwish Anutravanichkul
#pragma once
// Minimal arithmetic expression evaluator for the Cardputer calculator.
// Supports + - * / ( ) and decimals. Pure logic (host-testable).
#include <string>

namespace nexo {

bool evalExpression(const std::string& expr, double& out);

}  // namespace nexo
