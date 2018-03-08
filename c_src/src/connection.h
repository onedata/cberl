/**
 * @file connection.h
 * @author Krzysztof Trzepla
 * @copyright (C) 2017: Krzysztof Trzepla
 * This software is released under the MIT license cited in 'LICENSE.md'
 */

#ifndef COUCHBASE_CONNECTION_H
#define COUCHBASE_CONNECTION_H

#include "promiseProxy.h"
#include "requests/requests.h"
#include "responses/responses.h"

#include <asio.hpp>
#include <libcouchbase/couchbase.h>

#include <future>
#include <map>
#include <memory>
#include <string>

namespace cb {

using GetPromises = PromiseProxy<MultiResponse<GetResponse>>;
using StorePromises = PromiseProxy<MultiResponse<StoreResponse>>;
using RemovePromises = PromiseProxy<MultiResponse<RemoveResponse>>;
using ArithmeticPromises = PromiseProxy<MultiResponse<ArithmeticResponse>>;
using HttpPromises = PromiseProxy<HttpResponse>;
using DurabilityPromises = PromiseProxy<MultiResponse<DurabilityResponse>>;

class Connection : public GetPromises,
                   public StorePromises,
                   public RemovePromises,
                   public ArithmeticPromises,
                   public HttpPromises,
                   public DurabilityPromises,
                   public std::enable_shared_from_this<Connection> {
public:
    Connection(const ConnectRequest &bucket, asio::io_service *ioService,
        std::promise<ConnectResponse> &&connectPromise);

    ~Connection();

    std::promise<ConnectResponse> &connectPromise();

    std::shared_ptr<Connection> getShared() { return shared_from_this(); }

    void get(const MultiRequest<GetRequest> &request,
        std::promise<MultiResponse<GetResponse>>&& promise);

    void store(const MultiRequest<StoreRequest> &request,
        std::promise<MultiResponse<StoreResponse>> &&promise);

    void remove(const MultiRequest<RemoveRequest> &request,
        std::promise<MultiResponse<RemoveResponse>> &&promise);

    void arithmetic(const MultiRequest<ArithmeticRequest> &request,
        std::promise<MultiResponse<ArithmeticResponse>> &&promise);

    void http(const HttpRequest &request, std::promise<HttpResponse> &&promise);

    void durability(const MultiRequest<DurabilityRequest> &request,
        const DurabilityRequestOptions &options,
        std::promise<MultiResponse<DurabilityResponse>> &&promise);

private:
    lcb_t m_instance;
    std::promise<ConnectResponse> m_connectPromise;
};

} // namespace cb

#endif // COUCHBASE_CONNECTION_H
