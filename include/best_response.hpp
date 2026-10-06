// Exact exploitability, computed by full enumeration of Kuhn poker's tree
// rather than sampling: the game has 6 deals and at most 3 actions deep, so
// an exhaustive best response is cheap, and exact enumeration is the right
// oracle to score a sampled, iterative solver (CFR) against. This is this
// project's analogue of the matching-engine repo's "diff against a naive
// reference matcher": a slow, obviously-correct computation diffed against
// the fast, iterative one, not assumed to agree with it.
//
// The first version of this file computed, for each dealt pair of cards
// independently, the best continuation and then averaged over deals. That
// is wrong: it lets the "best responder" pick a different action depending
// on the opponent's hidden card, which is exactly the information a real
// best response does not have. The bug was caught by an independent
// brute-force cross-check (see README "Findings"), not by this file's own
// logic looking wrong on inspection -- a reminder that an internally
// consistent computation can still answer the wrong question. The fix:
// a best-response action is chosen once per information set (the acting
// player's own card and the history, never the opponent's card), by
// aggregating counterfactual value over every opponent card consistent
// with that information set before taking the max, not after.
#pragma once
#include <array>
#include <functional>
#include <string>
#include <unordered_map>

#include "cfr_solver.hpp"
#include "kuhn_poker.hpp"

namespace kuhn {

namespace detail {

inline std::array<double, 2> strategy_at(const InfoSetTable& table, int card,
                                          const std::string& history) {
    auto it = table.find(std::to_string(card) + history);
    return it != table.end() ? it->second.average_strategy() : std::array<double, 2>{0.5, 0.5};
}

// Probability that `fixed_player` takes exactly the actions already baked
// into `history`, given their card, under `fixed_strategy`. The free
// player's own moves in `history` contribute a factor of 1: this is a
// counterfactual-style reach probability (as if the free player always
// plays into this history), which is precisely what makes aggregating
// over it before taking the max well-defined per information set.
inline double fixed_player_path_prob(const InfoSetTable& fixed_strategy, int fixed_player,
                                      const std::string& history, int card0, int card1) {
    double prob = 1.0;
    std::string prefix;
    for (char c : history) {
        const int player = player_at(prefix);
        if (player == fixed_player) {
            const int card = player == 0 ? card0 : card1;
            const auto strat = strategy_at(fixed_strategy, card, prefix);
            const auto acts = actions_at(prefix);
            prob *= (c == acts[0]) ? strat[0] : strat[1];
        }
        prefix.push_back(c);
    }
    return prob;
}

// Resolves a pure best-response action at every information set of
// `free_player` against `fixed_strategy`, memoized and computed bottom-up
// via ordinary recursion (a shallower information set's best action can
// depend on a deeper one's, and recursion resolves the deeper one first
// the first time it is reached). Returns the subtree value (to player 0)
// at the given history once every needed information set is resolved.
class BestResponseSolver {
public:
    BestResponseSolver(const InfoSetTable& fixed_strategy, int fixed_player)
        : fixed_strategy_(fixed_strategy), fixed_player_(fixed_player) {}

    double value_from(const std::string& history, int card0, int card1) {
        if (is_terminal(history)) return terminal_value_p1(history, card0, card1);

        const int player = player_at(history);
        const auto acts = actions_at(history);
        if (player == fixed_player_) {
            const int card = player == 0 ? card0 : card1;
            const auto strat = strategy_at(fixed_strategy_, card, history);
            return strat[0] * value_from(history + acts[0], card0, card1) +
                   strat[1] * value_from(history + acts[1], card0, card1);
        }

        const int free_player = 1 - fixed_player_;
        const int own_card = free_player == 0 ? card0 : card1;
        const std::string key = std::to_string(own_card) + history;
        auto it = resolved_action_.find(key);
        if (it == resolved_action_.end()) {
            it = resolved_action_.emplace(key, resolve(history, free_player, own_card)).first;
        }
        return value_from(history + acts[it->second], card0, card1);
    }

private:
    // Aggregates counterfactual value for each action at this information
    // set over every opponent card consistent with it, then picks whichever
    // action is best *for the free player*. value_from always returns
    // utility to player 0, so player 0 wants the max of it and player 1
    // wants the min -- using the wrong sign here was this file's second
    // bug (see README "Findings"): a free player 1 that maximized
    // utility-to-P1 was best-responding against its own interest.
    // Recursing into value_from may resolve deeper information sets first.
    int resolve(const std::string& history, int free_player, int own_card) {
        std::array<double, 2> cv{0.0, 0.0};
        const auto acts = actions_at(history);
        for (int opp = 0; opp < 3; ++opp) {
            if (opp == own_card) continue;
            const int card0 = free_player == 0 ? own_card : opp;
            const int card1 = free_player == 0 ? opp : own_card;
            const double w = fixed_player_path_prob(fixed_strategy_, fixed_player_, history, card0, card1);
            for (int a = 0; a < 2; ++a) {
                cv[a] += w * value_from(history + acts[a], card0, card1);
            }
        }
        if (free_player == 0) return cv[0] >= cv[1] ? 0 : 1;   // player 0 maximizes utility-to-P1
        return cv[0] <= cv[1] ? 0 : 1;                          // player 1 minimizes utility-to-P1
    }

    const InfoSetTable& fixed_strategy_;
    int fixed_player_;
    std::unordered_map<std::string, int> resolved_action_;
};

}  // namespace detail

// v1(sigma2): the value player 0 gets by best-responding to player 1's
// average strategy (player 1 held fixed, player 0 free).
inline double best_response_value_as_p1(const InfoSetTable& avg_strategy) {
    detail::BestResponseSolver solver(avg_strategy, /*fixed_player=*/1);
    double total = 0.0;
    for (const auto& deal : kDeals) total += solver.value_from("", deal[0], deal[1]);
    return total / static_cast<double>(kDeals.size());
}

// v2(sigma1): the value player 1 gets by best-responding to player 0's
// average strategy (player 0 held fixed, player 1 free to minimize
// utility-to-P1, which is how `resolve()` treats a free player 1). The
// result is utility-to-P1 under that play; negating it converts to
// utility-to-P2, which is what v2 is defined to be.
inline double best_response_value_as_p2(const InfoSetTable& avg_strategy) {
    detail::BestResponseSolver solver(avg_strategy, /*fixed_player=*/0);
    double total = 0.0;
    for (const auto& deal : kDeals) total += solver.value_from("", deal[0], deal[1]);
    return -(total / static_cast<double>(kDeals.size()));
}

// Standard 2-player zero-sum exploitability: the average of how much each
// player could gain by unilaterally switching to their best response,
// against the other player's current (average) strategy. Zero only at an
// exact Nash equilibrium.
inline double exploitability(const InfoSetTable& avg_strategy) {
    return 0.5 * (best_response_value_as_p1(avg_strategy) + best_response_value_as_p2(avg_strategy));
}

// Exact expected value to player 0 when BOTH players play avg_strategy
// (no maximization at all): compared against the published -1/18 constant.
inline double evaluate_value_p1(const InfoSetTable& strategy, const std::string& history, int card0,
                                 int card1) {
    if (is_terminal(history)) return terminal_value_p1(history, card0, card1);
    const int player = player_at(history);
    const int card = player == 0 ? card0 : card1;
    const auto strat = detail::strategy_at(strategy, card, history);
    const auto acts = actions_at(history);
    return strat[0] * evaluate_value_p1(strategy, history + acts[0], card0, card1) +
           strat[1] * evaluate_value_p1(strategy, history + acts[1], card0, card1);
}

inline double evaluate_game_value_p1(const InfoSetTable& strategy) {
    double total = 0.0;
    for (const auto& deal : kDeals) total += evaluate_value_p1(strategy, "", deal[0], deal[1]);
    return total / static_cast<double>(kDeals.size());
}

}  // namespace kuhn
