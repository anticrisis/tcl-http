#pragma once
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <unordered_map>
#include <utility>

namespace http_tcl
{
using headers = std::unordered_map<std::string, std::string>;

class alt_handler
{
public:
  using head_r = std::tuple<int, std::optional<headers>, size_t, std::string>;
  using get_r
    = std::tuple<int, std::optional<headers>, std::string, std::string>;
  using options_r      = get_r;
  using post_r         = get_r;
  using put_r          = std::tuple<int, std::optional<headers>>;
  using delete_r       = get_r;
  using headers_access = std::function<headers()>;

  virtual options_r
  options(std::string_view target,
          std::string_view body,
          headers_access&& get_headers)
    = 0;

  virtual head_r
  head(std::string_view target, headers_access&& get_headers)
    = 0;

  virtual get_r
  get(std::string_view target, headers_access&& get_headers)
    = 0;

  virtual post_r
  post(std::string_view target,
       std::string_view body,
       headers_access&& get_headers)
    = 0;

  virtual put_r
  put(std::string_view target,
      std::string_view body,
      headers_access&& get_headers)
    = 0;

  virtual delete_r
  delete_(std::string_view target,
          std::string_view body,
          headers_access&& get_headers)
    = 0;
};

template <typename T>
class thread_safe_handler : public alt_handler
{
  std::mutex                   mutex_;
  std::shared_ptr<alt_handler> handler_;

public:
  options_r
  options(std::string_view target,
          std::string_view body,
          headers_access&& get_headers) override
  {
    std::lock_guard lock(mutex_);
    return static_cast<T*>(this)->do_options(target,
                                             body,
                                             std::move(get_headers));
  }
  head_r
  head(std::string_view target, headers_access&& get_headers) override
  {
    std::lock_guard lock(mutex_);
    return static_cast<T*>(this)->do_head(target, std::move(get_headers));
  }

  get_r
  get(std::string_view target, headers_access&& get_headers) override
  {
    std::lock_guard lock(mutex_);
    return static_cast<T*>(this)->do_get(target, std::move(get_headers));
  }

  post_r
  post(std::string_view target,
       std::string_view body,
       headers_access&& get_headers) override
  {
    std::lock_guard lock(mutex_);
    return static_cast<T*>(this)->do_post(target, body, std::move(get_headers));
  }

  put_r
  put(std::string_view target,
      std::string_view body,
      headers_access&& get_headers) override
  {
    std::lock_guard lock(mutex_);
    return static_cast<T*>(this)->do_put(target, body, std::move(get_headers));
  }

  delete_r
  delete_(std::string_view target,
          std::string_view body,
          headers_access&& get_headers) override
  {
    std::lock_guard lock(mutex_);
    return static_cast<T*>(this)->do_delete_(target,
                                             body,
                                             std::move(get_headers));
  }
};

// begin gsl - MIT License - https://github.com/microsoft/GSL

// final_action allows you to ensure something gets run at the end of a scope
template <class F>
class final_action
{
public:
  static_assert(! std::is_reference<F>::value && ! std::is_const<F>::value
                  && ! std::is_volatile<F>::value,
                "Final_action should store its callable by value");

  explicit final_action(F f) noexcept : f_(std::move(f)) {}

  final_action(final_action&& other) noexcept
      : f_(std::move(other.f_))
      , invoke_(std::exchange(other.invoke_, false))
  {
  }

  final_action(const final_action&) = delete;
  final_action&
  operator=(const final_action&)
    = delete;
  final_action&
  operator=(final_action&&)
    = delete;

  ~final_action() noexcept
  {
    if (invoke_)
      f_();
  }

private:
  F    f_;
  bool invoke_{ true };
};

// finally() - convenience function to generate a final_action
template <class F>
[[nodiscard]] final_action<
  typename std::remove_cv<typename std::remove_reference<F>::type>::type>
finally(F&& f) noexcept
{
  return final_action<
    typename std::remove_cv<typename std::remove_reference<F>::type>::type>(
    std::forward<F>(f));
}

// end gsl


int
run(std::string_view address_,
    unsigned short   port,
    alt_handler*     alt_handler,
    int              max_connections = 250);

std::tuple<int, headers, std::string>
http_client(std::string_view              method,
            std::string                   host,
            std::string                   port,
            std::string                   target,
            std::optional<headers> const& headers,
            std::string_view              body);

} // namespace http_tcl
