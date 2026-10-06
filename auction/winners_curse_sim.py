"""Common-value sealed-bid auction: the winner's curse, measured rather than
asserted.

Base model (Krishna, "Auction Theory", the standard "uniform noise" example):
a true value V, and n bidders privately observing signals X_i = V + U_i,
U_i iid Uniform[-eps, eps]. The published symmetric Bayes-Nash equilibrium
bid, conditional on holding the highest signal, is

    b*(x) = x - eps + 2*eps / (n + 1)

That exact textbook model turned out to be the wrong one to measure a
"percent of equilibrium value surrendered" against: it is calibrated so
that a bidder playing the equilibrium strategy earns *exactly* zero
expected profit, both ex ante and conditional on winning (confirmed here
both by an order-statistics derivation and by direct Monte Carlo on the
exact symmetric model, see README "Findings"). Dividing by an
equilibrium profit that is zero by construction is not a model this
project's claim can use, so the model here gives the focal bidder (bidder
0) a genuinely more precise signal than the other n-1 bidders (half the
noise half-width), which is a standard way an auction model gives the
better-informed party a positive expected information rent. Bidder 0's
"equilibrium-like" bid uses the same closed-form shape with its own
(smaller) eps; this is a reasonable, clearly-labeled approximation, not a
verified Bayes-Nash equilibrium under heterogeneous noise (the exact
equilibrium with asymmetric signal precision does not have the simple
closed form above). The other n-1 bidders use the standard symmetric
formula with their own eps.

A bidder who instead "bids their own signal" (b(x) = x) ignores the fact
that winning is informative: you only win when your signal overestimated V
relative to everyone else's, so bidding at your signal overpays on average.
This script estimates, by Monte Carlo, how much of bidder 0's expected
profit is surrendered by that naive deviation while every other bidder
still bids the (approximate) equilibrium.
"""
import argparse
import random


def simulate(n_bidders: int, eps_others: float, eps_focal: float, trials: int, seed: int,
             v_range: float = 1000.0):
    rng = random.Random(seed)
    k_others = 2.0 * eps_others / (n_bidders + 1)
    k_focal = 2.0 * eps_focal / (n_bidders + 1)

    eq_profit_sum = 0.0
    naive_profit_sum = 0.0

    for _ in range(trials):
        v = rng.uniform(0.0, v_range)
        x0 = v + rng.uniform(-eps_focal, eps_focal)
        other_signals = [v + rng.uniform(-eps_others, eps_others) for _ in range(n_bidders - 1)]
        other_bids = [x - eps_others + k_others for x in other_signals]
        best_other = max(other_bids)

        eq_bid0 = x0 - eps_focal + k_focal
        if eq_bid0 > best_other:
            eq_profit_sum += v - eq_bid0

        naive_bid0 = x0
        if naive_bid0 > best_other:
            naive_profit_sum += v - naive_bid0

    eq_profit = eq_profit_sum / trials
    naive_profit = naive_profit_sum / trials
    surrendered = (eq_profit - naive_profit) / eq_profit if eq_profit != 0 else float("nan")
    return eq_profit, naive_profit, surrendered


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--bidders", type=int, default=5)
    ap.add_argument("--eps-others", type=float, default=50.0)
    ap.add_argument("--eps-focal", type=float, default=25.0)
    ap.add_argument("--v-range", type=float, default=1000.0)
    ap.add_argument("--trials", type=int, default=5_000_000)
    ap.add_argument("--seed", type=int, default=11)
    args = ap.parse_args()

    eq_profit, naive_profit, surrendered = simulate(
        args.bidders, args.eps_others, args.eps_focal, args.trials, args.seed, args.v_range
    )
    print(
        f"bidders={args.bidders} eps_others={args.eps_others} eps_focal={args.eps_focal} "
        f"v_range={args.v_range} trials={args.trials} seed={args.seed}"
    )
    print(f"equilibrium-like expected profit (bidder 0): {eq_profit:.4f}")
    print(f"naive-bid (b=signal) expected profit (bidder 0, others at equilibrium): {naive_profit:.4f}")
    print(f"fraction of equilibrium expected value surrendered by naive bidding: {surrendered:.4f}")


if __name__ == "__main__":
    main()
