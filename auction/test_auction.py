"""Sanity checks for the winner's-curse Monte Carlo."""
import math

from winners_curse_sim import simulate


def test_symmetric_textbook_model_has_zero_equilibrium_profit():
    # The degenerate case this project's README documents: with no
    # information-precision edge (eps_focal == eps_others), the published
    # closed-form bid is calibrated to earn exactly zero expected profit at
    # equilibrium, so the "percent surrendered" ratio has a ~0 denominator.
    # This is reproduced here as a regression check on that finding, not as
    # the project's main measurement.
    eq_profit, _, _ = simulate(
        n_bidders=5, eps_others=50.0, eps_focal=50.0, trials=1_000_000, seed=1
    )
    assert abs(eq_profit) < 0.5  # should be close to 0; exactly 0 in expectation


def test_more_precise_focal_bidder_earns_positive_equilibrium_profit():
    # Bidder 0 observing a strictly less noisy signal than its competitors
    # is a standard source of a genuine information rent.
    eq_profit, _, _ = simulate(
        n_bidders=5, eps_others=50.0, eps_focal=25.0, trials=1_000_000, seed=2
    )
    assert eq_profit > 0.5


def test_naive_bidding_costs_money_relative_to_equilibrium():
    eq_profit, naive_profit, surrendered = simulate(
        n_bidders=5, eps_others=50.0, eps_focal=25.0, trials=1_000_000, seed=3
    )
    assert naive_profit < eq_profit
    assert surrendered > 0


def test_more_competitors_increase_the_naive_shortfall():
    # More competitors means winning is a stronger signal that your own
    # estimate overshot everyone else's, so the gap between equilibrium
    # and naive profit should widen as n grows.
    _, _, surrendered_3 = simulate(n_bidders=3, eps_others=50.0, eps_focal=25.0,
                                    trials=1_000_000, seed=4)
    _, _, surrendered_9 = simulate(n_bidders=9, eps_others=50.0, eps_focal=25.0,
                                    trials=1_000_000, seed=4)
    assert surrendered_3 < surrendered_9


def test_result_is_a_finite_real_number():
    _, _, surrendered = simulate(n_bidders=5, eps_others=50.0, eps_focal=25.0,
                                  trials=200_000, seed=5)
    assert math.isfinite(surrendered)
