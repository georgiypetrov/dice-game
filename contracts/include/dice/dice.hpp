#pragma once

#include <game-contract-sdk/game_base.hpp>

namespace dice {

using eosio::name;
using eosio::asset;
using bytes = std::vector<char>;
using eosio::checksum256;


namespace constant
{
constexpr double all_range = 100.;  //< total range of possible dice numbers
constexpr double house_edge = 0.01; //< casino's house edge

constexpr uint16_t min_bet_param_type = 0;    //< type of game param that represent min bet amount
constexpr uint16_t max_bet_param_type = 1;    //< type of game param that represent max bet amount
constexpr uint16_t max_payout_param_type = 2; //< type of game param that represent max possible payout

constexpr uint8_t roll_action_type = 0;       //< type of "roll" game action
}

using dice_number_t = uint16_t;


// Main game contract class, should be extended from DAOPlatform game class
// More details: https://github.com/daocasino/game-contract-sdk
class [[eosio::contract]] dice: public game_sdk::game {
public:
    /**
       Storage types definition
       More info about storage: https://developers.eos.io/welcome/latest/getting-started/smart-contract-development/data-persistence
    */

    // Struct that represents single storage table row
    // Attribute used for ABI generation
    // More details: https://developers.eos.io/manuals/eosio.cdt/v1.6/guides/generator-attributes/#abicode-generator-attributes
    struct [[eosio::table("roll")]] roll_row {
        // Unique session ID, that ID provided by game SDK
        uint64_t ses_id;

        // Small number that chosen by player before rolling dice which is range start
        dice_number_t number_small;
        // Big number that chosen by player before rolling dice which is range end (excluded)
        dice_number_t number_big;

        // It's required method for table's row struct
        // That method result used as primary index of table
        uint64_t primary_key() const { return ses_id; }
    };

    // Rolls storage table definition
    using roll_table = eosio::multi_index<
        "roll"_n,   // table name
        roll_row    // table row type
    >;

public:
    // Required constructor, any game contract class should have ctor with that params
    dice(name receiver, name code, eosio::datastream<const char*> ds):
        game(receiver, code, ds), ///< base class initialization
        rolls(_self, _self.value) ///< initilization of "rolls" table
    { }


    /**
       Game life-cycle handlers, each handler invokes by game SDK
       More info about life-cycle: https://github.com/DaoCasino/game-contract-sdk/blob/master/README.md
    */

    // New game session handler (required, must be implemented)
    // Called once when new game session created
    void on_new_game(uint64_t ses_id) override final;

    // Game action handler (required, must be implemented)
    // Called every time when player made game action
    void on_action(uint64_t ses_id, uint16_t type, std::vector<game_sdk::param_t> params) override final;

    // Random handler (required, must be implemented)
    // Called every time when received requested random number by "require_random()" func
    void on_random(uint64_t ses_id, checksum256 rand) override final;

    // Game session ending handler (optional)
    // Called once when game session finished
    void on_finish(uint64_t ses_id) override final;

private:
    /**
       Internal functions realated to particular game logic
    */

    // Calculate and return payout amount for given session and player choise
    asset get_win_payout(uint64_t ses_id, dice_number_t number_small, dice_number_t number_big) const;

    // Check game session initial params
    void check_params(uint64_t ses_id) const;

    // Check player's bet amount
    void check_bet(uint64_t ses_id) const;

    // Check player's game action
    void check_action_params(const std::vector<game_sdk::param_t>& params) const;

private:
    // Calculate win coef for given player's choice
    static double get_win_coefficient(dice_number_t num_small, dice_number_t num_big);

private:
    // roll table object
    roll_table rolls;
};

} // namespace dice
