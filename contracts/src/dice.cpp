#include <dice/dice.hpp>

namespace dice {

void dice::check_params(uint64_t ses_id) const {
    const auto min_bet = get_param_value(ses_id, constant::min_bet_param_type);
    const auto max_bet = get_param_value(ses_id, constant::max_bet_param_type);
    const auto max_payout = get_param_value(ses_id, constant::max_payout_param_type);

    eosio::check(min_bet != std::nullopt, "absent min bet param");
    eosio::check(max_bet != std::nullopt, "absent max bet param");
    eosio::check(max_payout != std::nullopt, "absent max payout param");

    eosio::check(max_bet > min_bet, "wrong bet border: max_bet less than min_bet");
    eosio::check(max_payout > max_bet, "max payout is less than max bet");
}

void dice::check_bet(uint64_t ses_id) const {
    const auto min_bet = asset(*get_param_value(ses_id, constant::min_bet_param_type), core_symbol);
    const auto max_bet = asset(*get_param_value(ses_id, constant::max_bet_param_type), core_symbol);

    const auto& session = get_session(ses_id);
    eosio::check(min_bet <= session.deposit, "deposit less than min bet");
    eosio::check(max_bet >= session.deposit, "deposit greater than max bet");
}

void dice::check_action_params(const std::vector<game_sdk::param_t>& params) const {
    eosio::check(params.size() == 2, "params amount should be 2");
    eosio::check(params[0] > 0, "small number should be more than 0");
    eosio::check(params[0] < 100, "small number should be less than 100");

    eosio::check(params[1] > 1, "big number should be more than 1");
    eosio::check(params[1] <= 100, "big number should be less or equal than 100");

    eosio::check(params[0] < params[1], "small number should be less than big number");
}

double dice::get_win_coefficient(dice_number_t num_small, dice_number_t num_big) {
    return (constant::all_range / (constant::all_range - (num_big - num_small))) * (1. - constant::house_edge);
}

void dice::on_new_game(uint64_t ses_id) {
    check_params(ses_id);
    check_bet(ses_id);

    // Requesting game action from player
    require_action(constant::roll_action_type);
}

void dice::on_action(uint64_t ses_id, uint16_t type, std::vector<game_sdk::param_t> params) {
    eosio::check(type == constant::roll_action_type, "allowed only roll action with type 0");
    check_action_params(params);

    const dice_number_t num_small = params[0]; ///< "params[0]" is a player small number choice
    const dice_number_t num_big = params[1]; ///< "params[1]" is a player big number choice
    rolls.emplace(get_self(), [&](auto& row) {
        row.ses_id = ses_id;
        row.number_small = num_small;
        row.number_big = num_big;
    });

    // Here we say to platform the amount of maximum possible payout
    // It's platform requirement, game must provide that amount every time when it's changed
    update_max_win(get_win_payout(ses_id, num_small, num_big));

    // Request random number
    // After that contract will initiate secure random generation protocol
    // Result will handled by "on_random" method
    require_random();
}

asset dice::get_win_payout(uint64_t ses_id, dice_number_t num_small, dice_number_t num_big) const {
    auto win_payout = get_session(ses_id).deposit;

    // Assets can't be multiply to double. Use dirty way.
    win_payout.amount = uint64_t(double(win_payout.amount) * get_win_coefficient(num_small, num_big));

    const auto max_payout = asset(*get_param_value(ses_id, constant::max_payout_param_type), core_symbol);
    return win_payout < max_payout ? win_payout : max_payout;
}

void dice::on_random(uint64_t ses_id, checksum256 rand) {
    // get random in range [0, 100)
    const dice_number_t actual_number = get_prng(std::move(rand))->next(0, 100);
    eosio::print("rand num: ", actual_number, "\n");

    auto payout = zero_asset;

    const auto bet_num_small = rolls.get(ses_id).number_small;
    const auto bet_num_big = rolls.get(ses_id).number_big;
    if (bet_num_small <= actual_number && actual_number < bet_num_big) { // Win
        payout = get_win_payout(ses_id, bet_num_small, bet_num_big);
    }

    // Initiate game finalization and fund transferring to player and casino
    finish_game(payout, std::vector<game_sdk::param_t> {actual_number});
}

void dice::on_finish(uint64_t ses_id) {
    const auto roll_itr = rolls.find(ses_id);

    if (roll_itr != rolls.end()) {
        // Just clear data of finished session
        rolls.erase(roll_itr);
    }
}

} // namespace dice


// Required macro call
// That macro instantiate game contract entry point for given game class
GAME_CONTRACT(dice::dice)

