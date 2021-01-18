#pragma once
#include "http_tcl/http_tcl.h"

#include <optional>
#include <string>
#include <string_view>
#include <tcl.h>

// A refcount managed wrapper arount Tcl_Obj*
class TclObj
{
private:
  Tcl_Obj* ptr_;

public:
  // default constructor initializes to nullptr
  TclObj() = default;

  // copy constructor
  TclObj(TclObj const& other) noexcept;

  // move constructor
  TclObj(TclObj&& other) noexcept;

  // construct from a naked pointer
  TclObj(Tcl_Obj* p) noexcept;

  // destructor decrements ref count
  ~TclObj();

  // copy assignment
  TclObj&
  operator=(TclObj const& other) noexcept;

  // copy assignment from a naked pointer increases ref
  // count
  TclObj&
  operator=(Tcl_Obj* obj) noexcept;

  // move assignment operator
  TclObj&
  operator=(TclObj&& other) noexcept;

  // throws if ptr_ is null
  Tcl_Obj*
  value();
};

// Assertions about TclObj
static_assert(std::is_nothrow_default_constructible<TclObj>{});
static_assert(std::is_nothrow_destructible<TclObj>{});
static_assert(std::is_nothrow_move_constructible<TclObj>{});
static_assert(std::is_nothrow_move_assignable<TclObj>{});
static_assert(std::is_nothrow_assignable<TclObj, Tcl_Obj*>{});
static_assert(std::is_nothrow_copy_constructible<TclObj>{});
static_assert(std::is_nothrow_copy_assignable<TclObj>{});

// Utility to execute a cleanup lambda when it goes out of scope.
template <typename F>
class finally
{
  F fun_;

public:
  explicit finally(F&& fun) : fun_{ std::move(fun) } {}
  ~finally() { fun_(); }
};

void
tolower(std::string& out);

std::string_view
get_string(Tcl_Obj* obj);

std::optional<http_tcl::headers>
get_dict(Tcl_Interp* interp, Tcl_Obj* dict);

Tcl_Obj*
to_dict(Tcl_Interp* i, http_tcl::headers heads);

void
maybe_set_var(Tcl_Interp* i, Tcl_Obj* var_name, std::string_view val);

namespace url
{
std::string
percent_encode(std::string_view in);

std::optional<std::string>
percent_decode(std::string_view in);
} // namespace url
