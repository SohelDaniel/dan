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
    if (ctx.hasY && e.name == ctx.yName) return ctx.y;
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

bool exprUses(const Expr &e, const std::string &name) {
  if (e.kind == Expr::VAR && e.name == name) return true;
  for (auto &a : e.args) if (exprUses(*a, name)) return true;
  return false;
}

// ---------------------------------------------------------------- compiled programs
namespace {
enum Fn1 : unsigned char { SIN, COS, TAN, ASIN, ACOS, ATAN, EXP, LOG, LOG10, SQRT, ABS, FLOOR, CEIL, ROUND, SIGN };
enum Fn2 : unsigned char { POW2, ATAN2 };
int fn1Of(const std::string &n) {
  static const char *names[] = {"sin", "cos", "tan", "asin", "acos", "atan", "exp", "log", "log10", "sqrt", "abs", "floor", "ceil", "round", "sign"};
  for (int i = 0; i < 15; i++) if (n == names[i]) return i;
  if (n == "ln") return LOG;
  return -1;
}

struct Compiler {
  EvalCtx &ctx;
  Program &p;
  bool fail = false;
  bool free(const Expr &e) { return exprUses(e, ctx.xName) || exprUses(e, ctx.yName); }
  void emit(const Expr &e) {
    if (!free(e)) { // fold: evaluate once with x and y hidden
      bool hx = ctx.hasX, hy = ctx.hasY;
      ctx.hasX = ctx.hasY = false;
      p.ops.push_back({Program::K, 0, 0, eval(e, ctx)});
      ctx.hasX = hx; ctx.hasY = hy;
      return;
    }
    switch (e.kind) {
    case Expr::NUM: p.ops.push_back({Program::K, 0, 0, e.num}); return;
    case Expr::VAR:
      if (e.name == ctx.xName) { p.ops.push_back({Program::X}); p.usesX = true; }
      else { p.ops.push_back({Program::Y}); p.usesY = true; }
      return;
    case Expr::NEG: emit(*e.args[0]); p.ops.push_back({Program::NEG}); return;
    case Expr::BIN: {
      emit(*e.args[0]);
      emit(*e.args[1]);
      Program::Code c = e.op == '+' ? Program::ADD : e.op == '-' ? Program::SUB : e.op == '*' ? Program::MUL
                       : e.op == '/' ? Program::DIV : Program::POW;
      p.ops.push_back({c});
      return;
    }
    case Expr::CALL: {
      int f = fn1Of(e.name);
      size_t n = e.args.size();
      if (f >= 0 && n == 1) { emit(*e.args[0]); p.ops.push_back({Program::FN1, (unsigned char)f}); return; }
      if ((e.name == "pow" || e.name == "atan2") && n == 2) {
        emit(*e.args[0]); emit(*e.args[1]);
        p.ops.push_back({Program::FN2, (unsigned char)(e.name == "pow" ? POW2 : ATAN2)});
        return;
      }
      if ((e.name == "min" || e.name == "max") && n >= 1 && n < 255) {
        for (auto &a : e.args) emit(*a);
        p.ops.push_back({e.name == "min" ? Program::FNN_MIN : Program::FNN_MAX, 0, (unsigned char)n});
        return;
      }
      if (e.name == "clamp" && n == 3) {
        for (auto &a : e.args) emit(*a);
        p.ops.push_back({Program::CLAMP});
        return;
      }
      fail = true; // an engine function of x or y (a path call, deriv ...)
      return;
    }
    default: fail = true; return; // a member can't depend on x or y
    }
  }
};
} // namespace

Program compile(const Expr &e, EvalCtx &ctx) {
  Program p;
  Compiler c{ctx, p};
  c.emit(e);
  p.ok = !c.fail;
  return p;
}

double Program::run(double x, double y) const {
  double st[64];
  int sp = 0;
  for (const Op &o : ops) {
    switch (o.code) {
    case K: st[sp++] = o.k; break;
    case X: st[sp++] = x; break;
    case Y: st[sp++] = y; break;
    case ADD: sp--; st[sp - 1] += st[sp]; break;
    case SUB: sp--; st[sp - 1] -= st[sp]; break;
    case MUL: sp--; st[sp - 1] *= st[sp]; break;
    case DIV: sp--; st[sp - 1] /= st[sp]; break;
    case POW: sp--; st[sp - 1] = std::pow(st[sp - 1], st[sp]); break;
    case NEG: st[sp - 1] = -st[sp - 1]; break;
    case FN1: {
      double &a = st[sp - 1];
      switch (o.fn) {
      case SIN: a = std::sin(a); break;
      case COS: a = std::cos(a); break;
      case TAN: a = std::tan(a); break;
      case ASIN: a = std::asin(a); break;
      case ACOS: a = std::acos(a); break;
      case ATAN: a = std::atan(a); break;
      case EXP: a = std::exp(a); break;
      case LOG: a = std::log(a); break;
      case LOG10: a = std::log10(a); break;
      case SQRT: a = std::sqrt(a); break;
      case ABS: a = std::fabs(a); break;
      case FLOOR: a = std::floor(a); break;
      case CEIL: a = std::ceil(a); break;
      case ROUND: a = std::round(a); break;
      default: a = (a > 0) - (a < 0); break;
      }
      break;
    }
    case FN2: sp--; st[sp - 1] = o.fn == POW2 ? std::pow(st[sp - 1], st[sp]) : std::atan2(st[sp - 1], st[sp]); break;
    case FNN_MIN: case FNN_MAX: {
      double r = st[sp - o.n];
      for (int i = sp - o.n + 1; i < sp; i++) r = o.code == FNN_MIN ? std::fmin(r, st[i]) : std::fmax(r, st[i]);
      sp -= o.n;
      st[sp++] = r;
      break;
    }
    case CLAMP: sp -= 2; st[sp - 1] = std::fmin(std::fmax(st[sp - 1], st[sp]), st[sp + 1]); break;
    }
    if (sp >= 62) return NaN; // too deep for the fixed stack: the caller can fall back to eval
  }
  return sp == 1 ? st[0] : NaN;
}

} // namespace mk
