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
    auto batchSize = 100;

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

    auto benchmarkWorker = [&](std::string prefix) {
        cb::Client client;

        cb::ConnectRequest request{host, user, password, bucket, {}};

        auto connectFuture = client.connect(std::move(request));

        auto connectResponse = connectFuture.get();

        std::shared_ptr<cb::Connection> connection =
            connectResponse.connection();

        while (1) {

            auto start = std::chrono::system_clock::now();

            std::vector<std::future<cb::MultiResponse<cb::StoreResponse>>>
                storeFutures;
            storeFutures.reserve(batchSize);

            for (int i = 0; i < batchSize; i++) {
                std::tuple<int, std::string, std::string, lcb_uint32_t,
                    lcb_cas_t, lcb_time_t>
                    value{0, prefix + std::to_string(i), "CONTENT", 0, 0, 0};

                std::vector<cb::StoreRequest::Raw> reqs;
                reqs.push_back(std::move(value));

                cb::MultiRequest<cb::StoreRequest> storeRequest(reqs);

                assert(connection);

                storeFutures.push_back(
                    client.store(connection, std::move(storeRequest)));
            }

            for (auto &f : storeFutures) {
                f.get();
            }

            auto end = std::chrono::system_clock::now();
            auto elapsed =
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    end - start);

            start = std::chrono::system_clock::now();

            std::vector<std::future<cb::MultiResponse<cb::GetResponse>>>
                getFutures;
            getFutures.reserve(batchSize);

            for (int i = 0; i < batchSize; i++) {
                std::tuple<std::string, lcb_time_t, bool> value{
                    prefix + std::to_string(i), 0, false};

                std::vector<cb::GetRequest::Raw> reqs;
                reqs.push_back(std::move(value));

                cb::MultiRequest<cb::GetRequest> getRequest(reqs);

                assert(connection);

                getFutures.push_back(
                    client.get(connection, std::move(getRequest)));
            }

            for (auto &f : getFutures) {
                f.get();
            }

            end = std::chrono::system_clock::now();
            elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                end - start);

            start = std::chrono::system_clock::now();

            std::vector<std::future<cb::MultiResponse<cb::RemoveResponse>>>
                removeFutures;
            removeFutures.reserve(batchSize);

            for (int i = 0; i < batchSize; i++) {
                std::tuple<std::string, lcb_cas_t> value{
                    prefix + std::to_string(i), 0};

                std::vector<cb::RemoveRequest::Raw> reqs;
                reqs.push_back(std::move(value));

                cb::MultiRequest<cb::RemoveRequest> removeRequest(reqs);

                assert(connection);

                removeFutures.push_back(
                    client.remove(connection, std::move(removeRequest)));
            }

            for (auto &f : removeFutures) {
                f.get();
            }

            end = std::chrono::system_clock::now();
            elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                end - start);

            /*
             *std::cout << prefix << ": Deleted " << batchSize << " documents in
             *"
             *          << elapsed.count() << " [ms]\n";
             */
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
