/**
 * @file asio-plugin.h
 * @author Bartek Kryza
 * @copyright (C) 2018
 * This software is released under the MIT license cited in 'LICENSE.md'
 */

/*
 * Based on https://github.com/couchbaselabs/libcouchbase-boost-asio
 */

#include <libcouchbase/couchbase.h>
#include <libcouchbase/iops.h>

#include <array>
#include <asio.hpp>
#include <chrono>
#include <queue>
#include <string>
#include <vector>

namespace cb {

// clang-format off
/**
@startuml
asio_iops <|-- lcb_io_opt_st
asio_queued_write_handler <|-- asio_write_handler
asio_socket <|-- lcb_sockdata_st


asio_timer ..> asio_iops
asio_callback_timer ..> asio_timer : m_parent
asio_callback_timer *-- lcb_ioE_callback : callback



@enduml
*/
// clang-format on

using asio::ip::tcp;
using std::error_code;
using namespace asio;

class asio_socket;
class asio_timer;
struct connect_handler;
struct read_handler;

class asio_iops : lcb_io_opt_st {
public:
    asio_iops(io_service *svc_in);

    void set_error();

    void set_error(const error_code &ec);

    void set_error(int err);

    io_service &get_service();

    void run_until_dead();

    void run();

    void stop();

private:
    asio::io_service *svc;
    asio::io_service svc_s;
    bool is_stopped;
    bool is_service_owner;
};

struct asio_write_handler {
    asio_write_handler(
        asio_socket *s, lcb_ioC_write2_callback callback, void *cbarg);

    void operator()(const error_code &ec, size_t);

    void operator()();

    asio_socket *sock;
    void *arg;
    lcb_ioC_write2_callback cb;
};

struct asio_queued_write_handler : asio_write_handler {
    asio_queued_write_handler(asio_socket *s, lcb_ioC_write2_callback callback,
        void *cbarg, std::vector<const_buffer> &_bufs);

    std::vector<const_buffer> bufs;
};

struct asio_connect_handler {
    asio_connect_handler(lcb_io_connect_cb cb, asio_socket *s);

    void operator()(const error_code &ec);

    lcb_io_connect_cb callback;
    asio_socket *sock;
};

struct asio_read_handler {
    asio_read_handler(asio_socket *s);

    void operator()(const error_code &ec, size_t nbytes);

    asio_socket *sock;
};

class asio_socket : public lcb_sockdata_st {
public:
    asio_socket(std::shared_ptr<asio_iops> _parent, int domain);
    ~asio_socket();

    int start_connect(const sockaddr *saddr, lcb_io_connect_cb callback);

    int start_read(
        lcb_IOV *iov, size_t n, void *uarg, lcb_ioC_read2_callback cb);

    void flush_wpend();

    int start_write(
        lcb_IOV *iov, size_t niov, void *uarg, lcb_ioC_write2_callback cb);

    int is_closed(int flags);

    int get_nameinfo(lcb_nameinfo_st *ni);

    void close();

    void ref();
    void unref();

private:
    tcp::socket m_socket;
    asio_iops *m_parent;
    lcb_ioC_read2_callback rdcb;
    void *rdarg;
    size_t refcount;
    size_t wcount;
    bool lcb_closed;
    std::queue<asio_queued_write_handler> pending_writes;
};

class asio_callback_timer {
    asio_callback_timer(std::shared_ptr<asio_timer> tm);

    void operator()(const error_code &ec);

    std::shared_ptr<asio_timer> parent();

private:
    std::shared_ptr<asio_timer> m_parent;
};

class asio_timer {
public:
    asio_timer(asio_iops *parent);

    ~asio_timer();

    void schedule(uint32_t usec, lcb_ioE_callback cb, void *_arg);
    void cancel();

private:
    asio::steady_timer m_timer;
    asio_iops *m_parent;
    lcb_ioE_callback callback;
    void *arg;
};

} // namespace cb
