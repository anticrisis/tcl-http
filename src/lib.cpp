#include "dllexport.h"
#include "http_tcl/http_tcl.h"
#include "util.h"

#include <algorithm>
#include <iostream>
#include <memory>
#include <optional>
#include <tcl.h>
#include <thread>
#include <vector>

// need a macro for compile-time string concatenation
#define theNamespaceName    "::act::http"
#define theUrlNamespaceName "::act::url"
static constexpr auto theParentNamespace = "::act";
static constexpr auto thePackageName     = "act_http";
static constexpr auto thePackageVersion  = "0.1";

// Configuration structure with refcounted Tcl objects. Managed using
// the 'http::configure' command.
//
// TCL callbacks have the following specifications:
//
//    OPTIONS: -> {status content content_type} or see below
//    HEAD:    -> {status content_length content_type}
//    GET:     -> {status content content_type}
//    POST:    -> {status content content_type}
//    PUT:     -> {status}
//    DELETE:  -> {status content content_type}
//
// where
//
//    target = string request path, e.g. "/foo"
//    body   = string request body
//    status = integer HTTP status code
//    content_length = integer to place in 'Content-Length' header
//    content_type   = string to place in 'Content-Type' header
//
// and
//
//    Each callback, not just OPTIONS, may optionally return an additional value
//    as the last element of the list. That value is a dictionary of key/value
//    pairs to add to the response headers. For example:
//
//    proc post {target body} {
//        list 200 "hello" "text/plain" {Set-Cookie foo X-Other-Header bar}
//    }
//
struct config_t
{
  bool   valid{ false };
  TclObj options{};
  TclObj head{};
  TclObj get{};
  TclObj post{};
  TclObj put{};
  TclObj delete_{};
  TclObj req_target{};
  TclObj req_body{};
  TclObj req_headers{};
  TclObj host{};
  TclObj port{};
  TclObj exit_target{};

  void
  init();
};

void
config_t::init()
{
  // call after Tcl_InitStubs
  auto empty_string = [] {
    return Tcl_NewStringObj("", 0);
  };

  options     = empty_string();
  head        = empty_string();
  get         = empty_string();
  post        = empty_string();
  put         = empty_string();
  delete_     = empty_string();
  req_target  = empty_string();
  req_body    = empty_string();
  req_headers = empty_string();
  host        = empty_string();
  port        = empty_string();
  exit_target = empty_string();
  valid       = true;
}

struct tcl_handler final : public http_tcl::thread_safe_handler<tcl_handler>
{
  Tcl_Interp* interp_;
  config_t    config_;

  void
  set_target(std::string_view target)
  {
    maybe_set_var(interp_, config_.req_target.value(), target);
  }

  void
  set_body(std::string_view body)
  {
    maybe_set_var(interp_, config_.req_body.value(), body);
  }

  void
  set_headers(headers_access&& get_headers)
  {
    // only ask server to copy headers out of its internal
    // structure if we're actually going to use them.
    auto var_name_sv = get_string(config_.req_headers.value());
    if (! var_name_sv.empty())
    {
      auto dict = to_dict(interp_, get_headers());
      Tcl_ObjSetVar2(interp_,
                     config_.req_headers.value(),
                     nullptr,
                     dict,
                     TCL_GLOBAL_ONLY | TCL_LEAVE_ERR_MSG);
    }
  }

  std::optional<int>
  get_int(Tcl_Obj* obj)
  {
    int val{ 0 };
    if (Tcl_GetIntFromObj(interp_, obj, &val) == TCL_OK)
      return val;
    return std::nullopt;
  }

  std::optional<std::tuple<int, Tcl_Obj**>>
  get_list(Tcl_Obj* list)
  {
    int       length{ 0 };
    Tcl_Obj** objv;
    if (Tcl_ListObjGetElements(interp_, list, &length, &objv) != TCL_OK)
      return std::nullopt;

    return std::make_tuple(length, objv);
  }

  std::optional<http_tcl::headers>
  get_dict(Tcl_Obj* dict)
  {
    return ::get_dict(interp_, dict);
  }

  std::optional<std::tuple<int, Tcl_Obj**>>
  eval_to_list(Tcl_Obj* obj)
  {
    if (Tcl_EvalObjEx(interp_, obj, TCL_EVAL_GLOBAL) != TCL_OK)
      return std::nullopt;

    return get_list(Tcl_GetObjResult(interp_));
  }

  std::string
  error_info()
  {
    if (auto cs = Tcl_GetVar(interp_, "errorInfo", TCL_GLOBAL_ONLY); cs)
      return cs;
    return "";
  }

public:
  void
  init(Tcl_Interp* i)
  {
    interp_ = i;
    config_.init();
  }

  auto&
  config()
  {
    return config_;
  }

  options_r
  do_options(std::string_view target,
             std::string_view body,
             headers_access&& get_headers)
  {
    constexpr auto req_args   = 3;
    const auto     make_error = [&](char const* msg = nullptr) -> options_r {
      return { 500, std::nullopt, msg ? msg : error_info(), "text/plain" };
    };

    // Not-so-secret back door to force exit, only if -exittarget is set. This
    // is used for test suites.
    if (auto exit = get_string(config_.exit_target.value()); ! exit.empty())
    {
      if (target == exit)
      {
        // exit after 50ms, hopefully enough time to cleanly complete the
        // response
        std::thread{
          [] {
            using namespace std::chrono_literals;
            std::this_thread::sleep_for(100ms);
            Tcl_Exit(0);
          }
        }.detach();

        return { 204, std::nullopt, "", "" };
      }
    }

    set_target(target);
    set_body(body);
    set_headers(std::move(get_headers));

    auto list = eval_to_list(config_.options.value());
    if (! list)
      return make_error();

    auto [objc, objv] = *list;

    if (objc < req_args)
      return make_error("wrong number of items returned from callback");

    auto sc           = get_int(*objv++);
    auto res_body     = get_string(*objv++);
    auto content_type = get_string(*objv++);

    std::optional<http_tcl::headers> headers;
    if (objc > req_args)
      headers = get_dict(*objv++);

    if (! sc)
      return make_error(
        "could not understand status code returned from callback");

    return { *sc,
             headers,
             { res_body.data(), res_body.size() },
             { content_type.data(), content_type.size() } };
  }

  head_r
  do_head(std::string_view target, headers_access&& get_headers)
  {
    static const auto error
      = std::make_tuple(500, std::nullopt, 0, "text/plain");
    constexpr auto req_args = 3;

    set_target(target);
    set_body("");
    set_headers(std::move(get_headers));

    auto list = eval_to_list(config_.head.value());
    if (! list)
      return error;

    auto [objc, objv] = *list;

    if (objc < req_args)
      return error;
    auto sc             = get_int(*objv++);
    auto content_length = get_int(*objv++);
    auto content_type   = get_string(*objv++);

    std::optional<http_tcl::headers> headers;
    if (objc > req_args)
      headers = get_dict(*objv++);

    if (! sc || ! content_length)
      return error;

    return { *sc,
             headers,
             static_cast<size_t>(*content_length),
             { content_type.data(), content_type.size() } };
  }

  get_r
  do_get(std::string_view target, headers_access&& get_headers)
  {
    constexpr auto req_args = 3;

    const auto make_error = [&](char const* msg = nullptr) -> get_r {
      return { 500, std::nullopt, msg ? msg : error_info(), "text/plain" };
    };

    set_target(target);
    set_body("");
    set_headers(std::move(get_headers));

    auto list = eval_to_list(config_.get.value());
    if (! list)
      return make_error();

    auto [objc, objv] = *list;

    if (objc < req_args)
      return make_error("wrong number of items returned from callback");

    auto sc           = get_int(*objv++);
    auto body         = get_string(*objv++);
    auto content_type = get_string(*objv++);

    std::optional<http_tcl::headers> headers;
    if (objc > req_args)
      headers = get_dict(*objv++);

    if (! sc)
      return make_error(
        "could not understand status code returned from callback");

    return { *sc,
             headers,
             { body.data(), body.size() },
             { content_type.data(), content_type.size() } };
  }

  post_r
  do_post(std::string_view target,
          std::string_view body,
          headers_access&& get_headers)
  {
    constexpr auto req_args   = 3;
    const auto     make_error = [&](char const* msg = nullptr) -> post_r {
      return { 500, std::nullopt, msg ? msg : error_info(), "text/plain" };
    };

    set_target(target);
    set_body(body);
    set_headers(std::move(get_headers));

    auto list = eval_to_list(config_.post.value());
    if (! list)
      return make_error();

    auto [objc, objv] = *list;

    if (objc < req_args)
      return make_error("wrong number of items returned from callback");

    auto sc           = get_int(*objv++);
    auto res_body     = get_string(*objv++);
    auto content_type = get_string(*objv++);

    std::optional<http_tcl::headers> headers;
    if (objc > req_args)
      headers = get_dict(*objv++);

    if (! sc)
      return make_error(
        "could not understand status code returned from callback");

    return { *sc,
             headers,
             { res_body.data(), res_body.size() },
             { content_type.data(), content_type.size() } };
  }

  put_r
  do_put(std::string_view target,
         std::string_view body,
         headers_access&& get_headers)
  {
    static const auto error    = std::make_tuple(500, http_tcl::headers{});
    constexpr auto    req_args = 1;

    set_target(target);
    set_body(body);
    set_headers(std::move(get_headers));

    auto list = eval_to_list(config_.put.value());
    if (! list)
      return error;

    auto [objc, objv] = *list;

    if (objc < req_args)
      return error;
    auto sc = get_int(*objv++);

    std::optional<http_tcl::headers> headers;
    if (objc > req_args)
      headers = get_dict(*objv++);

    if (! sc)
      return error;

    return { *sc, headers };
  }

  delete_r
  do_delete_(std::string_view target,
             std::string_view body,
             headers_access&& get_headers)
  {
    constexpr auto req_args   = 3;
    const auto     make_error = [&](char const* msg = nullptr) -> delete_r {
      return { 500, std::nullopt, msg ? msg : error_info(), "text/plain" };
    };

    set_target(target);
    set_body(body);
    set_headers(std::move(get_headers));

    auto list = eval_to_list(config_.delete_.value());
    if (! list)
      return make_error();

    auto [objc, objv] = *list;

    if (objc < req_args)
      return make_error("wrong number of items returned from callback");

    auto sc           = get_int(*objv++);
    auto res_body     = get_string(*objv++);
    auto content_type = get_string(*objv++);

    std::optional<http_tcl::headers> headers;
    if (objc > req_args)
      headers = get_dict(*objv++);

    if (! sc)
      return make_error(
        "could not understand status code returned from callback");

    return { *sc,
             headers,
             { res_body.data(), res_body.size() },
             { content_type.data(), content_type.size() } };
  }
};

struct client_data
{
  tcl_handler handler;

  void
  init(Tcl_Interp*);
};

void
client_data::init(Tcl_Interp* i)
{
  handler.init(i);
}

// global
client_data theClientData;

//

int
configure(ClientData cd, Tcl_Interp* i, int objc, Tcl_Obj* const objv[])
{
  static const char* options[] = { "-head",
                                   "-get",
                                   "-post",
                                   "-put",
                                   "-delete",
                                   "-reqtargetvariable",
                                   "-reqbodyvariable",
                                   "-reqheadersvariable",
                                   "-host",
                                   "-port",
                                   "-options",
                                   "-exittarget",
                                   nullptr };

  auto  cd_ptr    = static_cast<client_data*>(cd);
  auto& my_config = cd_ptr->handler.config();

  if (objc == 2)
  {
    // return value of single option
    int opt{ -1 };
    if (Tcl_GetIndexFromObj(i, objv[1], options, "option", 0, &opt) != TCL_OK)
      return TCL_ERROR;

    std::vector<Tcl_Obj*> objv;
    switch (opt)
    {
    case 0: objv.push_back(my_config.head.value()); break;
    case 1: objv.push_back(my_config.get.value()); break;
    case 2: objv.push_back(my_config.post.value()); break;
    case 3: objv.push_back(my_config.put.value()); break;
    case 4: objv.push_back(my_config.delete_.value()); break;
    case 5: objv.push_back(my_config.req_target.value()); break;
    case 6: objv.push_back(my_config.req_body.value()); break;
    case 7: objv.push_back(my_config.req_headers.value()); break;
    case 8: objv.push_back(my_config.host.value()); break;
    case 9: objv.push_back(my_config.port.value()); break;
    case 10: objv.push_back(my_config.options.value()); break;
    case 11: objv.push_back(my_config.exit_target.value()); break;
    default: return TCL_ERROR;
    }
    auto list = Tcl_NewListObj(objv.size(), objv.data());
    Tcl_SetObjResult(i, list);
    return TCL_OK;
  }

  // require odd number of arguments
  if (objc % 2 == 0)
  {
    Tcl_WrongNumArgs(
      i,
      objc,
      objv,
      "?-host host? ?-port port? ?-head headCmd? ?-get getCmd? "
      "?-post postCmd? ?-put "
      "putCmd? ?-delete delCmd? ?-options optCmd? ?-reqtargetvariable varName? "
      "?-reqbodyvariable varName? ?-reqheadersvariable varName? ?-exittarget "
      "target?");
    return TCL_ERROR;
  }

  if (objc == 1)
  {
    // return list of configuration
    std::vector<Tcl_Obj*> objv;
    objv.reserve(20);
    objv.push_back(Tcl_NewStringObj("-host", -1));
    objv.push_back(my_config.host.value());
    objv.push_back(Tcl_NewStringObj("-port", -1));
    objv.push_back(my_config.port.value());
    objv.push_back(Tcl_NewStringObj("-head", -1));
    objv.push_back(my_config.head.value());
    objv.push_back(Tcl_NewStringObj("-get", -1));
    objv.push_back(my_config.get.value());
    objv.push_back(Tcl_NewStringObj("-post", -1));
    objv.push_back(my_config.post.value());
    objv.push_back(Tcl_NewStringObj("-put", -1));
    objv.push_back(my_config.put.value());
    objv.push_back(Tcl_NewStringObj("-delete", -1));
    objv.push_back(my_config.delete_.value());
    objv.push_back(Tcl_NewStringObj("-options", -1));
    objv.push_back(my_config.options.value());
    objv.push_back(Tcl_NewStringObj("-reqtargetvariable", -1));
    objv.push_back(my_config.req_target.value());
    objv.push_back(Tcl_NewStringObj("-reqbodyvariable", -1));
    objv.push_back(my_config.req_body.value());
    objv.push_back(Tcl_NewStringObj("-reqheadersvariable", -1));
    objv.push_back(my_config.req_headers.value());
    objv.push_back(Tcl_NewStringObj("-exittarget", -1));
    objv.push_back(my_config.exit_target.value());

    auto list = Tcl_NewListObj(objv.size(), objv.data());
    Tcl_SetObjResult(i, list);
    return TCL_OK;
  }

  for (auto idx = 1; idx < objc - 1; idx += 2)
  {
    int opt{ -1 };
    if (Tcl_GetIndexFromObj(i, objv[idx], options, "option", 0, &opt) != TCL_OK)
      return TCL_ERROR;

    auto obj = objv[idx + 1];

    switch (opt)
    {
    case 0: my_config.head = obj; break;
    case 1: my_config.get = obj; break;
    case 2: my_config.post = obj; break;
    case 3: my_config.put = obj; break;
    case 4: my_config.delete_ = obj; break;
    case 5: my_config.req_target = obj; break;
    case 6: my_config.req_body = obj; break;
    case 7: my_config.req_headers = obj; break;
    case 8: my_config.host = obj; break;
    case 9: my_config.port = obj; break;
    case 10: my_config.options = obj; break;
    case 11: my_config.exit_target = obj; break;
    default: return TCL_ERROR;
    }
  }
  return TCL_OK;
}

int
http_client(ClientData cd, Tcl_Interp* i, int objc, Tcl_Obj* const objv[])
{
  static const char* options[]
    = { "-host", "-port", "-target", "-method", "-body", "-headers", nullptr };

  auto const error = [&i, &objc, &objv] {
    Tcl_WrongNumArgs(
      i,
      objc,
      objv,
      "?-host host? ?-port port? ?-target target? ?-method http-method? "
      "?-body body? ?-headers headerDict?");
    return TCL_ERROR;
  };

  // require odd number of arguments
  if (objc % 2 == 0)
    return error();

  std::string                      host;
  std::string                      port{ "80" };
  std::string                      target{ "/" };
  std::string                      method{ "get" };
  std::string                      body;
  std::optional<http_tcl::headers> headers;

  for (auto idx = 1; idx < objc - 1; idx += 2)
  {
    int opt{ -1 };
    if (Tcl_GetIndexFromObj(i, objv[idx], options, "option", 0, &opt) != TCL_OK)
      return TCL_ERROR;

    auto obj = objv[idx + 1];

    switch (opt)
    {
    case 0: host = get_string(obj); break;
    case 1: port = get_string(obj); break;
    case 2: target = get_string(obj); break;
    case 3: method = get_string(obj); break;
    case 4: body = get_string(obj); break;
    case 5: headers = get_dict(i, obj); break;
    default: return TCL_ERROR;
    }
  }

  if (host.empty())
    return error();

  tolower(method);
  auto [sc, heads, res_body]
    = http_tcl::http_client(method, host, port, target, headers, body);

  std::vector<Tcl_Obj*> resv{
    Tcl_NewStringObj(std::to_string(sc).c_str(), -1),
    to_dict(i, heads),
    Tcl_NewStringObj(res_body.c_str(), -1),
  };
  auto list = Tcl_NewListObj(resv.size(), resv.data());
  Tcl_SetObjResult(i, list);

  return TCL_OK;
}

int
run(ClientData cd, Tcl_Interp* i, int objc, Tcl_Obj* const objv[])
{
  auto  cd_ptr    = static_cast<client_data*>(cd);
  auto& my_config = cd_ptr->handler.config();

  auto host = Tcl_GetString(my_config.host.value());
  int  port{ 0 };
  if (Tcl_GetIntFromObj(i, my_config.port.value(), &port) != TCL_OK)
  {
    Tcl_SetObjResult(i, Tcl_NewStringObj("Invalid port number.", -1));
    return TCL_ERROR;
  }

  http_tcl::run(host, port, &cd_ptr->handler);
  return TCL_OK;
}

int
percent_encode(ClientData cd, Tcl_Interp* i, int objc, Tcl_Obj* const objv[])
{
  if (objc != 2)
  {
    Tcl_WrongNumArgs(i, objc, objv, "string");
    return TCL_ERROR;
  }

  auto in  = get_string(objv[1]);
  auto out = url::percent_encode(in);
  Tcl_SetObjResult(i, Tcl_NewStringObj(out.c_str(), out.size()));
  return TCL_OK;
}

int
percent_decode(ClientData cd, Tcl_Interp* i, int objc, Tcl_Obj* const objv[])
{
  if (objc != 2)
  {
    Tcl_WrongNumArgs(i, objc, objv, "string");
    return TCL_ERROR;
  }
  auto in  = get_string(objv[1]);
  auto out = url::percent_decode(in);
  if (out)
  {
    Tcl_SetObjResult(i, Tcl_NewStringObj(out->c_str(), out->size()));
    return TCL_OK;
  }
  else
  {
    Tcl_AddErrorInfo(i, "could not decode string.");
    return TCL_ERROR;
  }
}

extern "C"
{
  DllExport int
  Act_http_Init(Tcl_Interp* i)
  {
    if (Tcl_InitStubs(i, TCL_VERSION, 0) == nullptr)
      return TCL_ERROR;

    theClientData.init(i);

#define def(name, func)                                                        \
  Tcl_CreateObjCommand(i,                                                      \
                       theNamespaceName "::" name,                             \
                       (func),                                                 \
                       &theClientData,                                         \
                       nullptr)

#define urldef(name, func)                                                     \
  Tcl_CreateObjCommand(i,                                                      \
                       theUrlNamespaceName "::" name,                          \
                       (func),                                                 \
                       &theClientData,                                         \
                       nullptr)

    auto parent_ns
      = Tcl_CreateNamespace(i, theParentNamespace, nullptr, nullptr);

    auto ns     = Tcl_CreateNamespace(i, theNamespaceName, nullptr, nullptr);
    auto url_ns = Tcl_CreateNamespace(i, theUrlNamespaceName, nullptr, nullptr);

    def("configure", configure);
    def("run", run);
    def("client", http_client);

    urldef("encode", percent_encode);
    urldef("decode", percent_decode);

    if (Tcl_Export(i, ns, "*", 0) != TCL_OK)
      return TCL_ERROR;

    if (Tcl_Export(i, url_ns, "*", 0) != TCL_OK)
      return TCL_ERROR;

    if (Tcl_Export(i, parent_ns, "*", 0) != TCL_OK)
      return TCL_ERROR;

    Tcl_CreateEnsemble(i, theNamespaceName, ns, 0);
    Tcl_CreateEnsemble(i, theUrlNamespaceName, url_ns, 0);

    Tcl_PkgProvide(i, thePackageName, thePackageVersion);
    return TCL_OK;
#undef def
#undef urldef
  }

  DllExport int
  Act_http_Unload(Tcl_Interp* i, int flags)
  {
    auto ns = Tcl_FindNamespace(i, theNamespaceName, nullptr, 0);
    Tcl_DeleteNamespace(ns);

    // init client data again to free variables in the prior configuration
    theClientData.init(i);
    return TCL_OK;
  }
}
