/**
 * @file client.h
 * @author Krzysztof Trzepla
 * @copyright (C) 2017: Krzysztof Trzepla
 * This software is released under the MIT license cited in 'LICENSE.md'
 */

#ifndef COUCHBASE_CLIENT_H
#define COUCHBASE_CLIENT_H

#include "requests/requests.h"
#include "responses/responses.h"
#include "types.h"

#include <asio/executor_work_guard.hpp>
#include <asio/io_service.hpp>
#include <libcouchbase/couchbase.h>

#include <functional>
#include <future>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace cb {

class Client {
    template <typename T> using Callback = std::function<void(const T &)>;

public:
    Client();

    ~Client();

    std::future<ConnectResponse> connect(ConnectRequest);

    std::future<MultiResponse<GetResponse>> get(
        ConnectionPtr connection, MultiRequest<GetRequest> request);

    std::future<MultiResponse<StoreResponse>> store(
        ConnectionPtr connection, MultiRequest<StoreRequest> request);

    std::future<MultiResponse<RemoveResponse>> remove(
        ConnectionPtr connection, MultiRequest<RemoveRequest> request);

    std::future<MultiResponse<ArithmeticResponse>> arithmetic(
        ConnectionPtr connection, MultiRequest<ArithmeticRequest> request);

    std::future<HttpResponse> http(
        ConnectionPtr connection, HttpRequest request);

    std::future<MultiResponse<DurabilityResponse>> durability(
        ConnectionPtr connection, MultiRequest<DurabilityRequest> request,
        DurabilityRequestOptions options);

private:
    const unsigned short m_workerCount;

    asio::io_service m_ioService;
    asio::executor_work_guard<asio::io_service::executor_type> m_work;

    std::vector<std::thread> m_workers;
    std::deque<std::shared_ptr<cb::Connection>> m_connections;

    void join();
};

} // namespace cb

#endif // COUCHBASE_CLIENT_H
