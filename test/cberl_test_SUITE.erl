%%%-------------------------------------------------------------------
%%% @author Krzysztof Trzepla
%%% @copyright (C) 2017: Krzysztof Trzepla
%%% This software is released under the MIT license cited in 'LICENSE.md'.
%%% @end
%%%-------------------------------------------------------------------
%%% @doc
%%% This file contains cberl tests.
%%% @end
%%%-------------------------------------------------------------------
-module(cberl_test_SUITE).
-author("Krzysztof Trzepla").

-include_lib("common_test/include/ct.hrl").

%% export for ct
-export([all/0, init_per_testcase/2]).

%% tests
-export([
    store_test/1,
    bulk_store_test/1,
    get_test/1,
    bulk_get_test/1,
    remove_test/1,
    bulk_remove_test/1,
    arithmetic_test/1,
    bulk_arithmetic_test/1,
    durability_test/1,
    bulk_durability_test/1,
    http_test/1
]).

all() -> [
    store_test,
    bulk_store_test,
    get_test,
    bulk_get_test,
    remove_test,
    bulk_remove_test,
    arithmetic_test,
    bulk_arithmetic_test,
    durability_test,
    bulk_durability_test,
    http_test
].

-define(TIMEOUT, timer:seconds(5)).

%%%===================================================================
%%% Test functions
%%%===================================================================

store_test(Config) ->
    C = ?config(connection, Config),
    lists:foreach(fun({Key, Value, Encoder}) ->
        {ok, _} = cberl:store(C, set, Key, Value, Encoder, 0, 0, ?TIMEOUT)
    end, [
        {<<"k1">>, <<"v1">>, none},
        {<<"k2">>, {[{<<"k2">>, <<"v2">>}]}, json},
        {<<"k3">>, v3, raw}
    ]).

bulk_store_test(Config) ->
    C = ?config(connection, Config),
    {ok, [
        {<<"k1">>, {ok, _}},
        {<<"k2">>, {ok, _}},
        {<<"k3">>, {ok, _}}
    ]} = cberl:bulk_store(C, [
        {set, <<"k1">>, <<"v1">>, none, 0, 0},
        {set, <<"k2">>, {[{<<"k2">>, <<"v2">>}]}, json, 0, 0},
        {set, <<"k3">>, v3, raw, 0, 0}
    ], ?TIMEOUT).

get_test(Config) ->
    C = ?config(connection, Config),
    lists:foreach(fun({Key, Value, Encoder}) ->
        {ok, Cas} = cberl:store(C, set, Key, Value, Encoder, 0, 0, ?TIMEOUT),
        {ok, Cas, Value} = cberl:get(C, Key, 0, false, ?TIMEOUT)
    end, [
        {<<"k1">>, <<"v1">>, none},
        {<<"k2">>, {[{<<"k2">>, <<"v2">>}]}, json},
        {<<"k3">>, v3, raw}
    ]).

bulk_get_test(Config) ->
    C = ?config(connection, Config),
    Value1 = <<"v1">>,
    Value2 = {[{<<"k2">>, <<"v2">>}]},
    Value3 = v3,
    {ok, [
        {<<"k1">>, {ok, Cas1}},
        {<<"k2">>, {ok, Cas2}},
        {<<"k3">>, {ok, Cas3}}
    ]} = cberl:bulk_store(C, [
        {set, <<"k1">>, Value1, none, 0, 0},
        {set, <<"k2">>, Value2, json, 0, 0},
        {set, <<"k3">>, Value3, raw, 0, 0}
    ], ?TIMEOUT),
    {ok, [
        {<<"k1">>, {ok, Cas1, Value1}},
        {<<"k2">>, {ok, Cas2, Value2}},
        {<<"k3">>, {ok, Cas3, Value3}}
    ]} = cberl:bulk_get(C, [
        {<<"k1">>, 0, false},
        {<<"k2">>, 0, false},
        {<<"k3">>, 0, false}
    ], ?TIMEOUT).

remove_test(Config) ->
    C = ?config(connection, Config),
    Key = <<"k">>,
    {ok, _} = cberl:store(C, set, Key, <<"v">>, none, 0, 0, ?TIMEOUT),
    ok = cberl:remove(C, Key, 0, ?TIMEOUT).

bulk_remove_test(Config) ->
    C = ?config(connection, Config),
    {ok, [
        {<<"k1">>, {ok, _}},
        {<<"k2">>, {ok, _}},
        {<<"k3">>, {ok, _}}
    ]} = cberl:bulk_store(C, [
        {set, <<"k1">>, <<"v1">>, none, 0, 0},
        {set, <<"k2">>, {[{<<"k2">>, <<"v2">>}]}, json, 0, 0},
        {set, <<"k3">>, v3, raw, 0, 0}
    ], ?TIMEOUT),
    {ok, [
        {<<"k1">>, ok},
        {<<"k2">>, ok},
        {<<"k3">>, ok}
    ]} = cberl:bulk_remove(C, [
        {<<"k1">>, 0},
        {<<"k2">>, 0},
        {<<"k3">>, 0}
    ], ?TIMEOUT).

arithmetic_test(Config) ->
    C = ?config(connection, Config),
    cberl:remove(C, <<"k4">>, 0, ?TIMEOUT),
    lists:foreach(fun(N) ->
        {ok, _, N} = cberl:arithmetic(C, <<"k4">>, 1, 0, 0, ?TIMEOUT)
    end, lists:seq(0, 9)).

bulk_arithmetic_test(Config) ->
    C = ?config(connection, Config),
    cberl:remove(C, <<"k5">>, 0, ?TIMEOUT),
    cberl:remove(C, <<"k6">>, 0, ?TIMEOUT),
    {ok, [
        {<<"k5">>, {ok, _, 1}},
        {<<"k6">>, {ok, _, 2}}
    ]} = cberl:bulk_arithmetic(C, [
        {<<"k5">>, 1, 1, 0},
        {<<"k6">>, 1, 2, 0}
    ], ?TIMEOUT).

http_test(Config) ->
    C = ?config(connection, Config),
    NonExistentViewPath = <<"_design/dev_example/_view/nothinghere">>,
    {ok, 404, _} = cberl:http(C, view, get, NonExistentViewPath,
                              <<"application/json">>, <<>>, ?TIMEOUT).

durability_test(Config) ->
    C = ?config(connection, Config),
    lists:foreach(fun({Key, Value, Encoder}) ->
        {ok, Cas} = cberl:store(C, set, Key, Value, Encoder, 0, 0, ?TIMEOUT),
        {ok, Cas} = cberl:durability(C, Key, 0, 1, -1, ?TIMEOUT)
    end, [
        {<<"k1">>, <<"v1">>, none},
        {<<"k2">>, {[{<<"k2">>, <<"v2">>}]}, json},
        {<<"k3">>, v3, raw}
    ]).

bulk_durability_test(Config) ->
    C = ?config(connection, Config),
    {ok, [
        {K1, {ok, _}},
        {K2, {ok, _}},
        {K3, {ok, _}},
        {K4, {ok, _}},
        {K5, {ok, _}},
        {K6, {ok, _}},
        {K7, {ok, _}},
        {K8, {ok, _}},
        {K9, {ok, _}},
        {K10, {ok, _}}
    ]} = cberl:bulk_store(C, [
        {set, <<"k1">>, <<"v1">>, none, 0, 0},
        {set, <<"k2">>, {[{<<"k2">>, <<"v2">>}]}, json, 0, 0},
        {set, <<"k3">>, v3, raw, 0, 0},
        {set, <<"k4">>, v3, raw, 0, 0},
        {set, <<"k5">>, v3, raw, 0, 0},
        {set, <<"k6">>, v3, raw, 0, 0},
        {set, <<"k7">>, v3, raw, 0, 0},
        {set, <<"k8">>, v3, raw, 0, 0},
        {set, <<"k9">>, v3, raw, 0, 0},
        {set, <<"k10">>, v3, raw, 0, 0}
    ], ?TIMEOUT),
    StoredKeys = [K1, K2, K3, K4, K5, K6, K7, K8, K9, K10],
    {ok, [
        {S1, {ok, _}},
        {S2, {ok, _}},
        {S3, {ok, _}},
        {S4, {ok, _}},
        {S5, {ok, _}},
        {S6, {ok, _}},
        {S7, {ok, _}},
        {S8, {ok, _}},
        {S9, {ok, _}},
        {S10, {ok, _}}
    ]} = cberl:bulk_durability(C, [
        {<<"k1">>, 0},
        {<<"k2">>, 0},
        {<<"k3">>, 0},
        {<<"k4">>, 0},
        {<<"k5">>, 0},
        {<<"k6">>, 0},
        {<<"k7">>, 0},
        {<<"k8">>, 0},
        {<<"k9">>, 0},
        {<<"k10">>, 0}
    ], {1, -1}, ?TIMEOUT),
    RetrievedKeys = [S1, S2, S3, S4, S5, S6, S7, S8, S9, S10],
    lists:sort(StoredKeys) =:= lists:sort(RetrievedKeys).

%%%===================================================================
%%% Init/teardown functions
%%%===================================================================

init_per_testcase(_Case, Config) ->
    Host = proplists:get_value(host, Config, <<"192.168.64.11">>),
    Username = proplists:get_value(username, Config, <<>>),
    Password = proplists:get_value(password, Config, <<>>),
    Bucket = proplists:get_value(bucket, Config, <<"default">>),
    Opts = [
        {operation_timeout, 5000000},
        {config_total_timeout, 5000000},
        {view_timeout, 60000000},
        {durability_interval, 10000},
        {durability_timeout, 30000000},
        {http_timeout, 10000000}
    ],
    {ok, C} = cberl:connect(Host, Username, Password, Bucket, Opts, ?TIMEOUT),
    [{connection, C} | Config].
