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

Client::Client() {}

Client::~Client() {}

void Client::connect(ConnectRequest request, Callback<ConnectResponse> callback)
{
    auto connection = std::make_shared<Connection>();
    m_connections.emplace_back(connection);

    connection->bootstrap(request, std::move(callback));
}

void Client::get(ConnectionPtr connection, MultiRequest<GetRequest> request,
    Callback<MultiResponse<GetResponse>> callback)
{
    connection->get(request, std::move(callback));
}

void Client::store(ConnectionPtr connection, MultiRequest<StoreRequest> request,
    Callback<MultiResponse<StoreResponse>> callback)
{
    connection->store(request, std::move(callback));
}

void Client::remove(ConnectionPtr connection,
    MultiRequest<RemoveRequest> request,
    Callback<MultiResponse<RemoveResponse>> callback)
{
    connection->remove(request, std::move(callback));
}

void Client::arithmetic(ConnectionPtr connection,
    MultiRequest<ArithmeticRequest> request,
    Callback<MultiResponse<ArithmeticResponse>> callback)
{
    connection->arithmetic(request, std::move(callback));
}

void Client::http(ConnectionPtr connection, HttpRequest request,
    Callback<HttpResponse> callback)
{
    connection->http(request, std::move(callback));
}

void Client::durability(ConnectionPtr connection,
    MultiRequest<DurabilityRequest> request, DurabilityRequestOptions options,
    Callback<MultiResponse<DurabilityResponse>> callback)
{
    connection->durability(request, std::move(options), std::move(callback));
}

} // namespace cb
