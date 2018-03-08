/**
 * @file client.cc
 * @author Krzysztof Trzepla
 * @copyright (C) 2017: Krzysztof Trzepla
 * This software is released under the MIT license cited in 'LICENSE.md'
 */

#include "client.h"
#include "connection.h"

#include <asio/post.hpp>
#include <iostream>

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

void Client::join()
{
    for (auto &thread : m_workers)
        if (thread.joinable())
            thread.join();
}

Client::~Client()
{
    m_ioService.stop();
    join();
}

std::future<ConnectResponse> Client::connect(ConnectRequest request)
{
    std::promise<ConnectResponse> connectPromise;
    auto connectFuture = connectPromise.get_future();

    asio::post(m_ioService, [
        this, request = std::move(request), ioServicePtr = &m_ioService,
        promise = std::move(connectPromise)
    ]() mutable {
        m_connections.emplace_back(std::make_shared<Connection>(
            request, ioServicePtr, std::move(promise)));
    });

    return connectFuture;
}

std::future<MultiResponse<GetResponse>> Client::get(
    ConnectionPtr connection, MultiRequest<GetRequest> request)
{
    std::promise<MultiResponse<GetResponse>> getPromise;
    auto getFuture = getPromise.get_future();

    asio::post(m_ioService, [
        connection = std::move(connection), request = std::move(request),
        promise = std::move(getPromise)
    ]() mutable { connection->get(request, std::move(promise)); });

    return getFuture;
}

std::future<MultiResponse<StoreResponse>> Client::store(
    ConnectionPtr connection, MultiRequest<StoreRequest> request)
{
    std::promise<MultiResponse<StoreResponse>> storePromise;
    auto storeFuture = storePromise.get_future();

    asio::post(m_ioService, [
        connection = std::move(connection), request = std::move(request),
        promise = std::move(storePromise)
    ]() mutable { connection->store(request, std::move(promise)); });

    return storeFuture;
}

std::future<MultiResponse<RemoveResponse>> Client::remove(
    ConnectionPtr connection, MultiRequest<RemoveRequest> request)
{
    std::promise<MultiResponse<RemoveResponse>> removePromise;
    auto removeFuture = removePromise.get_future();

    asio::post(m_ioService, [
        connection = std::move(connection), request = std::move(request),
        promise = std::move(removePromise)
    ]() mutable { connection->remove(request, std::move(promise)); });

    return removeFuture;
}

std::future<MultiResponse<ArithmeticResponse>> Client::arithmetic(
    ConnectionPtr connection, MultiRequest<ArithmeticRequest> request)
{
    std::promise<MultiResponse<ArithmeticResponse>> arithmeticPromise;
    auto arithmeticFuture = arithmeticPromise.get_future();

    asio::post(m_ioService, [
        connection = std::move(connection), request = std::move(request),
        promise = std::move(arithmeticPromise)
    ]() mutable { connection->arithmetic(request, std::move(promise)); });

    return arithmeticFuture;
}

std::future<HttpResponse> Client::http(
    ConnectionPtr connection, HttpRequest request)
{
    std::promise<HttpResponse> httpPromise;
    auto httpFuture = httpPromise.get_future();

    asio::post(m_ioService, [
        connection = std::move(connection), request = std::move(request),
        promise = std::move(httpPromise)
    ]() mutable { connection->http(request, std::move(promise)); });

    return httpFuture;
}

std::future<MultiResponse<DurabilityResponse>> Client::durability(
    ConnectionPtr connection, MultiRequest<DurabilityRequest> request,
    DurabilityRequestOptions options)
{
    std::promise<MultiResponse<DurabilityResponse>> durabilityPromise;
    auto durabilityFuture = durabilityPromise.get_future();

    asio::post(m_ioService, [
        connection = std::move(connection), request = std::move(request),
        options = std::move(options), promise = std::move(durabilityPromise)
    ]() mutable {
        connection->durability(request, options, std::move(promise));
    });

    return durabilityFuture;
}

} // namespace cb
