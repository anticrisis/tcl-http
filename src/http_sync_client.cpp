#include "http_tcl/http_tcl.h"

#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <cstdlib>
#include <iostream>
#include <string>

namespace beast = boost::beast; // from <boost/beast.hpp>
namespace http  = beast::http;  // from <boost/beast/http.hpp>
namespace net   = boost::asio;  // from <boost/asio.hpp>
using tcp       = net::ip::tcp; // from <boost/asio/ip/tcp.hpp>

namespace http_tcl
{
std::tuple<int, headers, std::string>
http_client(std::string_view              method,
            std::string                   host,
            std::string                   port,
            std::string                   target,
            std::optional<headers> const& headers,
            std::string_view              body)
{
  try
  {
    // The io_context is required for all I/O
    net::io_context ioc;

    // These objects perform our I/O
    tcp::resolver     resolver(ioc);
    beast::tcp_stream stream(ioc);

    // Look up the domain name
    auto const results = resolver.resolve(host, port);

    // Make the connection on the IP address we get from a lookup
    stream.connect(results);

    // Set up an HTTP request message
    http::verb verb = http::verb::get;

    // clang-format off
    if (method == "options")        verb = http::verb::options;
    else if (method == "head")      verb = http::verb::head;
    else if (method == "get")       verb = http::verb::get;
    else if (method == "post")      verb = http::verb::post;
    else if (method == "put")       verb = http::verb::put;
    else if (method == "delete")    verb = http::verb::delete_;
    // clang-format on

    http::request<http::string_body> req{ verb, target, 11 };
    req.set(http::field::host, host);
    req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
    if (headers)
      for (auto& kv: *headers)
      {
        req.base().set(kv.first, kv.second);
      }

    if (! body.empty())
    {
      req.set(http::field::content_length, std::to_string(body.size()));
      req.body() = body;
    }

    // Send the HTTP request to the remote host
    http::write(stream, req);

    // This buffer is used for reading and must be persisted
    beast::flat_buffer buffer;

    // Declare a container to hold the response
    http::response<http::dynamic_body> res;

    // Receive the HTTP response
    http::read(stream, buffer, res);

    // Write the message to standard out
    // std::cout << res << std::endl;

    // Gracefully close the socket
    beast::error_code ec;
    stream.socket().shutdown(tcp::socket::shutdown_both, ec);

    // not_connected happens sometimes
    // so don't bother reporting it.
    //
    if (ec && ec != beast::errc::not_connected)
      throw beast::system_error{ ec };

    // If we get here then the connection is closed gracefully

    http_tcl::headers res_head;
    for (auto const& kv: res.base())
    {
      std::string k{ kv.name_string() };
      std::string v{ kv.value() };
      res_head.insert({ k, v });
    }

    return {
      static_cast<int>(res.result_int()),
      res_head,
      beast::buffers_to_string(res.body().data()),
    };
  }
  catch (std::exception const& e)
  {
    return { 500, http_tcl::headers{}, e.what() };
  }
}

} // namespace http_tcl
