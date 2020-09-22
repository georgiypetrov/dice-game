#include <game_tester/game_tester.hpp>
#include <game_tester/strategy.hpp>

#include "contracts.hpp"

namespace testing {

constexpr uint8_t MAKE_BET_ACTION = 0;

class dice_tester: public game_tester {
public:
    static const name game_name;
    static constexpr uint32_t default_min_bet = 1;
    static constexpr uint32_t default_max_bet = 10;
    static constexpr uint32_t default_max_payout = 20;

    static constexpr double all_range = 100;
    static constexpr double house_edge = 0.01;

public:
    dice_tester() {
        create_account(game_name);

        game_params_type game_params = {
            {0, default_min_bet * 10000},
            {1, default_max_bet * 10000},
            {2, default_max_payout * 10000}
        };

        deploy_game<dice_game>(game_name, game_params);
    }

    double get_rtp(uint64_t run_count, std::function<uint32_t()> && bet_generator) {
        const auto player_name = N(player);
        create_player(player_name);
        link_game(player_name, game_name);

        const auto to_double = [](const asset& value) -> double {
            return double(value.get_amount()) / value.precision();
        };

        // Init balance for player and casino
        transfer(N(eosio), player_name, STRSYM("1000000.0000"));
        transfer(N(eosio), casino_name, STRSYM("1000000.0000"));

        const double start_player_balance = to_double(get_balance(player_name));

        auto graph = strategy::Graph([&](game_tester & tester, const uint32_t session_id) {
            tester.game_action(game_name, session_id, 0, { bet_generator() });
            return strategy::Result::End;
        });

        auto executor = strategy::Executor(std::move(graph));

        const uint32_t bet = 1;
        executor.process_strategy(
            *this, run_count, 10,
            [](game_tester & tester, const uint run) {
                return tester.new_game_session(game_name, player_name, casino_id, STRSYM("1.0000"));
            },
            [&](game_tester & tester, const uint32_t session_id) {
                tester.signidice(game_name, session_id);
            }
        );

        const auto end_player_balance = to_double(get_balance(player_name));
        const double all_bets_balance = double(run_count) * bet;

        return ((end_player_balance - start_player_balance) / all_bets_balance) + 1;
    }
};

const name dice_tester::game_name = N(dicegame);


BOOST_AUTO_TEST_SUITE(dice_tests)

BOOST_FIXTURE_TEST_CASE(new_session_test, dice_tester) try {
    auto player_name = N(player);

    create_player(player_name);
    link_game(player_name, game_name);

    transfer(N(eosio), player_name, STRSYM("10.0000"));

    auto ses_id = new_game_session(game_name, player_name, casino_id, STRSYM("5.0000"));

    auto session = get_game_session(game_name, ses_id);

    BOOST_REQUIRE_EQUAL(session["req_id"].as<uint64_t>(), ses_id);
    BOOST_REQUIRE_EQUAL(session["casino_id"].as<uint64_t>(), casino_id);
    BOOST_REQUIRE_EQUAL(session["ses_seq"].as<uint64_t>(), 0);
    BOOST_REQUIRE_EQUAL(session["player"].as<name>(), player_name);
    BOOST_REQUIRE_EQUAL(session["state"].as<uint32_t>(), 2); // req_action state
    BOOST_REQUIRE_EQUAL(session["deposit"].as<asset>(), STRSYM("5.0000"));

} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(max_win_min_test, dice_tester) try {
    auto player_name = N(player);

    create_player(player_name);
    link_game(player_name, game_name);

    transfer(N(eosio), player_name, STRSYM("10.0000"));

    auto ses_id = new_game_session(game_name, player_name, casino_id, STRSYM("5.0000"));

    const auto bet_num_small = 1;
    const auto bet_num_big = 2;
    game_action(game_name, ses_id, MAKE_BET_ACTION, { bet_num_small, bet_num_big });

    auto session = get_game_session(game_name, ses_id);
    BOOST_REQUIRE_EQUAL(session["last_max_win"].as<asset>(), STRSYM("0.0000"));

} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(max_win_max_test, dice_tester) try {
    auto player_name = N(player);

    create_player(player_name);
    link_game(player_name, game_name);

    transfer(N(eosio), player_name, STRSYM("10.0000"));

    const auto ses_id = new_game_session(game_name, player_name, casino_id, STRSYM("5.0000"));

    const auto bet_num_small = 1;
    const auto bet_num_big = 100;
    game_action(game_name, ses_id, MAKE_BET_ACTION, { bet_num_small, bet_num_big });

    auto session = get_game_session(game_name, ses_id);
    BOOST_REQUIRE_EQUAL(session["last_max_win"].as<asset>(), STRSYM("15.0000"));

} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(max_win_normal_test, dice_tester) try {
    auto player_name = N(player);

    create_player(player_name);
    link_game(player_name, game_name);
    transfer(N(eosio), player_name, STRSYM("24255.0000")); ///< (97 + 96 + 95 ... + 1) * 5 = 98 * 97 / 2 * 5 = 24255

    const auto core_symbol = symbol(CORE_SYM);

    const auto deposit = 5.0;
    const auto max_payout = default_max_payout - deposit;
    const auto precision = asset(0.001 * core_symbol.precision(), core_symbol);

    for (unsigned int bet_num_small = 2; bet_num_small <= 99; ++bet_num_small)
    {
        for (unsigned int bet_num_big = bet_num_small + 1; bet_num_big <= 100; ++bet_num_big) {
            const auto ses_id = new_game_session(game_name, player_name, casino_id, STRSYM("5.0000"));
            game_action(game_name, ses_id, MAKE_BET_ACTION, { bet_num_small, bet_num_big });

            auto expected_max_win = deposit * ((all_range * (1. - house_edge)) / (all_range - (bet_num_big - bet_num_small)) - 1.);
            expected_max_win = expected_max_win < max_payout ? expected_max_win : max_payout;

            const auto session = get_game_session(game_name, ses_id);
            BOOST_CHECK(
                    session["last_max_win"].as<asset>()
            - asset(expected_max_win * core_symbol.precision(), core_symbol) <
            precision
            );
        }
    }

} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(full_session_success_test, dice_tester) try {
    auto player_name = N(player);

    create_player(player_name);
    link_game(player_name, game_name);

    transfer(N(eosio), player_name, STRSYM("10.0000"));
    transfer(N(eosio), casino_name, STRSYM("1000.0000"));

    auto casino_balance_before = get_balance(casino_name);
    auto player_balance_before = get_balance(player_name);

    auto ses_id = new_game_session(game_name, player_name, casino_id, STRSYM("5.0000"));

    BOOST_REQUIRE_EQUAL(get_balance(game_name), STRSYM("5.0000"));

    const auto bet_num_small = 1;
    const auto bet_num_big = 91;
    game_action(game_name, ses_id, MAKE_BET_ACTION, { bet_num_small, bet_num_big });

    auto session = get_game_session(game_name, ses_id);
    BOOST_REQUIRE_EQUAL(session["state"].as<uint32_t>(), 3); // req_signidice_part_1 state

    signidice(game_name, ses_id);

    auto casino_balance_after = get_balance(casino_name);
    auto player_balance_after = get_balance(player_name);

    session = get_game_session(game_name, ses_id);
    BOOST_REQUIRE_EQUAL(session.is_null(), true);
    BOOST_REQUIRE_EQUAL(casino_balance_before + player_balance_before, casino_balance_after + player_balance_after);

} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(session_expiration_test, dice_tester) try {
    auto player_name = N(player);

    create_player(player_name);
    link_game(player_name, game_name);

    transfer(N(eosio), player_name, STRSYM("10.0000"));
    transfer(N(eosio), casino_name, STRSYM("1000.0000"));

    auto player_balance_before = get_balance(player_name);

    auto ses_id = new_game_session(game_name, player_name, casino_id, STRSYM("5.0000"));
    const auto bet_num_small = 1;
    const auto bet_num_big = 11;
    game_action(game_name, ses_id, MAKE_BET_ACTION, { bet_num_small, bet_num_big });

    BOOST_REQUIRE_EQUAL(get_balance(game_name), STRSYM("5.0000"));

    BOOST_REQUIRE_EQUAL(push_action(game_name, N(close), { platform_name, N(active) }, mvo()
        ("req_id", ses_id)
    ), wasm_assert_msg("session isn't expired, only expired session can be closed"));

    produce_block(fc::seconds(game_session_ttl + 1));

    close_session(game_name, ses_id);

    auto session = get_game_session(game_name, ses_id);
    BOOST_REQUIRE_EQUAL(session.is_null(), true);

    auto player_balance_after = get_balance(player_name);
    BOOST_REQUIRE_EQUAL(player_balance_before, player_balance_after);

} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(new_session_bad_auth_test, dice_tester) try {
    auto player_name = N(player);

    create_player(player_name);
    link_game(player_name, game_name);

    transfer(N(eosio), player_name, STRSYM("10.0000"));
    transfer(N(eosio), casino_name, STRSYM("1000.0000"));

    auto ses_id = 0u;
    transfer(player_name, game_name, STRSYM("5.0000"), std::to_string(ses_id));

    // clang-format off
    BOOST_TEST_REQUIRE(
        push_action(
            game_name,
            N(newgame),
            { player_name, N(game) },
            { casino_name, N(active) },
            mvo()
                ("req_id", ses_id)
                ("casino_id", casino_id)
    ).find("but does not have signatures for it") != std::string::npos);

    BOOST_REQUIRE_EQUAL(
        push_action(
            game_name,
            N(newgame),
            { casino_name, N(active) },
            { casino_name, N(active) },
            mvo()
                ("req_id", ses_id)
                ("casino_id", casino_id)
        ), "missing authority of platform/gameaction");
    // clang-format on

} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(game_action_bad_auth_test, dice_tester) try {
    auto player_name = N(player);

    create_player(player_name);
    link_game(player_name, game_name);

    transfer(N(eosio), player_name, STRSYM("10.0000"));
    transfer(N(eosio), casino_name, STRSYM("1000.0000"));

    auto ses_id = new_game_session(game_name, player_name, casino_id, STRSYM("5.0000"));

    BOOST_TEST_REQUIRE(push_action(game_name, N(gameaction), { player_name, N(game) }, { casino_name, N(active) }, mvo()
        ("req_id", ses_id)
        ("type", 0)
        ("params", std::vector<uint32_t> { 1, 31 })
    ).find("but does not have signatures for it") != std::string::npos);

    BOOST_REQUIRE_EQUAL(push_action(game_name, N(gameaction), { casino_name, N(active) }, { casino_name, N(active) }, mvo()
        ("req_id", ses_id)
        ("type", 0)
        ("params", std::vector<uint32_t> { 1, 31 })
    ), "missing authority of platform/gameaction");

} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(deposit_bad_sender_test, dice_tester) try {
    auto player_name = N(player);

    create_player(player_name);
    link_game(player_name, game_name);

    transfer(N(eosio), player_name, STRSYM("10.0000"));
    transfer(N(eosio), casino_name, STRSYM("1000.0000"));

    auto ses_id = new_game_session(game_name, player_name, casino_id, STRSYM("5.0000"));
    const auto bet_num_small = 1;
    const auto bet_num_big = 31;
    game_action(game_name, ses_id, MAKE_BET_ACTION, { bet_num_small, bet_num_big });

    BOOST_REQUIRE_EQUAL(transfer(casino_name, game_name, STRSYM("5.0000"), std::to_string(ses_id)),
        wasm_assert_msg("state should be 'req_deposit' or 'req_action'")
    );

} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(deposit_bad_state_test, dice_tester) try {
    auto player_name = N(player);

    create_player(player_name);
    link_game(player_name, game_name);

    transfer(N(eosio), player_name, STRSYM("10.0000"));
    transfer(N(eosio), casino_name, STRSYM("1000.0000"));

    auto ses_id = new_game_session(game_name, player_name, casino_id, STRSYM("5.0000"));

    const auto bet_num_small = 1;
    const auto bet_num_big = 31;
    game_action(game_name, ses_id, MAKE_BET_ACTION, { bet_num_small, bet_num_big });

    BOOST_REQUIRE_EQUAL(transfer(player_name, game_name, STRSYM("5.0000"), std::to_string(ses_id)),
        wasm_assert_msg("state should be 'req_deposit' or 'req_action'")
    );

    auto digest = get_game_session(game_name, ses_id)["digest"].as<sha256>();
    auto sign_1 = rsa_sign(rsa_keys.at(platform_name), digest);
    BOOST_REQUIRE_EQUAL(push_action(game_name, N(sgdicefirst), { service_name, N(active) }, mvo()
        ("req_id", ses_id)
        ("sign", sign_1)
    ), success());

    BOOST_REQUIRE_EQUAL(transfer(player_name, game_name, STRSYM("5.0000"), std::to_string(ses_id)),
        wasm_assert_msg("state should be 'req_deposit' or 'req_action'")
    );
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(game_action_bad_state_test, dice_tester) try {
    auto player_name = N(player);

    create_player(player_name);
    link_game(player_name, game_name);

    transfer(N(eosio), player_name, STRSYM("10.0000"));
    transfer(N(eosio), casino_name, STRSYM("1000.0000"));

    auto ses_id = new_game_session(game_name, player_name, casino_id, STRSYM("5.0000"));

    const auto bet_num_small = 1;
    const auto bet_num_big = 31;
    game_action(game_name, ses_id, MAKE_BET_ACTION, { bet_num_small, bet_num_big });

    BOOST_REQUIRE_EQUAL(
        push_action(
            game_name,
            N(gameaction),
            { platform_name, N(gameaction) },
            mvo()
                ("req_id", ses_id)
                ("type", 0)
                ("params", std::vector<uint32_t> { 1, 31 })
        ), wasm_assert_msg("state should be 'req_deposit' or 'req_action'"));

    auto digest = get_game_session(game_name, ses_id)["digest"].as<sha256>();
    auto sign_1 = rsa_sign(rsa_keys.at(platform_name), digest);
    BOOST_REQUIRE_EQUAL(push_action(game_name, N(sgdicefirst), { service_name, N(active) }, mvo()
        ("req_id", ses_id)
        ("sign", sign_1)
    ), success());

    // clang-format off
    BOOST_REQUIRE_EQUAL(
        push_action(
            game_name,
            N(gameaction),
            { platform_name, N(gameaction) },
            mvo()
                ("req_id", ses_id)
                ("type", 0)
                ("params", std::vector<uint32_t> { 1, 31 })
        ), wasm_assert_msg("state should be 'req_deposit' or 'req_action'"));
    // clang-format on

} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(signidice_1_bad_state_test, dice_tester) try {
    auto player_name = N(player);

    create_player(player_name);
    link_game(player_name, game_name);

    transfer(N(eosio), player_name, STRSYM("10.0000"));
    transfer(N(eosio), casino_name, STRSYM("1000.0000"));

    auto ses_id = new_game_session(game_name, player_name, casino_id, STRSYM("5.0000"));

    auto digest = get_game_session(game_name, ses_id)["digest"].as<sha256>();
    auto sign_1 = rsa_sign(rsa_keys.at(platform_name), digest);
    BOOST_REQUIRE_EQUAL(push_action(game_name, N(sgdicefirst), { service_name, N(active) }, mvo()
        ("req_id", ses_id)
        ("sign", sign_1)
    ), wasm_assert_msg("state should be 'req_signidice_part_1'"));

    const auto bet_num_small = 1;
    const auto bet_num_big = 31;
    game_action(game_name, ses_id, MAKE_BET_ACTION, { bet_num_small, bet_num_big });

    BOOST_REQUIRE_EQUAL(push_action(game_name, N(sgdicefirst), { service_name, N(active) }, mvo()
        ("req_id", ses_id)
        ("sign", sign_1)
    ), success());

    BOOST_REQUIRE_EQUAL(push_action(game_name, N(sgdicefirst), { service_name, N(active) }, mvo()
        ("req_id", ses_id)
        ("sign", sign_1)
    ), wasm_assert_msg("state should be 'req_signidice_part_1'"));

} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(signidice_2_bad_state_test, dice_tester) try {
    auto player_name = N(player);

    create_player(player_name);
    link_game(player_name, game_name);

    transfer(N(eosio), player_name, STRSYM("10.0000"));
    transfer(N(eosio), casino_name, STRSYM("1000.0000"));

    auto ses_id = new_game_session(game_name, player_name, casino_id, STRSYM("5.0000"));

    auto digest = get_game_session(game_name, ses_id)["digest"].as<sha256>();
    auto sign = rsa_sign(rsa_keys.at(platform_name), digest);
    BOOST_REQUIRE_EQUAL(push_action(game_name, N(sgdicesecond), { service_name, N(active) }, mvo()
        ("req_id", ses_id)
        ("sign", sign)
    ), wasm_assert_msg("state should be 'req_signidice_part_2'"));

    const auto bet_num_small = 1;
    const auto bet_num_big = 31;
    game_action(game_name, ses_id, MAKE_BET_ACTION, { bet_num_small, bet_num_big });

    BOOST_REQUIRE_EQUAL(push_action(game_name, N(sgdicesecond), { service_name, N(active) }, mvo()
        ("req_id", ses_id)
        ("sign", sign)
    ), wasm_assert_msg("state should be 'req_signidice_part_2'"));

} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(dice_rtp_test, dice_tester, *boost::unit_test::disabled()) try {
    BOOST_TEST(get_rtp(1000000, [](){ return 1 + (rand() % 99); }) == 0.978, boost::test_tools::tolerance(0.015));
} FC_LOG_AND_RETHROW()

BOOST_AUTO_TEST_SUITE_END()

} // namespace testing

