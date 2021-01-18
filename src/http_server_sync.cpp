//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
// Portions Copyright (c) 2021 anticrisis <https://github.com/anticrisis>
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

//------------------------------------------------------------------------------
//
// Example: HTTP server, synchronous
//
// Changes by anticrisis marked with 'anticrisis'
//
//------------------------------------------------------------------------------

// anticrisis: include header
#include "http_tcl/http_tcl.h"

#include <atomic>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/config.hpp>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <thread>

// anticrisis: add namespace
namespace http_tcl
{
namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http  = beast::http;          // from <boost/beast/http.hpp>
namespace net   = boost::asio;          // from <boost/asio.hpp>
using tcp       = boost::asio::ip::tcp; // from <boost/asio/ip/tcp.hpp>

//------------------------------------------------------------------------------

// This function produces an HTTP response for the given
// request. The type of the response object depends on the
// contents of the request, so the interface requires the
// caller to pass a generic lambda for receiving the response.
// anticrisis: remove support for doc_root and static files; add support for
// alt_handler
template <class Body, class Allocator, class Send>
void
handle_request(alt_handler&                                         alt_handler,
               http::request<Body, http::basic_fields<Allocator>>&& req,
               Send&&                                               send)
{
  // Returns a bad request response
  auto const bad_request = [&req](beast::string_view why) {
    http::response<http::string_body> res{ http::status::bad_request,
                                           req.version() };
    res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
    res.set(http::field::content_type, "text/html");
    res.keep_alive(req.keep_alive());
    res.body() = std::string(why);
    res.prepare_payload();
    return res;
  };

  // Returns a not found response
  auto const not_found = [&req](beast::string_view target) {
    http::response<http::string_body> res{ http::status::not_found,
                                           req.version() };
    res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
    res.set(http::field::content_type, "text/html");
    res.keep_alive(req.keep_alive());
    res.body() = "The resource '" + std::string(target) + "' was not found.";
    res.prepare_payload();
    return res;
  };

  // Returns a server error response
  auto const server_error = [&req](beast::string_view what) {
    http::response<http::string_body> res{ http::status::internal_server_error,
                                           req.version() };
    res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
    res.set(http::field::content_type, "text/html");
    res.keep_alive(req.keep_alive());
    res.body() = "An error occurred: '" + std::string(what) + "'";
    res.prepare_payload();
    return res;
  };

  // anticrisis
  auto const send_no_content
    = [&send, &req](int status, std::optional<headers>&& headers) {
        http::response<http::empty_body> res{ static_cast<http::status>(status),
                                              req.version() };
        res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
        if (headers)
          for (auto& kv: *headers)
          {
            res.base().set(kv.first, std::move(kv.second));
          }
        res.keep_alive(req.keep_alive());
        return send(std::move(res));
      };

  auto const send_empty = [&send, &req](int                      status,
                                        std::optional<headers>&& headers,
                                        size_t                   content_size,
                                        std::string&&            content_type) {
    http::response<http::empty_body> res{ static_cast<http::status>(status),
                                          req.version() };
    res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
    res.set(http::field::content_type, content_type);
    res.content_length(content_size);
    if (headers)
      for (auto& kv: *headers)
      {
        res.base().set(kv.first, std::move(kv.second));
      }
    res.keep_alive(req.keep_alive());
    return send(std::move(res));
  };

  auto const send_body = [&send, &req](int                      status,
                                       std::optional<headers>&& headers,
                                       std::string&&            body,
                                       std::string&&            content_type) {
    http::response<http::string_body> res{ static_cast<http::status>(status),
                                           req.version() };
    res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
    res.set(http::field::content_type, content_type);
    res.content_length(body.size());
    if (headers)
      for (auto& kv: *headers)
      {
        res.base().set(kv.first, std::move(kv.second));
      }
    res.body() = std::move(body);
    res.keep_alive(req.keep_alive());
    return send(std::move(res));
  };

  auto get_headers = [&req]() {
    http_tcl::headers hs;
    for (auto const& kv: req.base())
    {
      hs.emplace(kv.name_string(), kv.value());
    }
    return hs;
  };

  // Make sure we can handle the method
  // anticrisis: add methods
  if (req.method() != http::verb::get && req.method() != http::verb::head
      && req.method() != http::verb::post && req.method() != http::verb::put
      && req.method() != http::verb::delete_
      && req.method() != http::verb::options)
    return send(bad_request("Unknown HTTP-method"));

  // Request path must be absolute and not contain "..".
  if (req.target().empty() || req.target()[0] != '/'
      || req.target().find("..") != beast::string_view::npos)
    return send(bad_request("Illegal request-target"));

  // anticrisis: replace doc_root support with alt_handler
  if (req.method() == http::verb::options)
  {
    auto [status, headers, body, content_type]
      = alt_handler.options({ req.target().data(), req.target().size() },
                            { req.body().data(), req.body().size() },
                            std::move(get_headers));
    return send_body(status,
                     std::move(headers),
                     std::move(body),
                     std::move(content_type));
  }
  else if (req.method() == http::verb::head)
  {
    auto [status, headers, size, content_type]
      = alt_handler.head({ req.target().data(), req.target().size() },
                         std::move(get_headers));
    return send_empty(status,
                      std::move(headers),
                      size,
                      std::move(content_type));
  }
  else if (req.method() == http::verb::get)
  {
    auto [status, headers, body, content_type]
      = alt_handler.get({ req.target().data(), req.target().size() },
                        std::move(get_headers));
    return send_body(status,
                     std::move(headers),
                     std::move(body),
                     std::move(content_type));
  }
  else if (req.method() == http::verb::post)
  {
    auto [status, headers, body, content_type]
      = alt_handler.post({ req.target().data(), req.target().size() },
                         { req.body().data(), req.body().size() },
                         std::move(get_headers));
    return send_body(status,
                     std::move(headers),
                     std::move(body),
                     std::move(content_type));
  }
  else if (req.method() == http::verb::put)
  {
    auto [status, headers]
      = alt_handler.put({ req.target().data(), req.target().size() },
                        { req.body().data(), req.body().size() },
                        std::move(get_headers));
    return send_no_content(status, std::move(headers));
  }
  else if (req.method() == http::verb::delete_)
  {
    auto [status, headers, body, content_type]
      = alt_handler.delete_({ req.target().data(), req.target().size() },
                            { req.body().data(), req.body().size() },
                            std::move(get_headers));
    return send_body(status,
                     std::move(headers),
                     std::move(body),
                     std::move(content_type));
  }

  return send(server_error("not implemented."));
}

//------------------------------------------------------------------------------

// Report a failure
void
fail(beast::error_code ec, char const* what)
{
  // anticrisis: ignore these common errors
  if (ec == net::error::operation_aborted || ec == beast::error::timeout
      || ec == net::error::connection_reset)
    return;

  std::cerr << what << ": " << ec.message() << "\n";
}

// This is the C++11 equivalent of a generic lambda.
// The function object is used to send an HTTP message.
template <class Stream>
struct send_lambda
{
  Stream&            stream_;
  bool&              close_;
  beast::error_code& ec_;

  explicit send_lambda(Stream& stream, bool& close, beast::error_code& ec)
      : stream_(stream)
      , close_(close)
      , ec_(ec)
  {
  }

  template <bool isRequest, class Body, class Fields>
  void
  operator()(http::message<isRequest, Body, Fields>&& msg) const
  {
    // Determine if we should close the connection after
    close_ = msg.need_eof();

    // We need the serializer here because the serializer requires
    // a non-const file_body, and the message oriented version of
    // http::write only works with const messages.
    http::serializer<isRequest, Body, Fields> sr{ msg };
    http::write(stream_, sr, ec_);
  }
};

// Handles an HTTP server connection
void
do_session(tcp::socket& socket, alt_handler* alt_handler)
{
  bool              close = false;
  beast::error_code ec;

  // This buffer is required to persist across reads
  beast::flat_buffer buffer;

  // This lambda is used to send messages
  send_lambda<tcp::socket> lambda{ socket, close, ec };

  for (;;)
  {
    // Read a request
    http::request<http::string_body> req;
    http::read(socket, buffer, req, ec);
    if (ec == http::error::end_of_stream)
      break;
    if (ec)
      return fail(ec, "read");

    // Send the response
    // anticrisis: remove doc_root
    handle_request(*alt_handler, std::move(req), lambda);
    if (ec)
      return fail(ec, "write");
    if (close)
    {
      // This means we should close the connection, usually because
      // the response indicated the "Connection: close" semantic.
      break;
    }
  }

  // Send a TCP shutdown
  socket.shutdown(tcp::socket::shutdown_send, ec);

  // At this point the connection is closed gracefully
}

//------------------------------------------------------------------------------

// anticrisis: change main to run; remove doc_root
int
run(std::string_view address_, unsigned short port, alt_handler* alt_handler)
{
  try
  {
    auto const address = net::ip::make_address(address_);

    // The io_context is required for all I/O
    net::io_context ioc{ 1 };

    // The acceptor receives incoming connections
    tcp::acceptor acceptor{ ioc, { address, port } };
    for (;;)
    {
      // This will receive the new connection
      tcp::socket socket{ ioc };

      // Block until we get a connection
      acceptor.accept(socket);

      // Launch the session, transferring ownership of the socket
      std::thread{ std::bind(&do_session, std::move(socket), alt_handler) }
        .detach();
    }
  }
  catch (const std::exception& e)
  {
    std::cerr << "Error: " << e.what() << std::endl;
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}

//------------------------------------------------------------------------------

alt_handler::options_r
options(std::string_view target)
{
  return { 404, headers{}, "", "" };
}
alt_handler::head_r
head(std::string_view target)
{
  return { 404, headers{}, 0, "" };
}
alt_handler::get_r
get(std::string_view target)
{
  return { 404, headers{}, "", "" };
}
alt_handler::post_r
post(std::string_view target, std::string_view body)
{
  return { 404, headers{}, "", "" };
}
alt_handler::put_r
put(std::string_view target, std::string_view body)
{
  return { 404, headers{} };
}
alt_handler::delete_r
delete_(std::string_view target, std::string_view body)
{
  return { 404, headers{}, "", "" };
}
} // namespace http_tcl
