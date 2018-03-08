/**
 * @file client.cc
 * @author Krzysztof Trzepla
 * @copyright (C) 2017: Krzysztof Trzepla
 * This software is released under the MIT license cited in 'LICENSE.md'
 */

#include "client.h"
#include "connection.h"

namespace cb {

Client::Client()
    : m_workerCount{1}
{
    m_executor = std::make_shared<folly::IOThreadPoolExecutor>(m_workerCount,
        std::make_shared<folly::NamedThreadFactory>("CBerlThreadPool"));
}

Client::~Client() { m_executor->join(); }

void Client::connect(ConnectRequest request, Callback<ConnectResponse> callback)
{
    m_executor->add([
            &, connection = std::make_shared<Connection>(),
        request = std::move(request), callback = std::move(callback)
    ] {
        try {
            connection->bootstrap(
                request, m_executor->getEventBase(), std::move(callback));
            m_connections.push_back(connection);
        }
        catch (lcb_error_t err) {
            callback(ConnectResponse{err, nullptr});
        }
    });
}

void Client::get(ConnectionPtr connection, MultiRequest<GetRequest> request,
    Callback<MultiResponse<GetResponse>> callback)
{
    m_executor->add([
        connection = std::move(connection), request = std::move(request),
        callback = std::move(callback)
    ] { connection->get(request, std::move(callback)); });
}

void Client::store(ConnectionPtr connection, MultiRequest<StoreRequest> request,
    Callback<MultiResponse<StoreResponse>> callback)
{
    m_executor->add([
        connection = std::move(connection), request = std::move(request),
        callback = std::move(callback)
    ] { connection->store(request, std::move(callback)); });
}

void Client::remove(ConnectionPtr connection,
    MultiRequest<RemoveRequest> request,
    Callback<MultiResponse<RemoveResponse>> callback)
{
    m_executor->add([
        connection = std::move(connection), request = std::move(request),
        callback = std::move(callback)
    ] { connection->remove(request, std::move(callback)); });
}

void Client::arithmetic(ConnectionPtr connection,
    MultiRequest<ArithmeticRequest> request,
    Callback<MultiResponse<ArithmeticResponse>> callback)
{
    m_executor->add([
        connection = std::move(connection), request = std::move(request),
        callback = std::move(callback)
    ] { connection->arithmetic(request, std::move(callback)); });
}

void Client::http(ConnectionPtr connection, HttpRequest request,
    Callback<HttpResponse> callback)
{
    m_executor->add([
        connection = std::move(connection), request = std::move(request),
        callback = std::move(callback)
    ] { connection->http(request, std::move(callback)); });
}

void Client::durability(ConnectionPtr connection,
    MultiRequest<DurabilityRequest> request, DurabilityRequestOptions options,
    Callback<MultiResponse<DurabilityResponse>> callback)
{
    m_executor->add([
        connection = std::move(connection), request = std::move(request),
        options = std::move(options), callback = std::move(callback)
    ] {
        connection->durability(
            request, std::move(options), std::move(callback));
    });
}

} // namespace cb
