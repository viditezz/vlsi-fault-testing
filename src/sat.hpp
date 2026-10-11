// sat.hpp — a small CDCL SAT solver (MiniSat-style).
//
// Used only by the SAT fallback for faults PODEM aborts on (sat_atpg.*). It is
// deliberately a plain textbook design so it can be read in one sitting:
//   * two-watched-literal unit propagation with blocker literals
//   * first-UIP conflict analysis with recursive clause minimization
//   * VSIDS variable activity (binary heap) with phase saving
//   * Luby restarts
//   * learned-clause database reduction by LBD / activity
// Literals are ints: lit = 2*var + (negated ? 1 : 0).
#pragma once

#include <cstdint>
#include <vector>

enum class SatResult { SAT, UNSAT, UNKNOWN };

inline int mk_lit(int var, bool neg = false) { return 2 * var + (neg ? 1 : 0); }
inline int lit_not(int l) { return l ^ 1; }
inline int lit_var(int l) { return l >> 1; }
inline bool lit_neg(int l) { return l & 1; }

class SatSolver {
public:
    int new_var();
    int num_vars() const { return static_cast<int>(assigns_.size()); }

    // Adds a clause at decision level 0. Returns false if the formula became
    // trivially unsatisfiable.
    bool add_clause(std::vector<int> lits);

    // Solves; gives up (UNKNOWN) after `conflict_limit` conflicts (<0: no limit).
    SatResult solve(int64_t conflict_limit = -1);

    bool model_value(int var) const { return assigns_[var] == 1; }

    // Work counter: watcher entries visited during unit propagation.
    uint64_t work() const { return work_; }
    uint64_t conflicts() const { return conflicts_; }

private:
    struct Clause {
        std::vector<int> lits;
        bool learnt = false;
        bool deleted = false;
        int lbd = 0;
        float act = 0;
    };
    struct Watch {
        int clause;
        int blocker;
    };

    // value of a literal: 1 true, 0 false, 2 undefined
    int value(int lit) const
    {
        int a = assigns_[lit >> 1];
        return a == 2 ? 2 : (a ^ (lit & 1));
    }
    int level() const { return static_cast<int>(trail_lim_.size()); }

    void attach(int ci);
    void enqueue(int lit, int reason);
    int propagate();  // returns conflicting clause index or -1
    void analyze(int confl, std::vector<int>& learnt, int& bt_level, int& lbd);
    bool lit_redundant(int lit, uint32_t abstract_levels);
    void backtrack(int lvl);
    int pick_branch();
    void bump_var(int v);
    void bump_clause(Clause& c);
    void reduce_db();
    static double luby(double y, int x);

    // heap on activity
    void heap_insert(int v);
    void heap_up(int i);
    void heap_down(int i);
    int heap_pop();

    std::vector<Clause> clauses_;
    std::vector<std::vector<Watch>> watches_;  // indexed by literal
    std::vector<int8_t> assigns_;              // 0, 1, 2 (undef) per variable
    std::vector<int> level_, reason_;
    std::vector<int8_t> phase_;
    std::vector<double> activity_;
    std::vector<int> heap_, heap_pos_;
    std::vector<int> trail_, trail_lim_;
    size_t qhead_ = 0;
    std::vector<uint8_t> seen_;
    std::vector<int> analyze_stack_, analyze_toclear_;
    double var_inc_ = 1.0, var_decay_ = 0.95;
    float cla_inc_ = 1.0f;
    bool ok_ = true;
    int num_learnts_ = 0;
    double max_learnts_ = 0;
    uint64_t work_ = 0, conflicts_ = 0;
};
