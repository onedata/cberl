/**
 * @file asio_plugin.cc
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

    void set_error() { set_error(errno); }

    void set_error(const error_code &ec) { set_error(ec.value()); }

    void set_error(int err) { LCB_IOPS_ERRNO(this) = err; }

    io_service &get_service() { return *svc; }

    void run_until_dead()
    {
        if (!is_service_owner) {
            return;
        }

        ref();
        while (refcount > 2) {
            svc->reset();
            svc->run();
        }
        delete this;
    }

    void run()
    {
        if (!is_service_owner) {
            return;
        }

        is_stopped = false;
        svc->reset();
        svc->run();
        is_stopped = true;
    }

    void stop()
    {
        if (!is_service_owner) {
            return;
        }

        if (is_stopped) {
            return;
        }
        is_stopped = true;
        svc->stop();
    }

private:
    asio::io_service *svc;
    asio::io_service svc_s;
    bool is_stopped;
    bool is_service_owner;
};

class asio_socket : public lcb_sockdata_st {
public:
    asio_socket(std::shared_ptr<asio_iops> _parent, int domain)
        : m_socket(_parent->get_service())
    {

        ::memset((lcb_sockdata_st *)this, 0, sizeof(lcb_sockdata_st));
        m_parent = _parent;
        rdarg = nullptr;
        rdcb = nullptr;
        refcount = 1;
        wcount = 0;
        lcb_closed = false;

        ip::address ipaddr;
        if (domain == AF_INET) {
            ipaddr = (ip::address)ip::address_v4();
            m_socket.open(tcp::v4());
        }
        else {
            ipaddr = (ip::address)ip::address_v6();
            m_socket.open(tcp::v6());
        }
        m_socket.bind(tcp::endpoint(ipaddr, 0));
        m_parent->ref();
        socket = m_socket.native_handle();
    }

    ~asio_socket() { m_parent->unref(); }

    int start_connect(const sockaddr *saddr, lcb_io_connect_cb callback)
    {
        ip::address ipaddr;
        int port;
        if (m_socket.local_endpoint().address().is_v4()) {
            const sockaddr_in *sin = (const sockaddr_in *)saddr;
            port = sin->sin_port;
            ipaddr = ip::address_v4(htonl(sin->sin_addr.s_addr));
        }
        else if (m_socket.local_endpoint().address().is_v6()) {
            const sockaddr_in6 *sin6 = (const sockaddr_in6 *)saddr;
            port = sin6->sin6_port;
            std::array<unsigned char, 16> addrbytes;
            ::memcpy(addrbytes.data(), &sin6->sin6_addr, 16);
            ipaddr = ip::address_v6(addrbytes, sin6->sin6_scope_id);
        }
        else {
            m_parent->set_error(ENOTSUP);
            return -1;
        }

        port = htons(port);
        m_socket.async_connect(
            tcp::endpoint(ipaddr, port), connect_handler(callback, this));
        return 0;
    }

    int start_read(
        lcb_IOV *iov, size_t n, void *uarg, lcb_ioC_read2_callback cb)
    {
        rdcb = cb;
        rdarg = uarg;

        ref();
        std::vector<mutable_buffer> bufs;
        for (size_t ii = 0; ii < n; ii++) {
            bufs.push_back(mutable_buffer(iov[ii].iov_base, iov[ii].iov_len));
        }
        m_socket.async_read_some(bufs, read_handler(this));
        return 0;
    }

    struct write_handler {
        asio_socket *sock;
        void *arg;
        lcb_ioC_write2_callback cb;
        write_handler(
            asio_socket *s, lcb_ioC_write2_callback callback, void *cbarg)
            : sock(s)
            , arg(cbarg)
            , cb(callback)
        {
        }

        void operator()(const error_code &ec, size_t)
        {
            int val = 0;
            if (ec) {
                val = -1;
                sock->m_parent->set_error(ec);
            }
            sock->wcount--;
            cb(sock, val, arg);
            sock->flush_wpend();
            sock->unref();
        }

        void operator()()
        {
            sock->wcount--;
            cb(sock, -1, arg);
            sock->unref();
        }
    };

    struct queued_write_handler : write_handler {
        queued_write_handler(asio_socket *s, lcb_ioC_write2_callback callback,
            void *cbarg, std::vector<const_buffer> &_bufs)
            : write_handler(s, callback, cbarg)
            , bufs(_bufs)
        {
        }
        std::vector<const_buffer> bufs;
    };

    void flush_wpend()
    {
        while (!pending_writes.empty()) {
            queued_write_handler w = pending_writes.front();
            pending_writes.pop();
            if (!lcb_closed) {
                async_write(m_socket, w.bufs, w);
                break;
            }
            else {
                m_parent->get_service().post(w);
            }
        }
    }

    int start_write(
        lcb_IOV *iov, size_t niov, void *uarg, lcb_ioC_write2_callback cb)
    {
        ref();

        std::vector<const_buffer> bufs;
        for (size_t ii = 0; ii < niov; ii++) {
            bufs.push_back(const_buffer(iov[ii].iov_base, iov[ii].iov_len));
        }

        if (lcb_closed) {
            m_parent->set_error(ESHUTDOWN);
            return -1;
        }

        if (pending_writes.empty() && wcount == 0) {
            async_write(m_socket, bufs, write_handler(this, cb, uarg));
        }
        else {
            pending_writes.push(queued_write_handler(this, cb, uarg, bufs));
        }
        wcount++;
        return 0;
    }

    int is_closed(int flags)
    {
        if (!m_socket.is_open()) {
            return LCB_IO_SOCKCHECK_STATUS_CLOSED;
        }
        error_code ec;
        char buf;
        mutable_buffers_1 dummy(&buf, 1);

        while (true) {
            bool was_blocking = m_socket.non_blocking();
            m_socket.non_blocking(true, ec);
            if (ec) {
                return LCB_IO_SOCKCHECK_STATUS_UNKNOWN;
            }
            size_t nr = m_socket.receive(dummy, tcp::socket::message_peek, ec);
            m_socket.non_blocking(was_blocking);
            if (ec) {
                if (ec == error::would_block) {
                    return LCB_IO_SOCKCHECK_STATUS_OK;
                }
                else if (ec == error::interrupted) {
                    continue;
                }
                else {
                    return LCB_IO_SOCKCHECK_STATUS_CLOSED;
                }
            }
            else if (nr > 0 && (flags & LCB_IO_SOCKCHECK_PEND_IS_ERROR)) {
                return LCB_IO_SOCKCHECK_STATUS_CLOSED;
            }
            else {
                return LCB_IO_SOCKCHECK_STATUS_OK;
            }
        }
        return LCB_IO_SOCKCHECK_STATUS_UNKNOWN;
    }

    int get_nameinfo(lcb_nameinfo_st *ni)
    {
        int rv;
        socklen_t lenp;

        lenp = sizeof(sockaddr_storage);
        rv = getsockname(socket, ni->local.name, &lenp);

        if (rv != 0) {
            m_parent->set_error();
            return -1;
        }

        lenp = sizeof(sockaddr_storage);
        rv = getpeername(socket, ni->remote.name, &lenp);
        if (rv != 0) {
            m_parent->set_error();
            return -1;
        }
        *ni->local.len = lenp;
        *ni->remote.len = lenp;
        return 0;
    }

    void close()
    {
        error_code ecdummy;
        m_socket.shutdown(socket_base::shutdown_both, ecdummy);
        m_socket.close(ecdummy);
        lcb_closed = true;
        unref();
    }
    void ref() { refcount++; }
    void unref()
    {
        if (!--refcount) {
            delete this;
        }
    }

private:
    tcp::socket m_socket;
    asio_iops *m_parent;
    lcb_ioC_read2_callback rdcb;
    void *rdarg;
    size_t refcount;
    size_t wcount;
    bool lcb_closed;
    std::queue<queued_write_handler> pending_writes;

    struct connect_handler {
        connect_handler(lcb_io_connect_cb cb, asio_socket *s)
            : callback(cb)
            , sock(s)
        {
        }
        lcb_io_connect_cb callback;
        asio_socket *sock;
        void operator()(const error_code &ec)
        {
            int rv = ec ? -1 : 0;
            if (ec) {
                sock->m_parent->set_error(ec);
            }
            callback((lcb_sockdata_st *)sock, rv);
        }
    };

    struct read_handler {
        asio_socket *sock;
        read_handler(asio_socket *s)
            : sock(s)
        {
        }

        void operator()(const error_code &ec, size_t nbytes)
        {
            ssize_t val = -1;
            if (ec) {
                sock->m_parent->set_error(ec);
            }
            else {
                val = nbytes;
            }

            sock->rdcb(sock, val, sock->rdarg);
            sock->unref();
        }
    };
};

class asio_timer {
public:
    struct H_Timer {
        asio_timer *parent;
        H_Timer(asio_timer *tm)
            : parent(tm)
        {
        }
        void operator()(const error_code &ec)
        {
            if (ec) {
                return;
            }
            parent->callback(-1, 0, parent->arg);
        }
    };

    asio_timer(asio_iops *parent)
        : m_timer(parent->get_service())
        , m_parent(parent)
    {
        callback = nullptr;
        arg = nullptr;
        m_parent->ref();
    }

    ~asio_timer() { m_parent->unref(); }

    void schedule(uint32_t usec, lcb_ioE_callback cb, void *_arg)
    {
        this->callback = cb;
        this->arg = _arg;
        m_timer.expires_from_now(std::chrono::microseconds(usec));
        m_timer.async_wait(H_Timer(this));
    }

    void cancel()
    {
        callback = nullptr;
        arg = nullptr;
        m_timer.cancel();
    }

private:
    asio::steady_timer m_timer;
    asio_iops *m_parent;
    lcb_ioE_callback callback;
    void *arg;
};

static asio_iops *getIops(lcb_io_opt_st *io)
{
    return reinterpret_cast<asio_iops *>(io);
}

extern "C" {
static void run_loop(lcb_io_opt_st *io) { getIops(io)->run(); }

static void stop_loop(lcb_io_opt_st *io) { getIops(io)->stop(); }

static void *create_timer(lcb_io_opt_st *io)
{
    return new asio_timer(getIops(io));
}

static void destroy_timer(lcb_io_opt_st *, void *timer)
{
    delete (asio_timer *)timer;
}

static int schedule_timer(
    lcb_io_opt_st *, void *timer, uint32_t us, void *arg, lcb_ioE_callback cb)
{
    ((asio_timer *)timer)->schedule(us, cb, arg);
    return 0;
}

static void cancel_timer(lcb_io_opt_st *, void *timer)
{
    ((asio_timer *)timer)->cancel();
}

static lcb_sockdata_st *create_socket(lcb_io_opt_st *io, int domain, int, int)
{
    return new asio_socket(getIops(io), domain);
}

static int connect_socket(lcb_io_opt_st *, lcb_sockdata_st *sock,
    const sockaddr *addr, unsigned, lcb_io_connect_cb cb)
{
    return ((asio_socket *)sock)->start_connect(addr, cb);
}

static int get_nameinfo(
    lcb_io_opt_st *, lcb_sockdata_st *sock, lcb_nameinfo_st *ni)
{
    return ((asio_socket *)sock)->get_nameinfo(ni);
}

static int read_socket(lcb_io_opt_st *, lcb_sockdata_st *sock, lcb_IOV *iov,
    lcb_SIZE niov, void *uarg, lcb_ioC_read2_callback cb)
{
    return ((asio_socket *)sock)->start_read(iov, niov, uarg, cb);
}

static int write_socket(lcb_io_opt_st *, lcb_sockdata_st *sock, lcb_IOV *iov,
    lcb_SIZE niov, void *uarg, lcb_ioC_write2_callback cb)
{
    return ((asio_socket *)sock)->start_write(iov, niov, uarg, cb);
}

static unsigned close_socket(lcb_io_opt_st *, lcb_sockdata_st *sock)
{
    ((asio_socket *)sock)->close();
    return 0;
}

static int check_closed(lcb_io_opt_st *, lcb_sockdata_st *sock, int flags)
{
    return ((asio_socket *)sock)->is_closed(flags);
}

static void iops_dtor(lcb_io_opt_st *io) { getIops(io)->run_until_dead(); }

static void get_procs(int, lcb_loop_procs *loop, lcb_timer_procs *tm,
    lcb_bsd_procs *, lcb_ev_procs *, lcb_completion_procs *iocp,
    lcb_iomodel_t *model)
{
    *model = LCB_IOMODEL_COMPLETION;
    loop->start = run_loop;
    loop->stop = stop_loop;

    tm->create = create_timer;
    tm->destroy = destroy_timer;
    tm->cancel = cancel_timer;
    tm->schedule = schedule_timer;

    iocp->socket = create_socket;
    iocp->connect = connect_socket;
    iocp->nameinfo = get_nameinfo;
    iocp->read2 = read_socket;
    iocp->write2 = write_socket;
    iocp->close = close_socket;
    iocp->is_closed = check_closed;

    iocp->write = nullptr;
    iocp->wballoc = nullptr;
    iocp->wbfree = nullptr;
    iocp->serve = nullptr;
}
}

asio_iops::asio_iops(io_service *svc_in)
{
    ::memset((lcb_io_opt_st *)this, 0, sizeof(lcb_io_opt_st));
    refcount = 1;
    is_stopped = false;
    version = 2;
    v.v2.get_procs = get_procs;
    destructor = iops_dtor;

    if (svc_in != nullptr) {
        svc = svc_in;
        is_service_owner = false;
    }
    else {
        svc = &svc_s;
        is_service_owner = true;
    }
}
}

extern "C" {
lcb_error_t lcb_create_asio_io_opts(
    int, lcb_io_opt_st **io, std::shared_ptr<lcb_io_opt_st> iops)
{
    assert(io.get() != nullptrptr);
    assert(arg != nullptrptr);

    *io = (lcb_io_opt_st *)iops.get();

    return LCB_SUCCESS;
}
}
