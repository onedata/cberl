/**
 * @file asio_io_opts.h
 * @author Bartek Kryza
 * @copyright (C) 2018
 * This software is released under the MIT license cited in 'LICENSE.md'
 */

#ifndef LCB_PLUGIN_ASIO_IO_OPTS_H
#define LCB_PLUGIN_ASIO_IO_OPTS_H 1

#include <libcouchbase/iops.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Create an instance of an event handler that utilize asio for
 * event notification.
 *
 * @param version the API version to use
 * @param loop the event loop (asio::io_service *) to hook use (please
 *             note that you shouldn't reference the event loop from
 *             multiple threads)
 * @param io a pointer to a newly created and initialized event handler
 * @return status of the operation
 */
LIBCOUCHBASE_API
lcb_error_t lcb_create_boost_asio_io_opts(
    int version, lcb_io_opt_st **io, void *arg);

#ifdef __cplusplus
}
#endif

#endif
