/**
 * @file connection.cc
 * @author Krzysztof Trzepla
 * @copyright (C) 2017: Krzysztof Trzepla
 * This software is released under the MIT license cited in 'LICENSE.md'
 */
#include "connection.h"
#include "asio_io_opts.h"

extern "C" {
lcb_error_t lcb_create_boost_asio_io_opts(int, lcb_io_opt_st **io, void *arg);
}

namespace {

void bootstrapCallback(lcb_t instance, lcb_error_t err)
{
    auto conn = const_cast<cb::Connection *>(
        static_cast<const cb::Connection *>(lcb_get_cookie(instance)));

    auto &promise = conn->connectPromise();
    promise.set_value(cb::ConnectResponse{err, conn->getShared()});
}

void getCallback(lcb_t instance, const void *cookie, lcb_error_t err,
    const lcb_get_resp_t *resp)
{
    auto connection = const_cast<cb::Connection *>(
        static_cast<const cb::Connection *>(lcb_get_cookie(instance)));

    auto promiseId = reinterpret_cast<const uint64_t>(cookie);

    auto &response =
        dynamic_cast<cb::GetPromises *>(connection)->promisedValue(promiseId);

    if (err == LCB_SUCCESS) {
        response.add(
            cb::GetResponse{resp->v.v0.key, resp->v.v0.nkey, resp->v.v0.cas,
                resp->v.v0.flags, resp->v.v0.bytes, resp->v.v0.nbytes});
    }
    else {
        response.add(cb::GetResponse{err, resp->v.v0.key, resp->v.v0.nkey});
    }

    if (response.complete())
        dynamic_cast<cb::GetPromises *>(connection)->fullfillPromise(promiseId);
}

void storeCallback(lcb_t instance, const void *cookie, lcb_storage_t operation,
    lcb_error_t err, const lcb_store_resp_t *resp)
{
    auto connection = const_cast<cb::Connection *>(
        static_cast<const cb::Connection *>(lcb_get_cookie(instance)));

    auto promiseId = reinterpret_cast<const uint64_t>(cookie);

    auto &response =
        dynamic_cast<cb::StorePromises *>(connection)->promisedValue(promiseId);

    if (err == LCB_SUCCESS) {
        response.add(
            cb::StoreResponse{resp->v.v0.key, resp->v.v0.nkey, resp->v.v0.cas});
    }
    else {
        response.add(cb::StoreResponse{err, resp->v.v0.key, resp->v.v0.nkey});
    }

    if (response.complete())
        dynamic_cast<cb::StorePromises *>(connection)
            ->fullfillPromise(promiseId);
}

void arithmeticCallback(lcb_t instance, const void *cookie, lcb_error_t err,
    const lcb_arithmetic_resp_t *resp)
{
    auto connection = const_cast<cb::Connection *>(
        static_cast<const cb::Connection *>(lcb_get_cookie(instance)));

    auto promiseId = reinterpret_cast<const uint64_t>(cookie);

    auto &response = dynamic_cast<cb::ArithmeticPromises *>(connection)
                         ->promisedValue(promiseId);

    if (err == LCB_SUCCESS) {
        response.add(cb::ArithmeticResponse{
            resp->v.v0.key, resp->v.v0.nkey, resp->v.v0.cas, resp->v.v0.value});
    }
    else {
        response.add(
            cb::ArithmeticResponse{err, resp->v.v0.key, resp->v.v0.nkey});
    }

    if (response.complete())
        dynamic_cast<cb::ArithmeticPromises *>(connection)
            ->fullfillPromise(promiseId);
}

void removeCallback(lcb_t instance, const void *cookie, lcb_error_t err,
    const lcb_remove_resp_t *resp)
{
    auto connection = const_cast<cb::Connection *>(
        static_cast<const cb::Connection *>(lcb_get_cookie(instance)));

    auto promiseId = reinterpret_cast<const uint64_t>(cookie);

    auto &response = dynamic_cast<cb::RemovePromises *>(connection)
                         ->promisedValue(promiseId);

    response.add(cb::RemoveResponse{err, resp->v.v0.key, resp->v.v0.nkey});

    if (response.complete())
        dynamic_cast<cb::RemovePromises *>(connection)
            ->fullfillPromise(promiseId);
}

void httpCallback(lcb_http_request_t request, lcb_t instance,
    const void *cookie, lcb_error_t err, const lcb_http_resp_t *resp)
{
    auto connection = const_cast<cb::Connection *>(
        static_cast<const cb::Connection *>(lcb_get_cookie(instance)));

    auto promiseId = reinterpret_cast<const uint64_t>(cookie);

    auto &response =
        dynamic_cast<cb::HttpPromises *>(connection)->promisedValue(promiseId);

    response.setError(err);
    if (err == LCB_SUCCESS) {
        response.setStatus(resp->v.v0.status);
        response.setBody(resp->v.v0.bytes, resp->v.v0.nbytes);
    }

    dynamic_cast<cb::HttpPromises *>(connection)->fullfillPromise(promiseId);
}

void durabilityCallback(lcb_t instance, const void *cookie, lcb_error_t err,
    const lcb_durability_resp_t *resp)
{
    auto connection = const_cast<cb::Connection *>(
        static_cast<const cb::Connection *>(lcb_get_cookie(instance)));

    auto promiseId = reinterpret_cast<const uint64_t>(cookie);

    auto &response = dynamic_cast<cb::DurabilityPromises *>(connection)
                         ->promisedValue(promiseId);

    if (err == LCB_SUCCESS) {
        response.add(cb::DurabilityResponse{
            resp->v.v0.key, resp->v.v0.nkey, resp->v.v0.cas});
    }
    else {
        response.add(
            cb::DurabilityResponse{err, resp->v.v0.key, resp->v.v0.nkey});
    }

    if (response.complete())
        dynamic_cast<cb::DurabilityPromises *>(connection)
            ->fullfillPromise(promiseId);
}
} // namespace

namespace cb {

Connection::Connection(const ConnectRequest &request,
    asio::io_service *ioService, std::promise<ConnectResponse> &&connectPromise)
    : m_connectPromise(std::move(connectPromise))
{
    struct lcb_create_st createOpts = {0};
    createOpts.v.v0.host = request.host().c_str();
    createOpts.v.v0.user = request.username().c_str();
    createOpts.v.v0.passwd = request.password().c_str();
    createOpts.v.v0.bucket = request.bucket().c_str();

    assert(ioService != nullptr);

    lcb_create_boost_asio_io_opts(0, &createOpts.v.v3.io, ioService);

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

    err = lcb_connect(m_instance);
    if (err != LCB_SUCCESS) {
        throw err;
    }
}

Connection::~Connection() { lcb_destroy(m_instance); }

std::promise<cb::ConnectResponse> &Connection::connectPromise()
{
    return m_connectPromise;
}

void Connection::get(const MultiRequest<GetRequest> &request,
    std::promise<MultiResponse<GetResponse>> &&promise)
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

    auto promiseId =
        GetPromises::addPromise(std::move(promise), std::move(response));
    auto error = lcb_get(m_instance, reinterpret_cast<void *>(promiseId),
        requests.size(), commandsPtr.data());

    if (error != LCB_SUCCESS) {
        GetPromises::cancelPromise(
            promiseId, std::runtime_error(lcb_strerror(m_instance, error)));
    }
}

void Connection::store(const MultiRequest<StoreRequest> &request,
    std::promise<MultiResponse<StoreResponse>> &&promise)
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

    auto promiseId =
        StorePromises::addPromise(std::move(promise), std::move(response));
    auto error = lcb_store(m_instance, reinterpret_cast<void *>(promiseId),
        requests.size(), commandsPtr.data());

    if (error != LCB_SUCCESS) {
        StorePromises::cancelPromise(
            promiseId, std::runtime_error(lcb_strerror(m_instance, error)));
    }
}

void Connection::remove(const MultiRequest<RemoveRequest> &request,
    std::promise<MultiResponse<RemoveResponse>> &&promise)
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

    auto promiseId =
        RemovePromises::addPromise(std::move(promise), std::move(response));
    auto error = lcb_remove(m_instance, reinterpret_cast<void *>(promiseId),
        requests.size(), commandsPtr.data());

    if (error != LCB_SUCCESS) {
        RemovePromises::cancelPromise(
            promiseId, std::runtime_error(lcb_strerror(m_instance, error)));
    }
}

void Connection::arithmetic(const MultiRequest<ArithmeticRequest> &request,
    std::promise<MultiResponse<ArithmeticResponse>> &&promise)
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

    auto promiseId =
        ArithmeticPromises::addPromise(std::move(promise), std::move(response));
    auto error = lcb_arithmetic(m_instance, reinterpret_cast<void *>(promiseId),
        requests.size(), commandsPtr.data());

    if (error != LCB_SUCCESS) {
        ArithmeticPromises::cancelPromise(
            promiseId, std::runtime_error(lcb_strerror(m_instance, error)));
    }
}

void Connection::http(
    const HttpRequest &request, std::promise<HttpResponse> &&promise)
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

    auto promiseId =
        HttpPromises::addPromise(std::move(promise), std::move(response));
    auto error = lcb_make_http_request(m_instance,
        reinterpret_cast<void *>(promiseId), request.type(), &command, &req);

    if (error != LCB_SUCCESS) {
        ArithmeticPromises::cancelPromise(
            promiseId, std::runtime_error(lcb_strerror(m_instance, error)));
    }
}

void Connection::durability(const MultiRequest<DurabilityRequest> &request,
    const DurabilityRequestOptions &requestOptions,
    std::promise<MultiResponse<DurabilityResponse>> &&promise)
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

    auto promiseId =
        DurabilityPromises::addPromise(std::move(promise), std::move(response));
    auto error =
        lcb_durability_poll(m_instance, reinterpret_cast<void *>(promiseId),
            &options, requests.size(), commandsPtr.data());

    if (error != LCB_SUCCESS) {
        DurabilityPromises::cancelPromise(
            promiseId, std::runtime_error(lcb_strerror(m_instance, error)));
    }
}

} // namespace cb
