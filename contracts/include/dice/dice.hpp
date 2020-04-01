#pragma once

#include <game-contract-sdk/game_base.hpp>

namespace dice {

using eosio::name;
using eosio::asset;
using bytes = std::vector<char>;
using eosio::checksum256;

namespace constant
{
constexpr double all_range = 100.;
constexpr double house_edge = 0.01;

constexpr uint16_t min_bet_param_type = 0;
constexpr uint16_t max_bet_param_type = 1;
constexpr uint16_t max_payout_param_type = 2;
constexpr uint8_t roll_action_type = 0;
}

struct action_type {
    uint8_t value;
};

class dice: public game_sdk::game {
public:
    struct [[eosio::table("roll")]] roll_row {
        uint64_t ses_id;
        uint32_t number;

        uint64_t primary_key() const { return ses_id; }
    };

    using roll_table = eosio::multi_index<"roll"_n, roll_row>;

public:
    dice(name receiver, name code, eosio::datastream<const char*> ds):
        game(receiver, code, ds),
        rolls(_self, _self.value)
    { }

    void on_new_game(uint64_t ses_id) override final;

    void on_action(uint64_t ses_id, uint16_t type, std::vector<uint32_t> params) override final;

    void on_random(uint64_t ses_id, checksum256 rand) override final;

    void on_finish(uint64_t ses_id) override final;

private:
    asset get_win_payout(uint64_t ses_id, uint32_t number) const;

    void check_params(uint64_t ses_id) const;
    void check_bet(uint64_t ses_id) const;
    void check_action_params(const std::vector<uint32_t>& params) const;
    uint32_t rand_range(const checksum256& rand, uint32_t lower, uint32_t upper);

private:
    static double get_win_coefficient(uint32_t num);

private:
    roll_table rolls;
};

} // namespace dice
