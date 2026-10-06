# Game-Theory Solver: Betting Game and Common-Value Auction

Vanilla counterfactual regret minimization (CFR) solves three-card Kuhn
poker, scored against the game's published analytic Nash equilibrium value
and against an exact (not sampled) best-response computation; a separate
Monte Carlo prices the winner's curse in a sealed-bid common-value auction.
Every number below was measured on this machine, not targeted, and two of
them are honestly short of this project's own resume bullet, with the
reason recorded in Findings rather than smoothed over.

## Why this exists

Almost no applicant to a trading desk arrives with a computed game-theoretic
equilibrium. Two design choices make the claims here checkable rather than
decorative: Kuhn poker is small enough to have a known analytic equilibrium
value, so CFR is graded against an oracle instead of a chosen opponent, and
the auction's headline is exploitability/surrendered-value, not a win rate
against an arbitrary bot, which would be unfalsifiable.

## Honest framing, up front

- **Vanilla CFR, not CFR+ or any sampling variant.** Kuhn poker has 12
  information sets; full traversal every iteration is cheap enough that
  nothing more sophisticated is needed to reach 10M iterations.
- **Exploitability is computed exactly, by full tree enumeration, not
  sampled.** The game has 6 possible deals and at most 3 plies, so this is
  cheap and exact; it is also the thing that caught two real bugs in this
  project's first draft (see Findings), which a sampled estimate would
  likely have hidden inside its own noise.
- **The auction section is not the textbook model's exact equilibrium.**
  The published closed-form bid for a symmetric, uniform-noise common-value
  auction turns out to earn exactly zero expected profit at equilibrium (by
  construction, confirmed here independently by order-statistics and by
  Monte Carlo), which makes "percent of equilibrium value surrendered"
  divide by zero. The model actually measured gives the focal bidder a
  more precise signal than its competitors, a standard way to generate a
  genuine, non-degenerate information rent; this is disclosed as an
  approximation, not a verified equilibrium under heterogeneous signal
  precision (no simple closed form exists for that case).
- **Every figure below was measured on this machine** (8 physical / 16
  logical cores, AMD Ryzen 7 7800X3D, WSL2 Ubuntu 22.04 capped to 12
  logical cores, g++ 11.4, `-O2`, Python 3.12), not targeted. Raw output is
  under [`docs/`](docs/).
- **"Milli-big-blinds per hand" here means milli-antes.** Kuhn poker has no
  blind structure; this project follows the common convention in the CFR
  literature of reporting a simplified poker game's base betting unit in
  the same "mbb" units used for full poker, rather than inventing a
  different unit.

## Architecture

```
include/kuhn_poker.hpp      game definition: history encoding, terminal payoffs,
                             the published -1/18 analytic game value
include/cfr_solver.hpp      vanilla CFR over the 12 information sets
include/best_response.hpp   exact exploitability: information-set-respecting
                             best response, enumerated, not sampled
src/run_cfr.cpp              CLI: N iterations, log-spaced exploitability checkpoints

tests/check.hpp               hand-rolled check suite
tests/kuhn_test.cpp           terminal payoffs, a hand-derived pure-strategy
                               exploitability oracle, an independent brute-force
                               cross-check of best_response.hpp, and CFR's own
                               convergence toward the analytic game value

auction/winners_curse_sim.py  the common-value auction Monte Carlo
auction/test_auction.py       pytest: the degenerate symmetric case, the
                                asymmetric-information case, monotonicity in n
```

### CFR over an explicit, unambiguous history encoding

Kuhn poker tutorials commonly encode history with two characters ('p'ass,
'b'et) and treat "pass after a bet" as a fold. This project instead uses
four explicit characters (`x` check, `b` bet, `f` fold, `c` call) so that a
reader (or a reviewer) checking `terminal_value_p1` against the rules does
not have to first decode an overloaded convention. `kuhn_poker.hpp`'s
payoff table was hand-derived directly from the rules, not copied from a
reference, and is covered by `tests/kuhn_test.cpp`'s first block.

### Exploitability: the bug that mattered most in this project

`best_response.hpp`'s exact exploitability computation is the project's
real validation layer; see Findings for the two bugs it took to get it
right. The final algorithm resolves a pure best-response action at every
information set of the "free" player by aggregating counterfactual value
over every opponent card consistent with that information set (weighted by
the fixed player's own path probability), then recursing: a shallower
information set's resolution can depend on a deeper one's, and ordinary
memoized recursion resolves the deeper one first the first time it is
needed, with no manual depth bookkeeping.

## Validation

### 1. Reference-oracle diff: exploitability vs. an independent brute force

`tests/kuhn_test.cpp::test_best_response_matches_brute_force` cross-checks
`best_response_value_as_p1`/`_p2` against a *differently structured*
computation: brute force over all 64 pure strategies of the free player
(3 cards x 2 decision points), rather than the memoized recursion the main
implementation uses. This is the test that caught both bugs described
below; the main implementation agreeing with itself proved nothing.

### 2. A hand-derived case, worked out independently of any library

`test_always_bet_strategy` fixes both players to "always bet when possible,
always call when facing a bet" and checks the game value (0, easy to see:
every hand reaches showdown with one bet each side, and wins and losses of
2 are equally likely) and the exploitability (1/3, not the more
naive-looking 1/2 -- see Findings for why) against values worked out by
hand on paper before any code was run.

### 3. CFR's own convergence against the analytic constant

`test_cfr_converges` runs 200,000 iterations and checks the exact
best-response exploitability drops under 20 mbb/hand and the average
strategy's game value lands within 0.01 of the published -1/18.

### 4. Sanitizers

AddressSanitizer + UBSan clean run, `kuhn_test` and a `run_cfr` smoke run:
[`docs/asan_ubsan_clean_run.txt`](docs/asan_ubsan_clean_run.txt).
ThreadSanitizer is not run; this binary is single-threaded (one sequential
game-tree traversal per iteration), so there is no concurrency to
race-check.

Raw test output: [`docs/test_output.txt`](docs/test_output.txt),
[`docs/auction_test_output.txt`](docs/auction_test_output.txt).

## Findings

**Exploitability plateaued around 275 mbb/hand from 50,000 iterations all
the way to 1,000,000, while the average strategy's game value converged
cleanly to -1/18 the whole time.** That split (one metric converging
perfectly, the other stuck) was the first sign something was wrong
specifically in `best_response.hpp`, not in CFR itself. Dumping the
converged average strategy showed 11 of 12 information sets matching the
published Kuhn-equilibrium family (a one-parameter family indexed by
alpha; this project's run converged to alpha roughly 0.28) almost exactly
-- strong evidence CFR was correct -- which narrowed the search to the
exploitability computation. **Root cause: the first version of
`best_response_value_p1` took the best continuation for each dealt pair of
cards independently, then averaged over deals.** That silently lets the
"best responder" choose a different action depending on the opponent's
hidden card, which a real best response cannot do (it only knows its own
card and the public history). An independent brute-force cross-check over
all 64 pure strategies (now `test_best_response_matches_brute_force`)
confirmed the bug directly: it returned 0.33 against a fixed deterministic
opponent strategy for which the first implementation claimed 0.5, and a
redone hand derivation -- this time correctly holding one action fixed per
information set across both possible opponent cards -- also gave 1/3, not
1/2. **A second bug of the same shape was hiding behind the first:** once
information sets were resolved correctly, the resolver still always
maximized utility-to-player-0, even when player 1 was the free player
best-responding, which means a "best-responding" player 1 was
(backwards) trying to make player 0 richer. The brute-force cross-check
caught this too (a `-1e9`/`1e9` sentinel surfacing in the comparison
during test-writing was the tell). The fix: the free player's action
should maximize *its own* utility, which is `+1 x utility-to-P1` for player
0 and `-1 x utility-to-P1` for player 1. After both fixes, exploitability
fell cleanly from 458 mbb/hand (the uniform-random strategy, i.e. before
any CFR) to 0.12 mbb/hand at 10,000,000 iterations -- the healthy
monotonic curve a correct implementation should have shown from the start.

**The auction's textbook equilibrium bid formula earns exactly zero
expected profit, both ex ante and conditional on winning.** This was not
a coding bug: an order-statistics derivation by hand predicted it (the
formula's shading term is precisely calibrated to offset the expected
winning noise draw), and a direct Monte Carlo on the unmodified symmetric
model confirmed it to within sampling noise (measured equilibrium profit
of roughly 0.00003 to -0.002 across several bidder counts, against a
predicted true value of exactly 0). "Percent of equilibrium value
surrendered" is undefined when the denominator is zero by construction, so
the model actually measured (see "Honest framing") gives the focal bidder
a more precise signal than its competitors, which produces a genuine
positive equilibrium profit and a well-defined ratio.

## Measured results

Machine: 8 physical / 16 logical cores (AMD Ryzen 7 7800X3D), WSL2 Ubuntu
22.04 capped to 12 logical cores, g++ 11.4, `-O2`, Python 3.12.10.

### CFR / Kuhn poker

Raw run: [`docs/cfr_convergence_seed7.csv`](docs/cfr_convergence_seed7.csv).

| Claim | Measured | Meets claim |
|---|---|---|
| Exploitability cut from 231 to 4.2 mbb/hand | **458.33 -> 0.12** mbb/hand (uniform-random baseline to 10,000,000 iterations, seed 7) | No (both endpoints differ from the claimed numbers; the direction and order of magnitude match) |
| 10M iterations | **10,000,000**, exactly | Yes |
| Scored against the game's analytic equilibrium | Average strategy's game value converges to **-0.055556**, matching the published -1/18 to 6 decimal places; exact best-response exploitability (not sampled) used throughout | Yes |
| Simplified multi-round betting game solved by CFR | Three-card Kuhn poker, vanilla CFR, 12 information sets | Yes |

Final exploitability was checked across 3 seeds for stability: 0.12 (seed
7), 0.19 (seed 1), 0.32 (seed 42) mbb/hand -- all comfortably under 1
mbb/hand, i.e. well past the claimed 4.2, not short of it. The claimed
starting point (231 mbb/hand) does not match this project's natural
baseline either: 458.33 mbb/hand is the exact exploitability of the
uniform-random strategy on this game (every information set playing each
action with probability 1/2), which is what "before any CFR" means here
and is not a free parameter to tune.

### Common-value auction

5 bidders, `eps_others=50`, `eps_focal=25` (bidder 0 observes half the
noise half-width of its competitors), `v_range=1000`, 5,000,000 trials,
seed 11. Raw run: [`docs/auction_benchmark_output.txt`](docs/auction_benchmark_output.txt).

| Claim | Measured | Meets claim |
|---|---|---|
| 5-bidder sealed-bid common-value auction | 5 bidders, common value V shared, private noisy signals | Yes |
| Bidding your own signal surrendered 38% of equilibrium expected value | **384%** (equilibrium-like profit 1.55, naive profit -4.41; naive bidding does not just give back value, it turns a profit into a loss) | No |

The honest reading: the specific 38% figure does not survive measurement
in either the exact textbook model (where the denominator is zero) or this
project's asymmetric-information fix (where the measured figure is an
order of magnitude larger). The qualitative finding -- naive bidding is
costly, and more costly with more bidders -- holds throughout: surrendered
value was 223% at 3 bidders, 384% at 5, and 968% at 9 (all in
[`docs/auction_benchmark_output.txt`](docs/auction_benchmark_output.txt)).

## Building and running

```sh
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j"$(( $(nproc) / 2 ))"
./kuhn_test
./run_cfr --iterations 10000000 --seed 7 > cfr.csv
```

ASan + UBSan build:

```sh
mkdir build-asan && cd build-asan
cmake -DCMAKE_BUILD_TYPE=Release -DENABLE_ASAN=ON ..
make -j"$(( $(nproc) / 2 ))"
./kuhn_test
```

Auction simulator (Python 3.12):

```sh
cd auction
python3 -m venv venv
./venv/bin/pip install -r requirements.txt   # Windows: venv\Scripts\pip
./venv/bin/python -m pytest -v
./venv/bin/python winners_curse_sim.py --bidders 5 --eps-others 50 --eps-focal 25 --trials 5000000
```

## Sibling comparison

Nothing in the existing 230+ repo portfolio is game-theoretic: no
equilibrium computation, no regret minimization, no auction model anywhere
else. The nearest thing structurally is `attraction-throughput-simulator`
(a discrete-event simulator with an independent cross-check), which shares
the validation habit (an independently-structured check, not just "it
produced plausible output") and nothing else about the domain.

## Limitations

- CFR here is vanilla, full-traversal CFR, not CFR+ or any Monte Carlo
  sampling variant; it would not scale past a handful of information sets
  without one of those.
- The auction's equilibrium bid for the focal bidder is an approximation
  (same closed-form shape, bidder 0's own noise plugged in), not a solved
  Bayes-Nash equilibrium under heterogeneous signal precision, which has no
  simple closed form.
- The auction uses a bounded but still fairly wide value prior
  (`v_range=1000` against `eps=50`); the true boundary-corrected
  equilibrium bid function near the edges of the support is not modeled.
