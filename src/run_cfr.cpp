// Runs vanilla CFR on Kuhn poker for --iterations hands, computing exact
// exploitability (full-tree best response, see best_response.hpp) at a
// log-spaced schedule of checkpoints, and writes one CSV row per
// checkpoint to stdout.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "best_response.hpp"
#include "cfr_solver.hpp"

int main(int argc, char** argv) {
    uint64_t iterations = 10'000'000;
    uint64_t seed = 7;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--iterations") == 0 && i + 1 < argc) {
            iterations = std::strtoull(argv[++i], nullptr, 10);
        } else if (std::strcmp(argv[i], "--seed") == 0 && i + 1 < argc) {
            seed = std::strtoull(argv[++i], nullptr, 10);
        }
    }

    std::vector<uint64_t> checkpoints;
    for (uint64_t base = 1; base <= iterations; base *= 10) {
        for (uint64_t mult : {1, 2, 5}) {
            const uint64_t cp = base * mult;
            if (cp <= iterations) checkpoints.push_back(cp);
        }
    }
    if (checkpoints.empty() || checkpoints.back() != iterations) checkpoints.push_back(iterations);

    kuhn::CfrSolver solver(seed);
    std::printf("iteration,exploitability_mbb_per_hand,game_value_p1,analytic_game_value_p1\n");

    uint64_t done = 0;
    for (uint64_t cp : checkpoints) {
        while (done < cp) {
            solver.run_iteration();
            ++done;
        }
        const double exploit = kuhn::exploitability(solver.infosets());
        const double value = kuhn::evaluate_game_value_p1(solver.infosets());
        std::printf("%llu,%.6f,%.6f,%.6f\n", static_cast<unsigned long long>(done), exploit * 1000.0,
                    value, kuhn::kAnalyticGameValueP1);
    }
    return 0;
}
