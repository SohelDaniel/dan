#include "flow.hpp"
#include "qp.hpp"
#include <algorithm>
#include <climits>
#include <numeric>

namespace place {
namespace {

const float kLaneTop = 38, kLaneBottom = 14; // a group's box: 14 px around its members, a 24 px name on top
const float kLanePad = kLaneTop + kLaneBottom + 16; // between lanes: both boxes and a margin
const float kDummyH = 12;    // the room a link takes where it passes a column

// the working graph: the things, then one stand-in ("dummy") per column a long link passes through
struct Node { Vec size; int lane = -1, rank = 0; bool dummy = false; float key = 0; };
struct Work {
  std::vector<Node> node;
  std::vector<std::vector<int>> pred, succ; // along the flow, between neighbouring columns
  std::vector<std::vector<int>> chain;      // per edge: the nodes it passes, in flow order
  std::vector<std::vector<int>> layer;      // per column, top to bottom
};

// 1. cycles: inside a cycle, a link to something declared earlier runs against the flow (backprop's
// gradients, a reply), as ELK's model-order cycle breaking does; links outside cycles keep their way.
// What's left is acyclic: inside a cycle every kept link goes to a later thing.
std::vector<bool> breakCycles(int n, const std::vector<FlowEdge> &edges) {
  std::vector<std::vector<int>> out(n);
  for (auto &e : edges) out[e.a].push_back(e.b);
  auto reaches = [&](int from, int to) {
    std::vector<bool> seen(n, false);
    std::vector<int> stack{from};
    while (!stack.empty()) {
      int v = stack.back();
      stack.pop_back();
      if (v == to) return true;
      if (seen[v]) continue;
      seen[v] = true;
      for (int w : out[v]) stack.push_back(w);
    }
    return false;
  };
  std::vector<bool> back(edges.size(), false);
  for (size_t e = 0; e < edges.size(); e++) back[e] = edges[e].a > edges[e].b && reaches(edges[e].b, edges[e].a);
  return back;
}

// 2. columns: the longest path from the sources; a source sits just before what it feeds. Things
// without links stand in columns by category in the order the categories appeared (inputs, hidden,
// outputs), joining a linked thing's column when they share its category.
std::vector<int> assignRanks(const FlowInput &in, const std::vector<std::pair<int, int>> &flow) {
  int n = (int)in.id.size();
  std::vector<std::vector<int>> succ(n), pred(n);
  for (auto [u, v] : flow) { succ[u].push_back(v); pred[v].push_back(u); }
  std::vector<int> rank(n, 0), indeg(n), topo;
  for (int v = 0; v < n; v++) if (!(indeg[v] = (int)pred[v].size())) topo.push_back(v);
  for (size_t i = 0; i < topo.size(); i++)
    for (int w : succ[topo[i]]) {
      rank[w] = std::max(rank[w], rank[topo[i]] + 1);
      if (--indeg[w] == 0) topo.push_back(w);
    }
  for (int v = 0; v < n; v++)
    if (pred[v].empty() && !succ[v].empty()) {
      int r = INT_MAX;
      for (int w : succ[v]) r = std::min(r, rank[w]);
      rank[v] = r - 1;
    }
  int maxRank = 0, cats = 0;
  for (int v = 0; v < n; v++) {
    cats = std::max(cats, in.category[v] + 1);
    if (!succ[v].empty() || !pred[v].empty()) maxRank = std::max(maxRank, rank[v]);
  }
  for (int v = 0; v < n; v++) {
    if (!succ[v].empty() || !pred[v].empty()) continue;
    int r = -1;
    for (int u = 0; u < n && r < 0; u++)
      if ((!succ[u].empty() || !pred[u].empty()) && in.category[u] == in.category[v]) r = rank[u];
    if (r < 0) r = flow.empty() ? (cats > 1 ? in.category[v] : v % std::min(n, 6)) : std::min(in.category[v], maxRank + 1);
    rank[v] = r;
  }
  return rank;
}

// 3. a link that skips columns gets a stand-in in every column it passes
Work buildWork(const FlowInput &in, const std::vector<int> &rank, const std::vector<bool> &back) {
  Work w;
  for (size_t i = 0; i < in.id.size(); i++) w.node.push_back({in.size[i], in.lane[i], rank[i]});
  w.pred.resize(w.node.size());
  w.succ.resize(w.node.size());
  auto join = [&](int u, int v) { w.succ[u].push_back(v); w.pred[v].push_back(u); };
  for (size_t e = 0; e < in.edges.size(); e++) {
    auto [a, b, lw] = in.edges[e];
    if (a == b || back[e]) { w.chain.push_back({}); continue; } // a loop or an arc: no columns to pass
    std::vector<int> c{a};
    int lane = in.lane[a] == in.lane[b] ? in.lane[a] : -1;
    for (int r = rank[a] + 1; r < rank[b]; r++) {
      w.node.push_back({{0, kDummyH}, lane, r, true});
      w.pred.emplace_back();
      w.succ.emplace_back();
      c.push_back((int)w.node.size() - 1);
    }
    c.push_back(b);
    for (size_t k = 1; k < c.size(); k++) join(c[k - 1], c[k]);
    w.chain.push_back(c);
  }
  return w;
}

// 4. order inside each column: what stays in its column keeps its order from last time; a newcomer is
// inserted where what feeds it is (the barycentre of its predecessors' positions); anything else comes
// after, in declaration order. A lane is one unit, so its members stay together, and lanes keep their
// declaration order in every column. A node's key is its position in its column.
void orderRanks(Work &w, const FlowInput &in, const FlowMemo &memo) {
  int R = 0;
  for (auto &nd : w.node) R = std::max(R, nd.rank + 1);
  w.layer.assign(R, {});
  for (int i = 0; i < (int)w.node.size(); i++) w.layer[w.node[i].rank].push_back(i);
  for (auto &L : w.layer) {
    auto bary = [&](int v) {
      if (w.pred[v].empty()) return -1.f;
      float s = 0;
      for (int u : w.pred[v]) s += w.node[u].key;
      return s / w.pred[v].size();
    };
    std::vector<int> seq, fresh;
    std::map<int, float> was; // kept: last time's height
    for (int v : L) {
      auto m = v < (int)in.id.size() ? memo.slot.find(in.id[v]) : memo.slot.end();
      if (m != memo.slot.end() && m->second.first == w.node[v].rank) { was[v] = m->second.second; seq.push_back(v); }
      else fresh.push_back(v);
    }
    std::stable_sort(seq.begin(), seq.end(), [&](int a, int b) { return was[a] < was[b]; });
    std::stable_sort(fresh.begin(), fresh.end(), [&](int a, int b) {
      float ba = bary(a), bb = bary(b);
      return ba >= 0 && (bb < 0 || ba < bb);
    });
    for (int f : fresh) {
      float bf = bary(f);
      auto at = bf < 0 ? seq.end() : std::find_if(seq.begin(), seq.end(), [&](int k) { return bary(k) > bf; });
      seq.insert(at, f);
    }
    for (size_t i = 0; i < seq.size(); i++) w.node[seq[i]].key = (float)i;

    struct Unit { int lane; float key = 0; std::vector<int> members; };
    std::vector<Unit> units;
    for (int v : seq) {
      int ln = w.node[v].lane;
      auto it = std::find_if(units.begin(), units.end(), [&](const Unit &u) { return ln >= 0 && u.lane == ln; });
      if (it == units.end()) { units.push_back({ln}); it = units.end() - 1; }
      it->members.push_back(v);
    }
    for (auto &u : units) {
      for (int v : u.members) u.key += w.node[v].key;
      u.key /= u.members.size();
    }
    std::stable_sort(units.begin(), units.end(), [](const Unit &a, const Unit &b) { return a.key < b.key; });
    std::vector<int> slots;
    std::vector<Unit> lanes;
    for (int i = 0; i < (int)units.size(); i++) if (units[i].lane >= 0) { slots.push_back(i); lanes.push_back(units[i]); }
    std::stable_sort(lanes.begin(), lanes.end(), [](const Unit &a, const Unit &b) { return a.lane < b.lane; });
    for (size_t k = 0; k < slots.size(); k++) units[slots[k]] = lanes[k];
    L.clear();
    for (auto &u : units) L.insert(L.end(), u.members.begin(), u.members.end());
    for (size_t i = 0; i < L.size(); i++) w.node[L[i]].key = (float)i;
  }
}

// 5. columns across: each gap is as wide as the widest label of a link leaving into it, then the
// slack is shared out (up to a comfortable width) and the drawing is centred
std::vector<float> columnX(const Work &w, const FlowInput &in, bool &overflow) {
  int R = (int)w.layer.size();
  std::vector<float> colW(R, 0), gap(std::max(0, R - 1), 70);
  for (auto &nd : w.node) colW[nd.rank] = std::max(colW[nd.rank], nd.size.x);
  for (size_t e = 0; e < in.edges.size(); e++)
    if (w.chain[e].size() > 1) {
      int r = w.node[w.chain[e][0]].rank;
      gap[r] = std::max(gap[r], in.edges[e].labelW + 40);
    }
  float total = std::accumulate(colW.begin(), colW.end(), 0.f) + std::accumulate(gap.begin(), gap.end(), 0.f);
  if (R > 1 && total < in.area.w) {
    float per = (in.area.w - total) / (R - 1);
    for (float &g : gap) g = std::max(g, std::min(g + per, 240.f));
  } else if (total > in.area.w) {
    float give = 0;
    for (float g : gap) give += std::max(0.f, g - 30);
    float f = give > 0 ? std::min(1.f, (total - in.area.w) / give) : 0;
    for (float &g : gap) g -= std::max(0.f, g - 30) * f;
  }
  total = std::accumulate(colW.begin(), colW.end(), 0.f) + std::accumulate(gap.begin(), gap.end(), 0.f);
  overflow = total > in.area.w + 0.5f;
  std::vector<float> x(R);
  float left = in.area.x + (in.area.w - total) / 2;
  for (int r = 0; r < R; r++) {
    x[r] = left + colW[r] / 2;
    left += colW[r] + (r < R - 1 ? gap[r] : 0);
  }
  return x;
}

// 6. heights: each thing goes level with its neighbours (and where it was), kept apart within its
// column and outside other lanes' bands, by least squares under separation constraints
std::vector<float> assignY(const Work &w, const FlowInput &in, const FlowMemo &memo, bool &overflow) {
  int n = (int)w.node.size();
  float colH = 0, cy = in.area.y + in.area.h / 2;
  size_t most = 1;
  for (auto &L : w.layer) {
    float h = 0;
    for (int v : L) h += w.node[v].size.y;
    colH = std::max(colH, h);
    most = std::max(most, L.size());
  }
  auto h = [&](int v) { return w.node[v].size.y; };
  std::vector<int> lo, hi; // each lane's span of columns
  for (auto &nd : w.node)
    if (nd.lane >= 0) {
      if ((int)lo.size() <= nd.lane) { lo.resize(nd.lane + 1, INT_MAX); hi.resize(nd.lane + 1, -1); }
      lo[nd.lane] = std::min(lo[nd.lane], nd.rank);
      hi[nd.lane] = std::max(hi[nd.lane], nd.rank);
    }
  std::vector<float> laneKey(lo.size(), 0), laneN(lo.size(), 0);
  for (auto &nd : w.node) if (nd.lane >= 0) { laneKey[nd.lane] += nd.key; laneN[nd.lane]++; }
  for (size_t l = 0; l < lo.size(); l++) if (laneN[l]) laneKey[l] /= laneN[l];
  std::vector<bool> first(n, false), last(n, false); // a lane's top and bottom thing in each column
  for (auto &L : w.layer)
    for (size_t i = 0; i < L.size(); i++) {
      int ln = w.node[L[i]].lane;
      if (ln < 0) continue;
      first[L[i]] = i == 0 || w.node[L[i - 1]].lane != ln;
      last[L[i]] = i + 1 == L.size() || w.node[L[i + 1]].lane != ln;
    }
  auto constraints = [&](float yGap) {
    std::vector<Sep> cons;
    auto apart = [&](int a, int b, float extra) { cons.push_back({a, b, (h(a) + h(b)) / 2 + extra}); };
    for (auto &L : w.layer)
      for (size_t i = 1; i < L.size(); i++) {
        int a = L[i - 1], b = L[i];
        bool lanes = w.node[a].lane != w.node[b].lane && (w.node[a].lane >= 0 || w.node[b].lane >= 0);
        bool thin = w.node[a].dummy || w.node[b].dummy;
        apart(a, b, (thin ? yGap / 2 : yGap) + (lanes ? kLanePad : 0));
      }
    // a lane's band is its own: lanes stack in declaration order, and a thing outside a lane stays
    // above or below the whole lane where the lane spans its column. Only each lane's top and bottom
    // thing per column need constraints; the column order keeps the rest inside.
    for (int a = 0; a < n; a++)
      for (int b = 0; b < n; b++) {
        int la = w.node[a].lane, lb = w.node[b].lane;
        if (la >= 0 && lb > la && last[a] && first[b] && lo[la] <= hi[lb] && lo[lb] <= hi[la]) apart(a, b, yGap + kLanePad);
        if (la < 0 && lb >= 0 && (first[b] || last[b]) && w.node[a].rank >= lo[lb] && w.node[a].rank <= hi[lb]) {
          const auto &L = w.layer[w.node[a].rank];
          auto pa = std::find(L.begin(), L.end(), a) - L.begin();
          auto pb = std::find_if(L.begin(), L.end(), [&](int v) { return w.node[v].lane == lb; }) - L.begin();
          bool above = pb < (long)L.size() ? pa < pb : w.node[a].key < laneKey[lb];
          if (above && first[b]) apart(a, b, yGap + kLanePad);
          if (!above && last[b]) apart(b, a, yGap + kLanePad);
        }
      }
    return cons;
  };
  auto solve = [&](float yGap) {
    std::vector<double> y(n), anchor(n), weight(n);
    for (auto &L : w.layer) { // a first guess: each column stacked about the middle
      float t = 0;
      for (int v : L) t += h(v) + yGap;
      float at = cy - (t - yGap) / 2;
      for (int v : L) { y[v] = at + h(v) / 2; at += h(v) + yGap; }
    }
    for (int v = 0; v < n; v++) {
      anchor[v] = y[v];
      weight[v] = w.node[v].dummy ? 4 : 1;
      if (v < (int)in.id.size())
        if (auto m = memo.slot.find(in.id[v]); m != memo.slot.end() && m->second.first == w.node[v].rank) anchor[v] = y[v] = m->second.second;
    }
    auto cons = constraints(yGap);
    for (int pass = 0; pass < 4; pass++) {
      std::vector<double> want(n);
      for (int v = 0; v < n; v++) {
        double s = 0;
        int k = 0;
        if (pass != 1) for (int u : w.pred[v]) { s += y[u]; k++; }
        if (pass != 0) for (int u : w.succ[v]) { s += y[u]; k++; }
        want[v] = k ? 0.7 * s / k + 0.3 * anchor[v] : anchor[v];
      }
      y = solve1d(want, weight, cons);
    }
    return std::vector<float>(y.begin(), y.end());
  };
  float yGap = std::clamp((in.area.h - colH) / most, 24.f, 90.f);
  auto extent = [&](const std::vector<float> &y, float &top, float &bottom) {
    top = 1e9f; bottom = -1e9f;
    for (int v = 0; v < n; v++) { // a lane's box and name count too
      bool lane = w.node[v].lane >= 0;
      top = std::min(top, y[v] - h(v) / 2 - (lane ? kLaneTop : 0));
      bottom = std::max(bottom, y[v] + h(v) / 2 + (lane ? kLaneBottom : 0));
    }
  };
  std::vector<float> y = solve(yGap);
  float top, bottom;
  extent(y, top, bottom);
  if (bottom - top > in.area.h) { y = solve(10); extent(y, top, bottom); }
  overflow = bottom - top > in.area.h + 0.5f;
  float shift = cy - (top + bottom) / 2;
  for (float &v : y) v += shift;
  return y;
}

// 7. links: through their stand-ins' columns, and spread along the sides of the things they leave
// and enter, in the order of where they go (a fan, never one line on top of another)
void routeEdges(const Work &w, const FlowInput &in, const std::vector<float> &x, const std::vector<float> &y,
                const std::vector<bool> &back, FlowOut &out) {
  std::vector<float> colW(x.size(), 0);
  for (auto &nd : w.node) colW[nd.rank] = std::max(colW[nd.rank], nd.size.x);
  out.route.assign(in.edges.size(), {});
  for (size_t e = 0; e < in.edges.size(); e++) {
    out.route[e].back = back[e];
    const auto &c = w.chain[e];
    for (size_t k = 1; k + 1 < c.size(); k++) {
      int r = w.node[c[k]].rank;
      out.route[e].via.push_back({x[r] - colW[r] / 2, y[c[k]]});
      out.route[e].via.push_back({x[r] + colW[r] / 2, y[c[k]]});
    }
  }
  auto spread = [&](int v, std::vector<std::pair<float, int>> &ends, bool source) {
    std::sort(ends.begin(), ends.end());
    int m = (int)ends.size();
    float step = m > 1 ? std::min(12.f, std::max(0.f, w.node[v].size.y - 16) / (m - 1)) : 0;
    for (int k = 0; k < m; k++) (source ? out.route[ends[k].second].portA : out.route[ends[k].second].portB) = (k - (m - 1) / 2.f) * step;
  };
  for (int v = 0; v < (int)in.id.size(); v++) {
    std::vector<std::pair<float, int>> outs, ins;
    for (size_t e = 0; e < in.edges.size(); e++) {
      if (w.chain[e].size() < 2) continue;
      const auto &via = out.route[e].via;
      if (in.edges[e].a == v) outs.push_back({via.empty() ? y[in.edges[e].b] : via.front().y, (int)e});
      if (in.edges[e].b == v) ins.push_back({via.empty() ? y[in.edges[e].a] : via.back().y, (int)e});
    }
    spread(v, outs, true);
    spread(v, ins, false);
  }
}

} // namespace

FlowOut layoutFlow(const FlowInput &in, FlowMemo &memo) {
  FlowOut out;
  int n = (int)in.id.size();
  if (n == 0) return out;
  std::vector<bool> back = breakCycles(n, in.edges);
  std::vector<std::pair<int, int>> flow;
  for (size_t e = 0; e < in.edges.size(); e++) {
    auto [a, b, lw] = in.edges[e];
    if (a != b) flow.push_back(back[e] ? std::pair{b, a} : std::pair{a, b});
  }
  std::vector<int> rank = assignRanks(in, flow);
  Work w = buildWork(in, rank, back);
  orderRanks(w, in, memo);
  bool wide = false, tall = false;
  std::vector<float> x = columnX(w, in, wide);
  std::vector<float> y = assignY(w, in, memo, tall);
  routeEdges(w, in, x, y, back, out);
  out.overflow = wide || tall;
  for (int v = 0; v < n; v++) {
    out.centre.push_back({x[rank[v]], y[v]});
    out.rank.push_back(rank[v]);
    memo.slot[in.id[v]] = {rank[v], y[v]};
  }
  return out;
}

std::vector<Vec> smoothRoute(const std::vector<Vec> &p, int perSeg) {
  if (p.size() < 2) return p;
  std::vector<Vec> out{p[0]};
  for (size_t i = 1; i < p.size(); i++) {
    Vec a = p[i - 1], b = p[i], c1{(a.x + b.x) / 2, a.y}, c2{(a.x + b.x) / 2, b.y};
    for (int k = 1; k <= perSeg; k++) {
      float t = (float)k / perSeg, u = 1 - t;
      out.push_back(a * (u * u * u) + c1 * (3 * u * u * t) + c2 * (3 * u * t * t) + b * (t * t * t));
    }
  }
  return out;
}

} // namespace place
