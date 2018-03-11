/**
 * @file client.cc
 * @author Krzysztof Trzepla
 * @copyright (C) 2017: Krzysztof Trzepla
 * This software is released under the MIT license cited in 'LICENSE.md'
 */

#include "client.h"
#include "connection.h"

#include <asio/post.hpp>

namespace cb {

Client::Client()
    : m_workerCount{1}
    , m_ioService{m_workerCount}
    , m_work{asio::make_work_guard(m_ioService)}
{
    for (size_t i = 0; i < m_workerCount; ++i) {
        m_workers.emplace_back([&]() { m_ioService.run(); });
    }
}

Client::~Client()
{
    m_ioService.stop();
    join();
}

void Client::join()
{
    for (auto &thread : m_workers)
        if (thread.joinable())
            thread.join();
}

void Client::connect(ConnectRequest request, Callback<ConnectResponse> callback)
{
    asio::post(m_ioService,
        [&, request = std::move(request), callback = std::move(callback) ] {
            auto connection = std::make_shared<Connection>();
            m_connections.emplace_back(connection);
            connection->bootstrap(request, &m_ioService, std::move(callback));
        });
}

void Client::get(ConnectionPtr connection, MultiRequest<GetRequest> request,
    Callback<MultiResponse<GetResponse>> callback)
{
    asio::post(m_ioService, [
        connection = std::move(connection), request = std::move(request),
        callback = std::move(callback)
    ] { connection->get(request, std::move(callback)); });
}

void Client::store(ConnectionPtr connection, MultiRequest<StoreRequest> request,
    Callback<MultiResponse<StoreResponse>> callback)
{
    asio::post(m_ioService, [
        connection = std::move(connection), request = std::move(request),
        callback = std::move(callback)
    ] { connection->store(request, std::move(callback)); });
}

void Client::remove(ConnectionPtr connection,
    MultiRequest<RemoveRequest> request,
    Callback<MultiResponse<RemoveResponse>> callback)
{
    asio::post(m_ioService, [
        connection = std::move(connection), request = std::move(request),
        callback = std::move(callback)
    ] { connection->remove(request, std::move(callback)); });
}

void Client::arithmetic(ConnectionPtr connection,
    MultiRequest<ArithmeticRequest> request,
    Callback<MultiResponse<ArithmeticResponse>> callback)
{
    asio::post(m_ioService, [
        connection = std::move(connection), request = std::move(request),
        callback = std::move(callback)
    ] { connection->arithmetic(request, std::move(callback)); });
}

void Client::http(ConnectionPtr connection, HttpRequest request,
    Callback<HttpResponse> callback)
{
    asio::post(m_ioService, [
        connection = std::move(connection), request = std::move(request),
        callback = std::move(callback)
    ] { connection->http(request, std::move(callback)); });
}

void Client::durability(ConnectionPtr connection,
    MultiRequest<DurabilityRequest> request, DurabilityRequestOptions options,
    Callback<MultiResponse<DurabilityResponse>> callback)
{
    asio::post(m_ioService, [
        connection = std::move(connection), request = std::move(request),
        options = std::move(options), callback = std::move(callback)
    ] {
        connection->durability(
            request, std::move(options), std::move(callback));
    });
}

} // namespace cb
