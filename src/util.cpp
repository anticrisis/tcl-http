#include "util.h"

#include <algorithm>
#include <sstream>
#include <unordered_map>

TclObj::TclObj(Tcl_Obj* p) noexcept : ptr_{ p }
{
  if (ptr_)
    Tcl_IncrRefCount(ptr_);
}

TclObj::TclObj(TclObj const& other) noexcept : ptr_{ other.ptr_ }
{
  if (ptr_)
    Tcl_IncrRefCount(ptr_);
}

TclObj::~TclObj()
{
  if (ptr_)
    Tcl_DecrRefCount(ptr_);
}

TclObj&
TclObj::operator=(Tcl_Obj* obj) noexcept
{
  // discarding my obj
  if (ptr_)
    Tcl_DecrRefCount(ptr_);

  ptr_ = obj;
  Tcl_IncrRefCount(ptr_);
  return *this;
}

TclObj::TclObj(TclObj&& other) noexcept : ptr_{ other.ptr_ }
{
  // place other in moved-from state
  other.ptr_ = nullptr;
}

TclObj&
TclObj::operator=(TclObj const& other) noexcept
{
  // copy assignment is the same as copy assigning a naked pointer
  return operator=(other.ptr_);
}
TclObj&
TclObj::operator=(TclObj&& other) noexcept
{
  // discarding my obj
  if (ptr_)
    Tcl_DecrRefCount(ptr_);

  // Taking over other's obj. No need to adjust refcount.
  ptr_ = other.ptr_;

  // place other in moved-from state
  other.ptr_ = nullptr;

  return *this;
}

Tcl_Obj*
TclObj::value()
{
  if (ptr_ == nullptr)
    throw std::runtime_error("TclObj invalid.");
  return ptr_;
}

void
tolower(std::string& out)
{
  std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) {
    return std::tolower(c);
  });
}

std::string_view
get_string(Tcl_Obj* obj)
{
  int  length{ 0 };
  auto cs = Tcl_GetStringFromObj(obj, &length);
  return { cs, static_cast<size_t>(length) };
}
std::optional<http_tcl::headers>
get_dict(Tcl_Interp* interp, Tcl_Obj* dict)
{
  Tcl_DictSearch search;
  Tcl_Obj*       key;
  Tcl_Obj*       value;
  int            done{ 0 };

  http_tcl::headers headers;

  if (Tcl_DictObjFirst(interp, dict, &search, &key, &value, &done) != TCL_OK)
    return std::nullopt;

  // cleanup when leaving function
  auto _ = finally([&search] { Tcl_DictObjDone(&search); });

  while (done == 0)
  {
    headers.emplace(get_string(key), get_string(value));
    Tcl_DictObjNext(&search, &key, &value, &done);
  }
  return headers;
}

Tcl_Obj*
to_dict(Tcl_Interp* i, http_tcl::headers heads)
{
  auto dict = Tcl_NewDictObj();
  for (auto const& kv: heads)
  {
    auto k = Tcl_NewStringObj(kv.first.c_str(), -1);
    auto v = Tcl_NewStringObj(kv.second.c_str(), -1);
    Tcl_DictObjPut(i, dict, k, v);
  }
  return dict;
}

void
maybe_set_var(Tcl_Interp* i, Tcl_Obj* var_name, std::string_view val)
{
  auto var_name_sv = get_string(var_name);
  if (! var_name_sv.empty())
  {
    auto val_obj = Tcl_NewStringObj(val.data(), val.size());
    Tcl_ObjSetVar2(i,
                   var_name,
                   nullptr,
                   val_obj,
                   TCL_GLOBAL_ONLY | TCL_LEAVE_ERR_MSG);
  }
}

namespace url
{
std::string
percent_encode(std::string_view in)
{
  // ascii table: https://tools.ietf.org/html/rfc20
  // rfc3986: https://tools.ietf.org/html/rfc3986
  // performs additional encoding of whitespace characters

  static std::unordered_map<char, char const*> map{
    { ' ', "%20" },  { '\t', "%09" }, { '\r', "%0D" }, { '\n', "%0A" },
    { '\f', "%0C" }, { '\v', "%0B" }, { '!', "%21" },  { '#', "%23" },
    { '$', "%24" },  { '%', "%25" },  { '&', "%26" },  { '\'', "%27" },
    { '(', "%28" },  { ')', "%29" },  { '*', "%2A" },  { '+', "%2B" },
    { ',', "%2C" },  { '/', "%2F" },  { ':', "%3A" },  { ';', "%3B" },
    { '=', "%3D" },  { '?', "%3F" },  { '@', "%40" },  { '[', "%5B" },
    { ']', "%5D" },
  };

  std::ostringstream os;

  for (auto const& c: in)
  {
    if (auto p = map.find(c); p != map.end())
      os << p->second;
    else
      os << c;
  }
  return os.str();
}

std::optional<std::string>
percent_decode(std::string_view in)
{
  // argument must be a string& due to use of in.substr()
  std::ostringstream os;
  std::string        temp_s{ "XX" };

  for (size_t i = 0; i < in.size(); ++i)
  {
    if (in[i] == '%')
    {
      if (i + 3 > in.size())
        return std::nullopt;

      int value{ 0 };
      temp_s[0] = in[i + 1];
      temp_s[1] = in[i + 2];
      std::istringstream is{ temp_s };
      if (is >> std::hex >> value)
      {
        os << static_cast<char>(value);
        i += 2;
      }
      else
        return std::nullopt;
    }
    else if (in[i] == '+')
      os << ' ';
    else
      os << in[i];
  }
  return os.str();
}
} // namespace url
