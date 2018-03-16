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

#include <folly/executors/IOThreadPoolExecutor.h>
#include <libcouchbase/couchbase.h>

#include <functional>
#include <future>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace cb {

class Client {

public:
    Client();

    ~Client();

    void connect(ConnectRequest, Callback<ConnectResponse> callback);

    void get(ConnectionPtr connection, MultiRequest<GetRequest> request,
        Callback<MultiResponse<GetResponse>> callback);

    void store(ConnectionPtr connection, MultiRequest<StoreRequest> request,
        Callback<MultiResponse<StoreResponse>> callback);

    void remove(ConnectionPtr connection, MultiRequest<RemoveRequest> request,
        Callback<MultiResponse<RemoveResponse>> callback);

    void arithmetic(ConnectionPtr connection,
        MultiRequest<ArithmeticRequest> request,
        Callback<MultiResponse<ArithmeticResponse>> callback);

    void http(ConnectionPtr connection, HttpRequest request,
        Callback<HttpResponse> callback);

    void durability(ConnectionPtr connection,
        MultiRequest<DurabilityRequest> request,
        DurabilityRequestOptions options,
        Callback<MultiResponse<DurabilityResponse>> callback);

private:
    // Currently the connection created via a @c client instance
    // will be released only after the client event loop is
    // completed and client is destroyed
    std::vector<std::shared_ptr<cb::Connection>> m_connections;

    const unsigned short m_workerCount;

    std::shared_ptr<folly::IOThreadPoolExecutor> m_executor;
};

} // namespace cb

#endif // COUCHBASE_CLIENT_H
