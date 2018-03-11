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

auto getBatch(std::shared_ptr<cb::Client> client,
    std::shared_ptr<cb::Connection> connection, std::string prefix,
    int batchSize)
{
    auto start = std::chrono::system_clock::now();

    std::vector<cb::GetRequest::Raw> reqs;

    for (int i = 0; i < batchSize; i++) {
        std::tuple<std::string, lcb_time_t, bool> value{
            prefix + std::to_string(i), 0, false};

        reqs.push_back(std::move(value));
    }

    cb::MultiRequest<cb::GetRequest> getRequest(std::move(reqs));

    auto p =
        std::make_shared<std::promise<cb::MultiResponse<cb::GetResponse>>>();
    auto f = p->get_future();

    client->get(connection, std::move(getRequest),
        [p = std::move(p)](
            const cb::MultiResponse<cb::GetResponse> &response) mutable {
            p->set_value(response);
        });

    f.get();

    auto end = std::chrono::system_clock::now();
    auto elapsed =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    return elapsed;
}

auto storeBatch(std::shared_ptr<cb::Client> client,
    std::shared_ptr<cb::Connection> connection, std::string prefix,
    int batchSize)
{
    auto start = std::chrono::system_clock::now();

    std::vector<cb::StoreRequest::Raw> reqs;

    for (int i = 0; i < batchSize; i++) {
        std::tuple<int, std::string, std::string, lcb_uint32_t, lcb_cas_t,
            lcb_time_t>
            value{0, prefix + std::to_string(i), CONTENT, 0, 0, 0};

        reqs.push_back(std::move(value));
    }

    cb::MultiRequest<cb::StoreRequest> storeRequest(reqs);

    auto p =
        std::make_shared<std::promise<cb::MultiResponse<cb::StoreResponse>>>();
    auto f = p->get_future();

    client->store(connection, std::move(storeRequest),
        [p = std::move(p)](
            const cb::MultiResponse<cb::StoreResponse> &response) mutable {
            p->set_value(response);
        });

    f.get();

    auto end = std::chrono::system_clock::now();
    auto elapsed =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    return elapsed;
}

auto removeBatch(std::shared_ptr<cb::Client> client,
    std::shared_ptr<cb::Connection> connection, std::string prefix,
    int batchSize)
{
    auto start = std::chrono::system_clock::now();

    std::vector<std::future<cb::MultiResponse<cb::RemoveResponse>>>
        removeFutures;

    std::vector<cb::RemoveRequest::Raw> reqs;

    for (int i = 0; i < batchSize; i++) {
        std::tuple<std::string, lcb_cas_t> value{prefix + std::to_string(i), 0};

        reqs.push_back(std::move(value));
    }

    cb::MultiRequest<cb::RemoveRequest> removeRequest(reqs);

    auto p =
        std::make_shared<std::promise<cb::MultiResponse<cb::RemoveResponse>>>();
    auto f = p->get_future();

    client->remove(connection, std::move(removeRequest),
        [p = std::move(p)](
            const cb::MultiResponse<cb::RemoveResponse> &response) mutable {
            p->set_value(response);
        });

    f.get();

    auto end = std::chrono::system_clock::now();
    auto elapsed =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    return elapsed;
}

auto durabilityBatch(std::shared_ptr<cb::Client> client,
    std::shared_ptr<cb::Connection> connection, std::string prefix,
    int batchSize)
{
    auto start = std::chrono::system_clock::now();

    std::vector<std::future<cb::MultiResponse<cb::DurabilityResponse>>>
        durabilityFutures;

    std::vector<cb::DurabilityRequest::Raw> reqs;

    for (int i = 0; i < batchSize; i++) {
        std::tuple<std::string, lcb_cas_t> value{prefix + std::to_string(i), 0};

        reqs.push_back(std::move(value));
    }

    cb::MultiRequest<cb::DurabilityRequest> durabilityRequest(reqs);
    std::tuple<std::int32_t, std::int32_t> dopts = std::make_tuple(1, 1);

    auto p = std::make_shared<
        std::promise<cb::MultiResponse<cb::DurabilityResponse>>>();
    auto f = p->get_future();

    client->durability(connection, std::move(durabilityRequest), dopts,
        [p = std::move(p)](
            const cb::MultiResponse<cb::DurabilityResponse> &response) mutable {
            p->set_value(response);
        });

    f.get();

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

    auto benchmarkWorker = [&](std::string prefix) {
        std::vector<uint32_t> getEmptyTimes;
        std::vector<uint32_t> storeTimes;
        std::vector<uint32_t> getTimes;
        std::vector<uint32_t> durabilityTimes;
        std::vector<uint32_t> removeTimes;

        int repeat = 10;

        while (repeat--) {
            auto getEmptyTime = getBatch(client, connection, prefix, batchSize);
            auto storeTime = storeBatch(client, connection, prefix, batchSize);
            auto getTime = getBatch(client, connection, prefix, batchSize);
            auto durabilityTime =
                durabilityBatch(client, connection, prefix, batchSize);
            auto removeTime =
                removeBatch(client, connection, prefix, batchSize);

            getEmptyTimes.push_back(getEmptyTime.count());
            storeTimes.push_back(storeTime.count());
            getTimes.push_back(getTime.count());
            durabilityTimes.push_back(durabilityTime.count());
            removeTimes.push_back(removeTime.count());

            std::cout << prefix << " -- " << repeat << std::endl;
        }

        auto getEmptyAverage =
            std::accumulate(getEmptyTimes.begin(), getEmptyTimes.end(), 0) /
            getEmptyTimes.size();

        auto storeAverage =
            std::accumulate(storeTimes.begin(), storeTimes.end(), 0) /
            storeTimes.size();

        auto getAverage = std::accumulate(getTimes.begin(), getTimes.end(), 0) /
            getTimes.size();

        auto durabilityAverage =
            std::accumulate(durabilityTimes.begin(), durabilityTimes.end(), 0) /
            durabilityTimes.size();

        auto removeAverage =
            std::accumulate(removeTimes.begin(), removeTimes.end(), 0) /
            removeTimes.size();

        std::cout << prefix << " SIZE=" << batchSize << ": ("
                  << "GET_EMPTY=" << getEmptyAverage
                  << " [ms], STORE=" << storeAverage
                  << " [ms], GET=" << getAverage
                  << " [ms], DURABILITY=" << durabilityAverage
                  << " [ms], REMOVE=" << removeAverage << " [ms])\n";

    };

    std::vector<std::thread> workers;
    for (int i = 0; i < workerCount; i++) {
        workers.emplace_back(std::thread(benchmarkWorker, std::to_string(i)));
    }

    for (auto &worker : workers) {
        worker.join();
    }
}
