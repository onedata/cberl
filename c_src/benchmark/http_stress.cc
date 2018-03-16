/**
 * @file benchmark.cc
 * @author Bartek Kryza
 * @copyright (C) 2018
 * This software is released under the MIT license cited in 'LICENSE.md'
 */

#include "client.h"
#include "connection.h"
#include "requests/requests.h"
#include "responses/responses.h"

#include <chrono>
#include <future>
#include <iostream>
#include <memory>
#include <random>
#include <string>
#include <vector>

const std::string CONTENT(1024, 'x');

constexpr auto HTTP_METHOD_GET = 0;
constexpr auto HTTP_METHOD_POST = 1;
constexpr auto HTTP_METHOD_PUT = 2;
constexpr auto HTTP_METHOD_DELETE = 3;

// clang-format off
json templateDoc = {
  {"guid", "CHANGEME"},
  {"value", 0},
  {"happy", true},
  {"name", "cberl"},
  {"nothing", nullptr},
  {"answer", {
    {"everything", 42}
  }},
  {"blob", CONTENT},
  {"list", {1, 0, 2}},
  {"object", {
    {"currency", "USD"},
    {"value", 42.99}
  }}
};
// clang-format on

auto prepare(std::shared_ptr<cb::Client> client,
    std::shared_ptr<cb::Connection> connection, std::string prefix,
    int testSize)
{
    auto start = std::chrono::system_clock::now();


    for (int i = 0; i < testSize; i++) {

        auto body = jsonTemplate;
        body["guid"] = "http-stress-"+std::to_string(i);
        body["value"] = i;

        std::tuple<std::int32_t, std::int32_t, std::string, std::string,
            std::string>
            value{
                LCB_HTTP_TYPE_VIEW,
                HTTP_METHOD_PUT,
                "",
                "application/json",
                body.dump()

            };

        cb::HttpRequest request(value);

        auto p = std::make_shared<std::promise<cb::HttpResponse>>();
        auto f = p->get_future();

        client->get(connection, std::move(request),
            [p = std::move(p)](
                const cb::HttpResponse &response) mutable {
                p->set_value(response);
            });

        f.get();
    }
    auto end = std::chrono::system_clock::now();
    auto elapsed =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    return elapsed;
}



auto viewGET(std::shared_ptr<cb::Client> client,
    std::shared_ptr<cb::Connection> connection, std::string prefix,
    int batchSize)
{
    auto start = std::chrono::system_clock::now();

    for (int i = 0; i < batchSize; i++) {
        std::tuple<std::int32_t, std::int32_t, std::string, std::string,
            std::string>
            value{
                LCB_HTTP_TYPE_VIEW,
                HTTP_METHOD_GET,
            };

        cb::HttpRequest request(value);

        auto p = std::make_shared<std::promise<cb::HttpResponse>>();
        auto f = p->get_future();

        client->get(connection, std::move(request),
            [p = std::move(p)](
                const cb::HttpResponse &response) mutable {
                p->set_value(response);
            });

        f.get();
    }
    auto end = std::chrono::system_clock::now();
    auto elapsed =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    return elapsed;
}

/**
 * cberl Couchbase benchmark.
 *
 * Usage:
 *  ./cberl_benchmark <couch_host> <username> <password> <bucket> \
 *                    <thread_count> <batch_size>
 *
 */
int main(int argc, char *argv[])
{
    auto host = "localhost:8091";
    auto user = "";
    auto password = "";
    auto bucket = "default";
    auto workerCount = 10;
    auto batchSize = 1200;

    if (argc > 1) {
        host = argv[1];
    }
    if (argc > 2) {
        user = argv[2];
    }
    if (argc > 3) {
        password = argv[3];
    }
    if (argc > 4) {
        bucket = argv[4];
    }
    if (argc > 5) {
        workerCount = std::stoi(argv[5]);
    }
    if (argc > 6) {
        batchSize = std::stoi(argv[6]);
    }

    auto client = std::make_shared<cb::Client>();

    cb::ConnectRequest request{host, user, password, bucket, {}};

    auto p = std::make_shared<std::promise<cb::ConnectResponse>>();
    auto connectFuture = p->get_future();

    client->connect(std::move(request),
        [p = std::move(p)](const cb::ConnectResponse &response) mutable {
            p->set_value(response);
        });

    auto connectResponse = connectFuture.get();

    if (connectResponse.error() != LCB_SUCCESS) {
        std::cout << "Cannot connect to Couchbase Server - exiting..."
                  << std::endl;
        return 1;
    }

    std::cout << "Connected to Couchbase Server - starting benchmark..."
              << std::endl;

    auto connection = connectResponse.connection();

    auto documentViewWorker = [&](std::string prefix) {
        std::vector<uint32_t> storeTimes;

        while (true) {
            auto viewTime = viewBatch(client, connection, prefix, batchSize);

            // std::cout << prefix << " -- " << repeat << std::endl;
        }

    };

    auto documentUpdateWorker = [&](std::string prefix) {
        std::vector<uint32_t> storeTimes;

        while (true) {
            auto storeTime = storeBatch(client, connection, prefix, batchSize);

            // std::cout << prefix << " -- " << repeat << std::endl;
        }

    };

    std::vector<std::thread> workers;
    for (int i = 0; i < workerCount; i++) {
        workers.emplace_back(std::thread(benchmarkWorker, std::to_string(i)));
    }

    for (auto &worker : workers) {
        worker.join();
    }
}
