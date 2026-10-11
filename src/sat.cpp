// sat.cpp — small CDCL SAT solver; see sat.hpp.
#include "sat.hpp"

#include <algorithm>
#include <cmath>

int SatSolver::new_var()
{
    int v = static_cast<int>(assigns_.size());
    assigns_.push_back(2);
    level_.push_back(0);
    reason_.push_back(-1);
    phase_.push_back(0);
    activity_.push_back(0.0);
    heap_pos_.push_back(-1);
    seen_.push_back(0);
    watches_.emplace_back();
    watches_.emplace_back();
    heap_insert(v);
    return v;
}

// ---------------------------------------------------------------------------
// Variable-order heap (max-heap on activity)
// ---------------------------------------------------------------------------
void SatSolver::heap_up(int i)
{
    int v = heap_[i];
    while (i > 0) {
        int parent = (i - 1) >> 1;
        if (activity_[heap_[parent]] >= activity_[v]) break;
        heap_[i] = heap_[parent];
        heap_pos_[heap_[i]] = i;
        i = parent;
    }
    heap_[i] = v;
    heap_pos_[v] = i;
}

void SatSolver::heap_down(int i)
{
    int v = heap_[i];
    const int n = static_cast<int>(heap_.size());
    while (true) {
        int child = 2 * i + 1;
        if (child >= n) break;
        if (child + 1 < n && activity_[heap_[child + 1]] > activity_[heap_[child]]) ++child;
        if (activity_[heap_[child]] <= activity_[v]) break;
        heap_[i] = heap_[child];
        heap_pos_[heap_[i]] = i;
        i = child;
    }
    heap_[i] = v;
    heap_pos_[v] = i;
}

void SatSolver::heap_insert(int v)
{
    if (heap_pos_[v] >= 0) return;
    heap_.push_back(v);
    heap_pos_[v] = static_cast<int>(heap_.size()) - 1;
    heap_up(heap_pos_[v]);
}

int SatSolver::heap_pop()
{
    int top = heap_[0];
    int last = heap_.back();
    heap_.pop_back();
    heap_pos_[top] = -1;
    if (!heap_.empty()) {
        heap_[0] = last;
        heap_pos_[last] = 0;
        heap_down(0);
    }
    return top;
}

void SatSolver::bump_var(int v)
{
    activity_[v] += var_inc_;
    if (activity_[v] > 1e100) {
        for (double& a : activity_) a *= 1e-100;
        var_inc_ *= 1e-100;
    }
    if (heap_pos_[v] >= 0) heap_up(heap_pos_[v]);
}

void SatSolver::bump_clause(Clause& c)
{
    c.act += cla_inc_;
    if (c.act > 1e20f) {
        for (Clause& d : clauses_)
            if (d.learnt) d.act *= 1e-20f;
        cla_inc_ *= 1e-20f;
    }
}

// ---------------------------------------------------------------------------
// Clauses and propagation
// ---------------------------------------------------------------------------
void SatSolver::attach(int ci)
{
    const Clause& c = clauses_[ci];
    watches_[c.lits[0]].push_back({ci, c.lits[1]});
    watches_[c.lits[1]].push_back({ci, c.lits[0]});
}

void SatSolver::enqueue(int lit, int reason)
{
    int v = lit_var(lit);
    assigns_[v] = lit_neg(lit) ? 0 : 1;
    level_[v] = level();
    reason_[v] = reason;
    trail_.push_back(lit);
}

bool SatSolver::add_clause(std::vector<int> lits)
{
    if (!ok_) return false;
    std::sort(lits.begin(), lits.end());
    std::vector<int> out;
    for (size_t i = 0; i < lits.size(); ++i) {
        int l = lits[i];
        if (i > 0 && l == lits[i - 1]) continue;       // duplicate
        if (i > 0 && l == lit_not(lits[i - 1])) return true;  // tautology (sorted: l, l^1 adjacent)
        int val = value(l);
        if (val == 1) return true;                      // already satisfied at level 0
        if (val == 0) continue;                         // false at level 0: drop
        out.push_back(l);
    }
    if (out.empty()) {
        ok_ = false;
        return false;
    }
    if (out.size() == 1) {
        enqueue(out[0], -1);
        ok_ = propagate() == -1;
        return ok_;
    }
    Clause c;
    c.lits = std::move(out);
    clauses_.push_back(std::move(c));
    attach(static_cast<int>(clauses_.size()) - 1);
    return true;
}

int SatSolver::propagate()
{
    int confl = -1;
    while (qhead_ < trail_.size()) {
        int p = trail_[qhead_++];
        int fl = lit_not(p);  // literal that just became false
        std::vector<Watch>& ws = watches_[fl];
        size_t i = 0, j = 0;
        while (i < ws.size()) {
            ++work_;
            Watch w = ws[i];
            if (value(w.blocker) == 1) {
                ws[j++] = ws[i++];
                continue;
            }
            Clause& c = clauses_[w.clause];
            if (c.lits[0] == fl) std::swap(c.lits[0], c.lits[1]);
            ++i;
            int first = c.lits[0];
            Watch nw{w.clause, first};
            if (first != w.blocker && value(first) == 1) {
                ws[j++] = nw;
                continue;
            }
            bool found = false;
            for (size_t k = 2; k < c.lits.size(); ++k) {
                if (value(c.lits[k]) != 0) {
                    c.lits[1] = c.lits[k];
                    c.lits[k] = fl;
                    watches_[c.lits[1]].push_back(nw);
                    found = true;
                    break;
                }
            }
            if (found) continue;
            ws[j++] = nw;
            if (value(first) == 0) {
                confl = w.clause;
                qhead_ = trail_.size();
                while (i < ws.size()) ws[j++] = ws[i++];
            } else {
                enqueue(first, w.clause);
            }
        }
        ws.resize(j);
        if (confl >= 0) break;
    }
    return confl;
}

// ---------------------------------------------------------------------------
// Conflict analysis (first UIP) with recursive minimization
// ---------------------------------------------------------------------------
bool SatSolver::lit_redundant(int p, uint32_t abstract_levels)
{
    analyze_stack_.clear();
    analyze_stack_.push_back(p);
    size_t top = analyze_toclear_.size();
    while (!analyze_stack_.empty()) {
        int q = analyze_stack_.back();
        analyze_stack_.pop_back();
        const Clause& c = clauses_[reason_[lit_var(q)]];
        for (size_t i = 1; i < c.lits.size(); ++i) {
            int l = c.lits[i];
            int v = lit_var(l);
            if (!seen_[v] && level_[v] > 0) {
                if (reason_[v] != -1 && ((1u << (level_[v] & 31)) & abstract_levels)) {
                    seen_[v] = 1;
                    analyze_stack_.push_back(l);
                    analyze_toclear_.push_back(l);
                } else {
                    for (size_t j = top; j < analyze_toclear_.size(); ++j)
                        seen_[lit_var(analyze_toclear_[j])] = 0;
                    analyze_toclear_.resize(top);
                    return false;
                }
            }
        }
    }
    return true;
}

void SatSolver::analyze(int confl, std::vector<int>& out, int& bt_level, int& lbd)
{
    int path = 0;
    int p = -1;
    out.clear();
    out.push_back(0);  // slot for the asserting literal
    int index = static_cast<int>(trail_.size()) - 1;
    do {
        Clause& c = clauses_[confl];
        if (c.learnt) bump_clause(c);
        for (size_t j = (p == -1 ? 0 : 1); j < c.lits.size(); ++j) {
            int q = c.lits[j];
            int v = lit_var(q);
            if (!seen_[v] && level_[v] > 0) {
                bump_var(v);
                seen_[v] = 1;
                if (level_[v] >= level()) ++path;
                else out.push_back(q);
            }
        }
        while (!seen_[lit_var(trail_[index--])]) {}
        p = trail_[index + 1];
        confl = reason_[lit_var(p)];
        seen_[lit_var(p)] = 0;
        --path;
    } while (path > 0);
    out[0] = lit_not(p);

    analyze_toclear_ = out;
    uint32_t abstract = 0;
    for (size_t i = 1; i < out.size(); ++i) abstract |= 1u << (level_[lit_var(out[i])] & 31);
    size_t j = 1;
    for (size_t i = 1; i < out.size(); ++i)
        if (reason_[lit_var(out[i])] == -1 || !lit_redundant(out[i], abstract)) out[j++] = out[i];
    out.resize(j);
    for (int l : analyze_toclear_) seen_[lit_var(l)] = 0;

    if (out.size() == 1) {
        bt_level = 0;
    } else {
        size_t mx = 1;
        for (size_t i = 2; i < out.size(); ++i)
            if (level_[lit_var(out[i])] > level_[lit_var(out[mx])]) mx = i;
        std::swap(out[1], out[mx]);
        bt_level = level_[lit_var(out[1])];
    }
    // literal block distance: number of distinct decision levels in the clause
    std::vector<int> lv;
    for (int l : out) lv.push_back(level_[lit_var(l)]);
    std::sort(lv.begin(), lv.end());
    lbd = static_cast<int>(std::unique(lv.begin(), lv.end()) - lv.begin());
}

void SatSolver::backtrack(int lvl)
{
    if (level() <= lvl) return;
    for (int i = static_cast<int>(trail_.size()) - 1; i >= trail_lim_[lvl]; --i) {
        int v = lit_var(trail_[i]);
        phase_[v] = assigns_[v];
        assigns_[v] = 2;
        heap_insert(v);
    }
    qhead_ = trail_lim_[lvl];
    trail_.resize(trail_lim_[lvl]);
    trail_lim_.resize(lvl);
}

int SatSolver::pick_branch()
{
    while (!heap_.empty()) {
        int v = heap_pop();
        if (assigns_[v] == 2) return mk_lit(v, phase_[v] == 0);
    }
    return -1;
}

void SatSolver::reduce_db()
{
    std::vector<int> cand;
    for (int ci = 0; ci < static_cast<int>(clauses_.size()); ++ci) {
        const Clause& c = clauses_[ci];
        if (!c.learnt || c.deleted || c.lbd <= 2 || c.lits.size() <= 2) continue;
        int v = lit_var(c.lits[0]);
        if (reason_[v] == ci && value(c.lits[0]) == 1) continue;  // locked: it is a reason
        cand.push_back(ci);
    }
    std::sort(cand.begin(), cand.end(), [&](int a, int b) {
        if (clauses_[a].lbd != clauses_[b].lbd) return clauses_[a].lbd > clauses_[b].lbd;
        return clauses_[a].act < clauses_[b].act;
    });
    for (size_t k = 0; k < cand.size() / 2; ++k) {
        Clause& c = clauses_[cand[k]];
        c.deleted = true;
        std::vector<int>().swap(c.lits);
        --num_learnts_;
    }
    for (auto& w : watches_) w.clear();
    for (int ci = 0; ci < static_cast<int>(clauses_.size()); ++ci)
        if (!clauses_[ci].deleted) attach(ci);
    max_learnts_ *= 1.1;
}

double SatSolver::luby(double y, int x)
{
    int size = 1, seq = 0;
    while (size < x + 1) {
        ++seq;
        size = 2 * size + 1;
    }
    while (size - 1 != x) {
        size = (size - 1) >> 1;
        --seq;
        x = x % size;
    }
    return std::pow(y, seq);
}

SatResult SatSolver::solve(int64_t conflict_limit)
{
    if (!ok_) return SatResult::UNSAT;
    backtrack(0);
    if (propagate() != -1) {
        ok_ = false;
        return SatResult::UNSAT;
    }
    max_learnts_ = std::max(4000.0, static_cast<double>(clauses_.size()) / 3.0);
    const int64_t end = conflict_limit < 0 ? -1 : static_cast<int64_t>(conflicts_) + conflict_limit;
    std::vector<int> learnt;
    for (int restart = 0;; ++restart) {
        const int64_t budget = static_cast<int64_t>(luby(2.0, restart) * 100);
        int64_t here = 0;
        for (;;) {
            int confl = propagate();
            if (confl != -1) {
                ++conflicts_;
                ++here;
                if (level() == 0) {
                    ok_ = false;
                    return SatResult::UNSAT;
                }
                int bt, lbd;
                analyze(confl, learnt, bt, lbd);
                backtrack(bt);
                if (learnt.size() == 1) {
                    enqueue(learnt[0], -1);
                } else {
                    Clause c;
                    c.lits = learnt;
                    c.learnt = true;
                    c.lbd = lbd;
                    clauses_.push_back(std::move(c));
                    int ci = static_cast<int>(clauses_.size()) - 1;
                    attach(ci);
                    bump_clause(clauses_[ci]);
                    ++num_learnts_;
                    enqueue(learnt[0], ci);
                }
                var_inc_ /= var_decay_;
                cla_inc_ /= 0.999f;
            } else {
                if (here >= budget) {
                    backtrack(0);
                    break;
                }
                if (end >= 0 && static_cast<int64_t>(conflicts_) >= end) {
                    backtrack(0);
                    return SatResult::UNKNOWN;
                }
                if (num_learnts_ - static_cast<int>(trail_.size()) >= max_learnts_) reduce_db();
                int next = pick_branch();
                if (next == -1) return SatResult::SAT;  // complete assignment; model stays readable
                trail_lim_.push_back(static_cast<int>(trail_.size()));
                enqueue(next, -1);
            }
        }
    }
}
