#pragma once

#include <string>
#include <utility>

namespace dxsplat {

struct Status {
  bool ok = true;
  std::string message;

  static Status Ok() { return {}; }
  static Status Error(std::string msg) { return {.ok = false, .message = std::move(msg)}; }
};

template <typename T>
struct StatusOr {
  Status status;
  T value{};

  static StatusOr<T> Error(std::string msg) { return {.status = Status::Error(std::move(msg))}; }
  static StatusOr<T> Ok(T v) { return {.status = Status::Ok(), .value = std::move(v)}; }
  bool ok() const { return status.ok; }
};

}  // namespace dxsplat
