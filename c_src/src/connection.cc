/**
 * @file connection.cc
 * @author Krzysztof Trzepla
 * @copyright (C) 2017: Krzysztof Trzepla
 * This software is released under the MIT license cited in 'LICENSE.md'
 */
#include "connection.h"

namespace {

void bootstrapCallback(lcb_t instance, lcb_error_t err)
{
    auto connection = const_cast<cb::Connection *>(
        static_cast<const cb::Connection *>(lcb_get_cookie(instance)));

    assert(connection);
    if (!connection)
        return;

    auto responseId = connection->connectionId();
    auto connectionPlaceholder =
        dynamic_cast<cb::ConnectionResponses *>(connection);

    if (!connectionPlaceholder->hasResponse(responseId))
        return;

    if (err == LCB_ETIMEDOUT) {
        // With large number of connections, this callback receives sometimes
        // one or more calls with timeout error, eventually followed by
        // successfull connection response.
        connectionPlaceholder->getResponse(responseId).setError(err);
        if (!connection->retry()) {
            connectionPlaceholder->emitResponse(responseId);
            connectionPlaceholder->forgetResponse(responseId);
        }
    }
    else {
        connectionPlaceholder->getResponse(responseId).setError(err);
        connectionPlaceholder->emitResponse(responseId);
        connectionPlaceholder->forgetResponse(responseId);
    }
}

void getCallback(lcb_t instance, const void *cookie, lcb_error_t err,
    const lcb_get_resp_t *resp)
{
    auto connection = const_cast<cb::Connection *>(
        static_cast<const cb::Connection *>(lcb_get_cookie(instance)));

    assert(connection);
    if (!connection)
        return;

    auto responseId = reinterpret_cast<const uint64_t>(cookie);
    auto getPlaceholder = dynamic_cast<cb::GetResponses *>(connection);
    if (!getPlaceholder->hasResponse(responseId))
        return;

    auto &response = getPlaceholder->getResponse(responseId);

    if (err == LCB_SUCCESS) {
        response.add(
            cb::GetResponse{resp->v.v0.key, resp->v.v0.nkey, resp->v.v0.cas,
                resp->v.v0.flags, resp->v.v0.bytes, resp->v.v0.nbytes});
    }
    else {
        response.add(cb::GetResponse{err, resp->v.v0.key, resp->v.v0.nkey});
    }

    if (response.complete()) {
        getPlaceholder->emitResponse(responseId);
        getPlaceholder->forgetResponse(responseId);
    }
}

void storeCallback(lcb_t instance, const void *cookie, lcb_storage_t operation,
    lcb_error_t err, const lcb_store_resp_t *resp)
{
    auto connection = const_cast<cb::Connection *>(
        static_cast<const cb::Connection *>(lcb_get_cookie(instance)));

    assert(connection);
    if (!connection)
        return;

    auto responseId = reinterpret_cast<const uint64_t>(cookie);
    auto storePlaceholder = dynamic_cast<cb::StoreResponses *>(connection);
    if (!storePlaceholder->hasResponse(responseId))
        return;

    auto &response = storePlaceholder->getResponse(responseId);

    if (err == LCB_SUCCESS) {
        response.add(
            cb::StoreResponse{resp->v.v0.key, resp->v.v0.nkey, resp->v.v0.cas});
    }
    else {
        response.add(cb::StoreResponse{err, resp->v.v0.key, resp->v.v0.nkey});
    }

    if (response.complete()) {
        storePlaceholder->emitResponse(responseId);
        storePlaceholder->forgetResponse(responseId);
    }
}

void arithmeticCallback(lcb_t instance, const void *cookie, lcb_error_t err,
    const lcb_arithmetic_resp_t *resp)
{
    auto connection = const_cast<cb::Connection *>(
        static_cast<const cb::Connection *>(lcb_get_cookie(instance)));

    assert(connection);
    if (!connection)
        return;

    auto responseId = reinterpret_cast<const uint64_t>(cookie);
    auto arithmeticPlaceholder =
        dynamic_cast<cb::ArithmeticResponses *>(connection);
    if (!arithmeticPlaceholder->hasResponse(responseId))
        return;

    auto &response = arithmeticPlaceholder->getResponse(responseId);

    if (err == LCB_SUCCESS) {
        response.add(cb::ArithmeticResponse{
            resp->v.v0.key, resp->v.v0.nkey, resp->v.v0.cas, resp->v.v0.value});
    }
    else {
        response.add(
            cb::ArithmeticResponse{err, resp->v.v0.key, resp->v.v0.nkey});
    }

    if (response.complete()) {
        arithmeticPlaceholder->emitResponse(responseId);
        arithmeticPlaceholder->forgetResponse(responseId);
    }
}

void removeCallback(lcb_t instance, const void *cookie, lcb_error_t err,
    const lcb_remove_resp_t *resp)
{
    auto connection = const_cast<cb::Connection *>(
        static_cast<const cb::Connection *>(lcb_get_cookie(instance)));

    assert(connection);
    if (!connection)
        return;

    auto responseId = reinterpret_cast<const uint64_t>(cookie);
    auto removePlaceholder = dynamic_cast<cb::RemoveResponses *>(connection);
    if (!removePlaceholder->hasResponse(responseId))
        return;

    auto &response = removePlaceholder->getResponse(responseId);

    response.add(cb::RemoveResponse{err, resp->v.v0.key, resp->v.v0.nkey});

    if (response.complete()) {
        removePlaceholder->emitResponse(responseId);
        removePlaceholder->forgetResponse(responseId);
    }
}

void httpCallback(lcb_http_request_t request, lcb_t instance,
    const void *cookie, lcb_error_t err, const lcb_http_resp_t *resp)
{
    auto connection = const_cast<cb::Connection *>(
        static_cast<const cb::Connection *>(lcb_get_cookie(instance)));

    assert(connection);
    if (!connection)
        return;

    auto responseId = reinterpret_cast<const uint64_t>(cookie);
    auto httpPlaceholder = dynamic_cast<cb::HttpResponses *>(connection);
    if (!httpPlaceholder->hasResponse(responseId))
        return;

    auto &response = httpPlaceholder->getResponse(responseId);

    response.setError(err);
    if (err == LCB_SUCCESS) {
        response.setStatus(resp->v.v0.status);
        response.setBody(resp->v.v0.bytes, resp->v.v0.nbytes);
    }

    httpPlaceholder->emitResponse(responseId);
    httpPlaceholder->forgetResponse(responseId);
}

void durabilityCallback(lcb_t instance, const void *cookie, lcb_error_t err,
    const lcb_durability_resp_t *resp)
{
    auto connection = const_cast<cb::Connection *>(
        static_cast<const cb::Connection *>(lcb_get_cookie(instance)));

    assert(connection);
    if (!connection)
        return;

    auto responseId = reinterpret_cast<const uint64_t>(cookie);
    auto durabilityPlaceholder =
        dynamic_cast<cb::DurabilityResponses *>(connection);
    if (!durabilityPlaceholder->hasResponse(responseId))
        return;

    auto &response = durabilityPlaceholder->getResponse(responseId);

    if (err == LCB_SUCCESS) {
        response.add(cb::DurabilityResponse{
            resp->v.v0.key, resp->v.v0.nkey, resp->v.v0.cas});
    }
    else {
        response.add(
            cb::DurabilityResponse{err, resp->v.v0.key, resp->v.v0.nkey});
    }

    if (response.complete()) {
        durabilityPlaceholder->emitResponse(responseId);
        durabilityPlaceholder->forgetResponse(responseId);
    }
}
} // namespace

namespace cb {

uint64_t Connection::m_connectionNextId = 1;
std::mutex Connection::m_mutex{};

Connection::Connection()
{
    std::lock_guard<std::mutex> lock(Connection::m_mutex);
    m_connectionId = Connection::m_connectionNextId++;
}

uint64_t Connection::connectionId() const { return m_connectionId; }

uint16_t Connection::retry()
{
    if (--m_retriesLeft < 0)
        m_retriesLeft = 0;
    return m_retriesLeft;
}

void Connection::bootstrap(const ConnectRequest &request,
    folly::EventBase *eventBase, Callback<ConnectResponse> callback)
{
    struct lcb_create_st createOpts = {0};
    createOpts.v.v0.host = request.host().c_str();
    createOpts.v.v0.user = request.username().c_str();
    createOpts.v.v0.passwd = request.password().c_str();
    createOpts.v.v0.bucket = request.bucket().c_str();

    // Setup lcb ioops struct to use Libevent
    lcb_io_opt_t ioops;
    struct lcb_create_io_ops_st ciops;
    memset(&ciops, 0, sizeof(ciops));
    ciops.v.v0.type = LCB_IO_OPS_LIBEVENT;
    ciops.v.v0.cookie = eventBase->getLibeventBase();
    lcb_create_io_ops(&ioops, &ciops);

    createOpts.v.v0.io = ioops;

    lcb_error_t err = lcb_create(&m_instance, &createOpts);
    if (err != LCB_SUCCESS) {
        throw err;
    }

    lcb_set_cookie(m_instance, this);

    lcb_set_bootstrap_callback(m_instance, bootstrapCallback);
    lcb_set_get_callback(m_instance, getCallback);
    lcb_set_store_callback(m_instance, storeCallback);
    lcb_set_arithmetic_callback(m_instance, arithmeticCallback);
    lcb_set_remove_callback(m_instance, removeCallback);
    lcb_set_http_complete_callback(m_instance, httpCallback);
    lcb_set_durability_callback(m_instance, durabilityCallback);

    std::string optName;
    int optValue;
    for (const auto &option : request.options()) {
        std::tie(optName, optValue) = option;
        if (optName == "operation_timeout") {
            err = lcb_cntl(
                m_instance, LCB_CNTL_SET, LCB_CNTL_OP_TIMEOUT, &optValue);
        }
        else if (optName == "config_total_timeout") {
            err = lcb_cntl(m_instance, LCB_CNTL_SET,
                LCB_CNTL_CONFIGURATION_TIMEOUT, &optValue);
        }
        else if (optName == "view_timeout") {
            err = lcb_cntl(
                m_instance, LCB_CNTL_SET, LCB_CNTL_VIEW_TIMEOUT, &optValue);
        }
        else if (optName == "durability_timeout") {
            err = lcb_cntl(m_instance, LCB_CNTL_SET,
                LCB_CNTL_DURABILITY_TIMEOUT, &optValue);
        }
        else if (optName == "durability_interval") {
            err = lcb_cntl(m_instance, LCB_CNTL_SET,
                LCB_CNTL_DURABILITY_INTERVAL, &optValue);
        }
        else if (optName == "http_timeout") {
            err = lcb_cntl(
                m_instance, LCB_CNTL_SET, LCB_CNTL_HTTP_TIMEOUT, &optValue);
        }
        if (err != LCB_SUCCESS) {
            throw err;
        }
    }

    cb::ConnectResponse response{LCB_SUCCESS, getShared()};

    ConnectionResponses::storeResponse(
        connectionId(), std::move(response), std::move(callback));

    err = lcb_connect(m_instance);
    if (err != LCB_SUCCESS) {
        ConnectionResponses::forgetResponse(connectionId());
        throw err;
    }
}

Connection::~Connection() { lcb_destroy(m_instance); }

void Connection::get(const MultiRequest<GetRequest> &request,
    Callback<MultiResponse<GetResponse>> callback)
{
    const auto &requests = request.requests();
    std::vector<lcb_get_cmd_t> commands{requests.size()};
    for (unsigned int i = 0; i < requests.size(); ++i) {
        commands[i].version = 0;
        commands[i].v.v0.key = requests[i].key().c_str();
        commands[i].v.v0.nkey = requests[i].key().size();
        commands[i].v.v0.exptime = requests[i].expiry();
        commands[i].v.v0.lock = requests[i].lock();
    }
    std::vector<const lcb_get_cmd_t *> commandsPtr{requests.size()};
    for (unsigned int i = 0; i < requests.size(); ++i) {
        commandsPtr[i] = &commands[i];
    }

    cb::MultiResponse<cb::GetResponse> response{LCB_SUCCESS, requests.size()};

    auto requestId =
        GetResponses::storeResponse(std::move(response), std::move(callback));

    auto err = lcb_get(m_instance, reinterpret_cast<void *>(requestId),
        requests.size(), commandsPtr.data());

    if (err != LCB_SUCCESS) {
        GetResponses::getResponse(requestId).setError(err);
        GetResponses::emitResponse(requestId);
        GetResponses::forgetResponse(requestId);
    }
}

void Connection::store(const MultiRequest<StoreRequest> &request,
    Callback<MultiResponse<StoreResponse>> callback)
{
    const auto &requests = request.requests();
    std::vector<lcb_store_cmd_t> commands{requests.size()};
    for (unsigned int i = 0; i < requests.size(); ++i) {
        commands[i].version = 0;
        commands[i].v.v0.operation = requests[i].operation();
        commands[i].v.v0.key = requests[i].key().c_str();
        commands[i].v.v0.nkey = requests[i].key().size();
        commands[i].v.v0.cas = requests[i].cas();
        commands[i].v.v0.flags = requests[i].flags();
        commands[i].v.v0.bytes = requests[i].value().c_str();
        commands[i].v.v0.nbytes = requests[i].value().size();
        commands[i].v.v0.exptime = requests[i].expiry();
    }
    std::vector<const lcb_store_cmd_t *> commandsPtr{requests.size()};
    for (unsigned int i = 0; i < requests.size(); ++i) {
        commandsPtr[i] = &commands[i];
    }

    cb::MultiResponse<cb::StoreResponse> response{LCB_SUCCESS, requests.size()};

    auto requestId =
        StoreResponses::storeResponse(std::move(response), std::move(callback));

    auto err = lcb_store(m_instance, reinterpret_cast<void *>(requestId),
        requests.size(), commandsPtr.data());

    if (err != LCB_SUCCESS) {
        StoreResponses::getResponse(requestId).setError(err);
        StoreResponses::emitResponse(requestId);
        StoreResponses::forgetResponse(requestId);
    }
}

void Connection::remove(const MultiRequest<RemoveRequest> &request,
    Callback<MultiResponse<RemoveResponse>> callback)
{
    const auto &requests = request.requests();
    std::vector<lcb_remove_cmd_t> commands{requests.size()};
    for (unsigned int i = 0; i < requests.size(); ++i) {
        commands[i].version = 0;
        commands[i].v.v0.key = requests[i].key().c_str();
        commands[i].v.v0.nkey = requests[i].key().size();
        commands[i].v.v0.cas = requests[i].cas();
    }
    std::vector<const lcb_remove_cmd_t *> commandsPtr{requests.size()};
    for (unsigned int i = 0; i < requests.size(); ++i) {
        commandsPtr[i] = &commands[i];
    }

    cb::MultiResponse<cb::RemoveResponse> response{
        LCB_SUCCESS, requests.size()};

    auto requestId = RemoveResponses::storeResponse(
        std::move(response), std::move(callback));

    auto err = lcb_remove(m_instance, reinterpret_cast<void *>(requestId),
        requests.size(), commandsPtr.data());

    if (err != LCB_SUCCESS) {
        RemoveResponses::getResponse(requestId).setError(err);
        RemoveResponses::emitResponse(requestId);
        RemoveResponses::forgetResponse(requestId);
    }
}

void Connection::arithmetic(const MultiRequest<ArithmeticRequest> &request,
    Callback<MultiResponse<ArithmeticResponse>> callback)
{
    const auto &requests = request.requests();
    std::vector<lcb_arithmetic_cmd_t> commands{requests.size()};
    for (unsigned int i = 0; i < requests.size(); ++i) {
        commands[i].version = 0;
        commands[i].v.v0.key = requests[i].key().c_str();
        commands[i].v.v0.nkey = requests[i].key().size();
        commands[i].v.v0.delta = requests[i].delta();
        commands[i].v.v0.create = requests[i].create();
        commands[i].v.v0.initial = requests[i].initial();
        commands[i].v.v0.exptime = requests[i].expiry();
    }
    std::vector<const lcb_arithmetic_cmd_t *> commandsPtr{requests.size()};
    for (unsigned int i = 0; i < requests.size(); ++i) {
        commandsPtr[i] = &commands[i];
    }

    cb::MultiResponse<cb::ArithmeticResponse> response{
        LCB_SUCCESS, requests.size()};

    auto requestId = ArithmeticResponses::storeResponse(
        std::move(response), std::move(callback));

    auto err = lcb_arithmetic(m_instance, reinterpret_cast<void *>(requestId),
        requests.size(), commandsPtr.data());

    if (err != LCB_SUCCESS) {
        ArithmeticResponses::getResponse(requestId).setError(err);
        ArithmeticResponses::emitResponse(requestId);
        ArithmeticResponses::forgetResponse(requestId);
    }
}

void Connection::http(
    const HttpRequest &request, Callback<HttpResponse> callback)
{
    lcb_http_request_t req;
    lcb_http_cmd_t command;
    command.version = 0;
    command.v.v0.method = request.method();
    command.v.v0.path = request.path().c_str();
    command.v.v0.npath = request.path().size();
    command.v.v0.content_type = request.contentType().c_str();
    command.v.v0.body = request.body().c_str();
    command.v.v0.nbody = request.body().size();
    command.v.v0.chunked = false;

    cb::HttpResponse response{LCB_SUCCESS};

    auto requestId =
        HttpResponses::storeResponse(std::move(response), std::move(callback));

    auto err = lcb_make_http_request(m_instance,
        reinterpret_cast<void *>(requestId), request.type(), &command, &req);

    if (err != LCB_SUCCESS) {
        HttpResponses::getResponse(requestId).setError(err);
        HttpResponses::emitResponse(requestId);
        HttpResponses::forgetResponse(requestId);
    }
}

void Connection::durability(const MultiRequest<DurabilityRequest> &request,
    const DurabilityRequestOptions &requestOptions,
    Callback<MultiResponse<DurabilityResponse>> callback)
{
    const auto &requests = request.requests();
    std::vector<lcb_durability_cmd_t> commands{requests.size()};
    for (unsigned int i = 0; i < requests.size(); ++i) {
        commands[i].version = 0;
        commands[i].v.v0.key = requests[i].key().c_str();
        commands[i].v.v0.nkey = requests[i].key().size();
        commands[i].v.v0.cas = requests[i].cas();
    }
    std::vector<const lcb_durability_cmd_t *> commandsPtr{requests.size()};
    for (unsigned int i = 0; i < requests.size(); ++i) {
        commandsPtr[i] = &commands[i];
    }

    lcb_durability_opts_t options = {};
    options.v.v0.persist_to = requestOptions.persistTo();
    options.v.v0.replicate_to = requestOptions.replicateTo();
    options.v.v0.cap_max = 1;

    cb::MultiResponse<cb::DurabilityResponse> response{
        LCB_SUCCESS, requests.size()};

    auto requestId = DurabilityResponses::storeResponse(
        std::move(response), std::move(callback));

    auto err =
        lcb_durability_poll(m_instance, reinterpret_cast<void *>(requestId),
            &options, requests.size(), commandsPtr.data());

    if (err != LCB_SUCCESS) {
        DurabilityResponses::getResponse(requestId).setError(err);
        DurabilityResponses::emitResponse(requestId);
        DurabilityResponses::forgetResponse(requestId);
    }
}

} // namespace cb
