/**
 * @file connection.h
 * @author Krzysztof Trzepla
 * @copyright (C) 2017: Krzysztof Trzepla
 * This software is released under the MIT license cited in 'LICENSE.md'
 */

#ifndef COUCHBASE_CONNECTION_H
#define COUCHBASE_CONNECTION_H

#include "requests/requests.h"
#include "responsePlaceholder.h"
#include "responses/responses.h"
#include "types.h"

#include <asio.hpp>
#include <libcouchbase/couchbase.h>

#include <future>
#include <map>
#include <memory>
#include <string>

namespace cb {

using ConnectionResponses = ResponsePlaceholder<ConnectResponse>;
using GetResponses = ResponsePlaceholder<MultiResponse<GetResponse>>;
using StoreResponses = ResponsePlaceholder<MultiResponse<StoreResponse>>;
using RemoveResponses = ResponsePlaceholder<MultiResponse<RemoveResponse>>;
using ArithmeticResponses =
    ResponsePlaceholder<MultiResponse<ArithmeticResponse>>;
using HttpResponses = ResponsePlaceholder<HttpResponse>;
using DurabilityResponses =
    ResponsePlaceholder<MultiResponse<DurabilityResponse>>;

class Connection : public ConnectionResponses,
                   public GetResponses,
                   public StoreResponses,
                   public RemoveResponses,
                   public ArithmeticResponses,
                   public HttpResponses,
                   public DurabilityResponses,
                   public std::enable_shared_from_this<Connection> {
public:
    Connection();

    ~Connection();

    std::shared_ptr<Connection> getShared() { return shared_from_this(); }

    void bootstrap(const ConnectRequest &request, asio::io_service *ioService,
        Callback<ConnectResponse> callback);

    void get(const MultiRequest<GetRequest> &request,
        Callback<MultiResponse<GetResponse>> callback);

    void store(const MultiRequest<StoreRequest> &request,
        Callback<MultiResponse<StoreResponse>> callback);

    void remove(const MultiRequest<RemoveRequest> &request,
        Callback<MultiResponse<RemoveResponse>> callback);

    void arithmetic(const MultiRequest<ArithmeticRequest> &request,
        Callback<MultiResponse<ArithmeticResponse>> callback);

    void http(const HttpRequest &request, Callback<HttpResponse> callback);

    void durability(const MultiRequest<DurabilityRequest> &request,
        const DurabilityRequestOptions &options,
        Callback<MultiResponse<DurabilityResponse>> callback);

private:
    lcb_t m_instance;

    void join();
};

} // namespace cb

#endif // COUCHBASE_CONNECTION_H
