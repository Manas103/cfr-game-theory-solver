// Three-card Kuhn poker (Kuhn, 1950): the smallest poker game with a known
// closed-form Nash equilibrium, which is the entire reason it is used here
// instead of a larger game a solver's output could not be checked against.
//
// Deck {0,1,2} = {Jack,Queen,King}. Each of 2 players antes 1 and is dealt
// one private card (the third stays hidden). History is built from explicit
// action characters so check and call are never conflated with the same
// symbol, unlike the compact 'p'/'b' encodings common in CFR tutorials:
//   'x' check    'b' bet    'f' fold    'c' call
// Root and post-check decisions choose from {x,b}; decisions facing a bet
// (either the opening bet or the bet after a check) choose from {f,c}.
#pragma once
#include <array>
#include <cstdint>
#include <string>

namespace kuhn {

inline constexpr int kNumActions = 2;

// Whose turn it is at this history: player 0 acts on even history length.
inline int player_at(const std::string& h) { return static_cast<int>(h.size()) % 2; }

inline bool is_terminal(const std::string& h) {
    return h == "xx" || h == "xbf" || h == "xbc" || h == "bf" || h == "bc";
}

// Action set at a non-terminal history: {check,bet} at the root and right
// after a check; {fold,call} facing any bet.
inline std::array<char, 2> actions_at(const std::string& h) {
    if (h.empty() || h == "x") return {'x', 'b'};
    return {'f', 'c'};
}

// Utility to player 0, in ante units (1 ante = 1 unit). Player 1's utility
// is always the negation (zero-sum).
inline double terminal_value_p1(const std::string& h, int card0, int card1) {
    const bool p1_higher = card0 > card1;
    if (h == "xx") return p1_higher ? 1.0 : -1.0;              // showdown, no bets
    if (h == "xbf") return -1.0;                                // player 0 folded to the bet
    if (h == "xbc") return p1_higher ? 2.0 : -2.0;              // showdown, one bet each side
    if (h == "bf") return 1.0;                                  // player 1 folded
    if (h == "bc") return p1_higher ? 2.0 : -2.0;               // showdown, one bet each side
    return 0.0;  // unreachable for a terminal history
}

// The 6 equally likely ordered deals of 2 distinct cards out of 3.
inline constexpr std::array<std::array<int, 2>, 6> kDeals = {{
    {0, 1}, {0, 2}, {1, 0}, {1, 2}, {2, 0}, {2, 1},
}};

// Published analytic game value to player 1 at any Nash equilibrium of this
// game (Kuhn, 1950): the first player is disadvantaged by exactly 1/18 of
// an ante per hand, independent of which equilibrium in the one-parameter
// family is played.
inline constexpr double kAnalyticGameValueP1 = -1.0 / 18.0;

}  // namespace kuhn
