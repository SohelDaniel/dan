// Tiny arithmetic expression language used inside {braces} in the markup.
//   {1 + h}   {f(1+h)}   {mean(score)}   {sin(x)*2}   {alice.balance - bob.balance}
#pragma once
#include <memory>
#include <string>
#include <vector>

namespace mk {

struct Expr;
using ExprPtr = std::shared_ptr<Expr>;

struct Expr {
  enum Kind { NUM, VAR, MEMBER, CALL, BIN, NEG } kind = NUM;
  double num = 0;
  std::string name, member; // VAR: name; MEMBER: name.member; CALL: name(args)
  char op = 0;              // BIN: + - * / ^
  std::vector<ExprPtr> args;
};

// The engine resolves names; return false from a hook to fall back to built-ins.
struct EvalCtx {
  double x = 0;
  bool hasX = false;
  const char *xName = "x";  // "t" while sampling a parametric path
  virtual ~EvalCtx() = default;
  virtual bool var(const std::string &name, double &out) = 0;
  virtual bool member(const std::string &id, const std::string &prop, double &out) = 0;
  virtual bool call(const std::string &name, const std::vector<ExprPtr> &args, double &out, int depth) = 0;
};

ExprPtr parseExpr(const std::string &src, std::string &err);
double eval(const Expr &e, EvalCtx &ctx, int depth = 0);
// every identifier the expression mentions (ids of marks it depends on)
void exprNames(const Expr &e, std::vector<std::string> &out);

} // namespace mk
