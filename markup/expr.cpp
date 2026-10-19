#include "expr.hpp"
#include <cctype>
#include <cmath>
#include <limits>

namespace mk {

namespace {

struct Parser {
  const std::string &s;
  size_t i = 0;
  std::string err;

  void ws() { while (i < s.size() && std::isspace((unsigned char)s[i])) i++; }
  bool eat(char c) {
    ws();
    if (i < s.size() && s[i] == c) { i++; return true; }
    return false;
  }
  ExprPtr bin(char op, ExprPtr a, ExprPtr b) {
    auto e = std::make_shared<Expr>();
    e->kind = Expr::BIN; e->op = op; e->args = {a, b};
    return e;
  }
  ExprPtr expr() { // + -
    ExprPtr a = term();
    for (;;) {
      if (eat('+')) a = bin('+', a, term());
      else if (eat('-')) a = bin('-', a, term());
      else return a;
    }
  }
  ExprPtr term() { // * /  and implicit multiplication like 2x
    ExprPtr a = unary();
    for (;;) {
      if (eat('*')) a = bin('*', a, unary());
      else if (eat('/')) a = bin('/', a, unary());
      else {
        ws();
        if (i < s.size() && (std::isalpha((unsigned char)s[i]) || s[i] == '(')) a = bin('*', a, unary());
        else return a;
      }
    }
  }
  ExprPtr unary() {
    if (eat('-')) {
      auto e = std::make_shared<Expr>();
      e->kind = Expr::NEG; e->args = {unary()};
      return e;
    }
    if (eat('+')) return unary();
    return power();
  }
  ExprPtr power() {
    ExprPtr a = atom();
    if (eat('^')) return bin('^', a, unary()); // right assoc
    return a;
  }
  ExprPtr atom() {
    ws();
    if (i >= s.size()) { err = "unexpected end"; return num(0); }
    char c = s[i];
    if (c == '(') {
      i++;
      ExprPtr e = expr();
      if (!eat(')')) err = "missing )";
      return e;
    }
    if (std::isdigit((unsigned char)c) || c == '.') {
      size_t j = i;
      while (j < s.size() && (std::isdigit((unsigned char)s[j]) || s[j] == '.')) j++;
      if (j < s.size() && (s[j] == 'e' || s[j] == 'E') && j + 1 < s.size() &&
          (std::isdigit((unsigned char)s[j + 1]) || s[j + 1] == '-')) {
        j += 2;
        while (j < s.size() && std::isdigit((unsigned char)s[j])) j++;
      }
      double v = std::atof(s.substr(i, j - i).c_str());
      i = j;
      return num(v);
    }
    if (std::isalpha((unsigned char)c) || c == '_') {
      size_t j = i;
      while (j < s.size() && (std::isalnum((unsigned char)s[j]) || s[j] == '_')) j++;
      std::string name = s.substr(i, j - i);
      i = j;
      auto e = std::make_shared<Expr>();
      e->name = name;
      if (i < s.size() && s[i] == '.' && i + 1 < s.size() && std::isalpha((unsigned char)s[i + 1])) {
        i++;
        size_t k = i;
        while (k < s.size() && (std::isalnum((unsigned char)s[k]) || s[k] == '_')) k++;
        e->kind = Expr::MEMBER;
        e->member = s.substr(i, k - i);
        i = k;
        return e;
      }
      ws();
      if (i < s.size() && s[i] == '(') {
        i++;
        e->kind = Expr::CALL;
        if (!eat(')')) {
          do e->args.push_back(expr()); while (eat(','));
          if (!eat(')')) err = "missing ) after arguments of " + name;
        }
        return e;
      }
      e->kind = Expr::VAR;
      return e;
    }
    err = std::string("unexpected '") + c + "'";
    i++;
    return num(0);
  }
  ExprPtr num(double v) {
    auto e = std::make_shared<Expr>();
    e->kind = Expr::NUM; e->num = v;
    return e;
  }
};

const double NaN = std::numeric_limits<double>::quiet_NaN();

bool builtin(const std::string &n, const std::vector<double> &a, double &out) {
  auto one = [&](double (*f)(double)) { if (a.size() != 1) return false; out = f(a[0]); return true; };
  if (n == "sin") return one(std::sin);
  if (n == "cos") return one(std::cos);
  if (n == "tan") return one(std::tan);
  if (n == "asin") return one(std::asin);
  if (n == "acos") return one(std::acos);
  if (n == "atan") return one(std::atan);
  if (n == "exp") return one(std::exp);
  if (n == "log" || n == "ln") return one(std::log);
  if (n == "log10") return one(std::log10);
  if (n == "sqrt") return one(std::sqrt);
  if (n == "abs") return one(std::fabs);
  if (n == "floor") return one(std::floor);
  if (n == "ceil") return one(std::ceil);
  if (n == "round") return one(std::round);
  if (n == "sign" && a.size() == 1) { out = (a[0] > 0) - (a[0] < 0); return true; }
  if (n == "pow" && a.size() == 2) { out = std::pow(a[0], a[1]); return true; }
  if (n == "atan2" && a.size() == 2) { out = std::atan2(a[0], a[1]); return true; }
  if ((n == "min" || n == "max") && !a.empty()) {
    out = a[0];
    for (double v : a) out = n == "min" ? std::fmin(out, v) : std::fmax(out, v);
    return true;
  }
  if (n == "clamp" && a.size() == 3) { out = std::fmin(std::fmax(a[0], a[1]), a[2]); return true; }
  return false;
}

} // namespace

ExprPtr parseExpr(const std::string &src, std::string &err) {
  Parser p{src};
  ExprPtr e = p.expr();
  p.ws();
  if (p.err.empty() && p.i < src.size()) p.err = "trailing text '" + src.substr(p.i) + "'";
  err = p.err;
  return e;
}

double eval(const Expr &e, EvalCtx &ctx, int depth) {
  if (depth > 64) return NaN; // cyclic reference
  switch (e.kind) {
  case Expr::NUM: return e.num;
  case Expr::NEG: return -eval(*e.args[0], ctx, depth + 1);
  case Expr::BIN: {
    double a = eval(*e.args[0], ctx, depth + 1), b = eval(*e.args[1], ctx, depth + 1);
    switch (e.op) {
    case '+': return a + b;
    case '-': return a - b;
    case '*': return a * b;
    case '/': return a / b;
    case '^': return std::pow(a, b);
    }
    return NaN;
  }
  case Expr::VAR: {
    if (ctx.hasX && e.name == ctx.xName) return ctx.x;
    if (e.name == "pi") return M_PI;
    if (e.name == "tau") return 2 * M_PI;
    if (e.name == "e") return M_E;
    double v;
    if (ctx.var(e.name, v)) return v;
    return NaN;
  }
  case Expr::MEMBER: {
    double v;
    if (ctx.member(e.name, e.member, v)) return v;
    return NaN;
  }
  case Expr::CALL: {
    double v;
    if (ctx.call(e.name, e.args, v, depth + 1)) return v;
    std::vector<double> a;
    for (auto &x : e.args) a.push_back(eval(*x, ctx, depth + 1));
    if (builtin(e.name, a, v)) return v;
    return NaN;
  }
  }
  return NaN;
}

void exprNames(const Expr &e, std::vector<std::string> &out) {
  if (e.kind == Expr::VAR || e.kind == Expr::MEMBER || e.kind == Expr::CALL) out.push_back(e.name);
  for (auto &a : e.args) exprNames(*a, out);
}

} // namespace mk
