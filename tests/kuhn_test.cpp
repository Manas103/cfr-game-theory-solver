// Three layers: (1) terminal payoffs hand-derived from the rules directly,
// (2) best_response.hpp checked against a pure strategy whose exploitability
// was worked out by hand on paper (not taken from any CFR library or
// memorized Kuhn-equilibrium formula, to keep this test independent of the
// one piece of literature this project otherwise trusts from memory, the
// -1/18 game-value constant), (3) CFR itself converging towards that
// constant and towards zero exploitability.
#include <cstdio>

#include "best_response.hpp"
#include "cfr_solver.hpp"
#include "check.hpp"
#include "kuhn_poker.hpp"

using namespace kuhn;

static void test_terminal_payoffs() {
    // Showdown, no bets: higher card wins the other's ante (1).
    CHECK_NEAR(terminal_value_p1("xx", 2, 0), 1.0, 1e-12);
    CHECK_NEAR(terminal_value_p1("xx", 0, 2), -1.0, 1e-12);
    // Player 0 folds to a bet after checking: loses exactly the ante.
    CHECK_NEAR(terminal_value_p1("xbf", 2, 0), -1.0, 1e-12);
    CHECK_NEAR(terminal_value_p1("xbf", 0, 2), -1.0, 1e-12);
    // Showdown after a bet and a call: winner nets 2 (ante + bet).
    CHECK_NEAR(terminal_value_p1("xbc", 2, 0), 2.0, 1e-12);
    CHECK_NEAR(terminal_value_p1("xbc", 0, 2), -2.0, 1e-12);
    // Player 1 folds to player 0's opening bet: player 0 wins the ante.
    CHECK_NEAR(terminal_value_p1("bf", 2, 0), 1.0, 1e-12);
    CHECK_NEAR(terminal_value_p1("bf", 0, 2), 1.0, 1e-12);
    CHECK_NEAR(terminal_value_p1("bc", 2, 0), 2.0, 1e-12);
    CHECK_NEAR(terminal_value_p1("bc", 0, 2), -2.0, 1e-12);
}

// "Always bet when possible, always call when facing a bet", for both
// players at every card. This always ends in a showdown with one bet each
// side ("bc" from the root), so the game value to player 0 is the average
// of +-2 over the 6 deals, i.e. exactly 0.
//
// Exploitability is 1/3, not the more naive-looking 1/2: a best responder
// cannot pick a different root action depending on the opponent's hidden
// card, only on their own, so (for example) a best-responding player 1
// facing card Q must choose one action that is good on average against
// both a Jack and a King behind it, not a different action for each. See
// README "Findings" for the full per-card derivation and for how an
// earlier, information-set-blind version of this computation got 0.5.
static void test_always_bet_strategy() {
    InfoSetTable table;
    for (const std::string& history : {std::string(""), std::string("x"), std::string("b"),
                                        std::string("xb")}) {
        for (int card = 0; card < 3; ++card) {
            InfoSetNode node;
            node.strategy_sum = {0.0, 1.0};  // second action (bet, or call) with probability 1
            table[std::to_string(card) + history] = node;
        }
    }

    CHECK_NEAR(evaluate_game_value_p1(table), 0.0, 1e-9);
    CHECK_NEAR(exploitability(table), 1.0 / 3.0, 1e-9);
    CHECK_NEAR(best_response_value_as_p1(table), 1.0 / 3.0, 1e-9);
    CHECK_NEAR(best_response_value_as_p2(table), 1.0 / 3.0, 1e-9);
}

// A second, independent implementation of best response, used only to
// cross-check best_response_value_as_p1/p2: brute force over all 64 pure
// strategies of the free player (3 cards x 2 decision points = 6 binary
// choices) rather than the memoized bottom-up recursion in
// best_response.hpp. This is the exact check that caught both bugs
// described in README "Findings" -- the main implementation agreeing with
// itself proved nothing; agreeing with a differently-structured
// computation is the real evidence.
static double eval_fixed_pure(const InfoSetTable& fixed_strategy, int fixed_player, int free_bits,
                               const std::string& history, int card0, int card1) {
    if (is_terminal(history)) return terminal_value_p1(history, card0, card1);
    const int player = player_at(history);
    const auto acts = actions_at(history);
    if (player == fixed_player) {
        const int card = player == 0 ? card0 : card1;
        const auto it = fixed_strategy.find(std::to_string(card) + history);
        const std::array<double, 2> strat =
            it != fixed_strategy.end() ? it->second.average_strategy() : std::array<double, 2>{0.5, 0.5};
        return strat[0] * eval_fixed_pure(fixed_strategy, fixed_player, free_bits, history + acts[0],
                                           card0, card1) +
               strat[1] * eval_fixed_pure(fixed_strategy, fixed_player, free_bits, history + acts[1],
                                           card0, card1);
    }
    const int own_card = player == 0 ? card0 : card1;
    const int point = (history.empty() || history == "x") ? 0 : 1;
    const int bit = own_card * 2 + point;
    const int action = (free_bits >> bit) & 1;
    return eval_fixed_pure(fixed_strategy, fixed_player, free_bits, history + acts[action], card0, card1);
}

static double brute_force_best_response(const InfoSetTable& fixed_strategy, int fixed_player) {
    double best = fixed_player == 0 ? 1e9 : -1e9;  // free player 1 minimizes, free player 0 maximizes
    for (int bits = 0; bits < 64; ++bits) {
        double total = 0.0;
        for (const auto& deal : kDeals) {
            total += eval_fixed_pure(fixed_strategy, fixed_player, bits, "", deal[0], deal[1]);
        }
        total /= static_cast<double>(kDeals.size());
        if (fixed_player == 0 ? total < best : total > best) best = total;
    }
    return best;
}

static void test_best_response_matches_brute_force() {
    CfrSolver solver(7);
    for (uint64_t i = 0; i < 300000; ++i) solver.run_iteration();
    const InfoSetTable& table = solver.infosets();

    // fixed_player=1 -> free player is 0 -> best_response_value_as_p1
    CHECK_NEAR(best_response_value_as_p1(table), brute_force_best_response(table, 1), 1e-9);
    // fixed_player=0 -> free player is 1, minimizing utility-to-P1
    CHECK_NEAR(-best_response_value_as_p2(table), brute_force_best_response(table, 0), 1e-9);
}

static void test_cfr_converges() {
    CfrSolver solver(1);
    for (uint64_t i = 0; i < 200000; ++i) solver.run_iteration();

    const double value = evaluate_game_value_p1(solver.infosets());
    const double exploit = exploitability(solver.infosets());

    CHECK_NEAR(value, kAnalyticGameValueP1, 0.01);
    CHECK(exploit < 0.02);  // 200k iterations is enough to get well under 20 mbb/hand
}

static void test_exploitability_decreases() {
    CfrSolver solver(2);
    double prev = 1e9;
    for (uint64_t checkpoint : {100ull, 1000ull, 10000ull, 100000ull}) {
        uint64_t done = 0;
        CfrSolver fresh(2);
        while (done < checkpoint) {
            fresh.run_iteration();
            ++done;
        }
        const double exploit = exploitability(fresh.infosets());
        CHECK(exploit < prev);
        prev = exploit;
    }
    (void)solver;
}

int main() {
    test_terminal_payoffs();
    test_always_bet_strategy();
    test_best_response_matches_brute_force();
    test_cfr_converges();
    test_exploitability_decreases();
    return check_summary("kuhn_test");
}
