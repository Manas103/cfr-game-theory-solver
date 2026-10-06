// Vanilla counterfactual regret minimization over Kuhn poker's 12
// information sets (2 players x 3 cards x 2 decision points each). This is
// the textbook algorithm (Zinkevich et al., 2007); nothing here is CFR+ or
// any sampling variant, which is also why 10M iterations of a 12-information-set
// game is cheap enough to run to completion rather than needing a sampled
// subset.
#pragma once
#include <array>
#include <cstdint>
#include <random>
#include <string>
#include <unordered_map>

#include "kuhn_poker.hpp"

namespace kuhn {

struct InfoSetNode {
    std::array<double, kNumActions> regret_sum{0.0, 0.0};
    std::array<double, kNumActions> strategy_sum{0.0, 0.0};

    std::array<double, kNumActions> current_strategy() const {
        double pos_sum = 0.0;
        std::array<double, kNumActions> strat{};
        for (int a = 0; a < kNumActions; ++a) {
            strat[a] = regret_sum[a] > 0.0 ? regret_sum[a] : 0.0;
            pos_sum += strat[a];
        }
        if (pos_sum > 0.0) {
            for (int a = 0; a < kNumActions; ++a) strat[a] /= pos_sum;
        } else {
            strat.fill(1.0 / kNumActions);
        }
        return strat;
    }

    std::array<double, kNumActions> average_strategy() const {
        const double sum = strategy_sum[0] + strategy_sum[1];
        if (sum <= 0.0) return {0.5, 0.5};
        return {strategy_sum[0] / sum, strategy_sum[1] / sum};
    }
};

// Keyed by "<card><history>", e.g. "2xb" = player holding the King, facing
// history "xb". The card is always that of the player to act, so this
// string is a legal information set label without ambiguity.
using InfoSetTable = std::unordered_map<std::string, InfoSetNode>;

class CfrSolver {
public:
    explicit CfrSolver(uint64_t seed) : rng_(seed) {}

    // Runs one iteration (one full traversal over a randomly dealt pair of
    // cards) and returns the root counterfactual value for player 0, purely
    // for callers that want to track convergence of the game value itself.
    double run_iteration() {
        const auto& deal = kDeals[deal_dist_(rng_)];
        return cfr(deal[0], deal[1], "", 1.0, 1.0);
    }

    const InfoSetTable& infosets() const { return infosets_; }

private:
    double cfr(int card0, int card1, const std::string& history, double p0, double p1) {
        if (is_terminal(history)) {
            const int player = player_at(history);
            const double v = terminal_value_p1(history, card0, card1);
            return player == 0 ? v : -v;
        }

        const int player = player_at(history);
        const int card = player == 0 ? card0 : card1;
        const std::string key = std::to_string(card) + history;
        InfoSetNode& node = infosets_[key];
        const double reach = player == 0 ? p0 : p1;
        const auto strategy = node.current_strategy();

        const auto acts = actions_at(history);
        std::array<double, kNumActions> util{};
        double node_util = 0.0;
        for (int a = 0; a < kNumActions; ++a) {
            const std::string next_history = history + acts[a];
            const double child = player == 0
                                      ? -cfr(card0, card1, next_history, p0 * strategy[a], p1)
                                      : -cfr(card0, card1, next_history, p0, p1 * strategy[a]);
            util[a] = child;
            node_util += strategy[a] * child;
        }

        const double opp_reach = player == 0 ? p1 : p0;
        for (int a = 0; a < kNumActions; ++a) {
            node.regret_sum[a] += opp_reach * (util[a] - node_util);
            node.strategy_sum[a] += reach * strategy[a];
        }
        return node_util;
    }

    std::mt19937_64 rng_;
    std::uniform_int_distribution<size_t> deal_dist_{0, kDeals.size() - 1};
    InfoSetTable infosets_;
};

}  // namespace kuhn
