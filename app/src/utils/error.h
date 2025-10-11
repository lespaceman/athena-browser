#ifndef ATHENA_UTILS_ERROR_H_
#define ATHENA_UTILS_ERROR_H_

#include <string>
#include <variant>
#include <stdexcept>

namespace athena {
namespace utils {

// Error class representing a failure condition
class Error {
 public:
  explicit Error(const std::string& message)
      : message_(message), code_(0) {}

  Error(int code, const std::string& message)
      : message_(message), code_(code) {}

  const std::string& Message() const { return message_; }
  int Code() const { return code_; }

  std::string ToString() const {
    if (code_ != 0) {
      return "Error(" + std::to_string(code_) + "): " + message_;
    }
    return "Error: " + message_;
  }

 private:
  std::string message_;
  int code_;
};

// Result<T, E> type for error handling
// Inspired by Rust's Result type
template<typename T, typename E = Error>
class Result {
 public:
  // Construct from value
  Result(const T& value) : data_(value) {}
  Result(T&& value) : data_(std::move(value)) {}

  // Construct from error
  Result(const E& error) : data_(error) {}
  Result(E&& error) : data_(std::move(error)) {}

  // Check if result contains value
  bool IsOk() const {
    return std::holds_alternative<T>(data_);
  }

  // Check if result contains error
  bool IsError() const {
    return std::holds_alternative<E>(data_);
  }

  // Get value (throws if error)
  const T& Value() const & {
    if (IsError()) {
      throw std::runtime_error("Attempted to access value of error result: " +
                               GetError().ToString());
    }
    return std::get<T>(data_);
  }

  T& Value() & {
    if (IsError()) {
      throw std::runtime_error("Attempted to access value of error result: " +
                               GetError().ToString());
    }
    return std::get<T>(data_);
  }

  T&& Value() && {
    if (IsError()) {
      throw std::runtime_error("Attempted to access value of error result: " +
                               GetError().ToString());
    }
    return std::move(std::get<T>(data_));
  }

  // Get error (throws if value)
  const E& GetError() const & {
    if (IsOk()) {
      throw std::runtime_error("Attempted to access error of ok result");
    }
    return std::get<E>(data_);
  }

  E& GetError() & {
    if (IsOk()) {
      throw std::runtime_error("Attempted to access error of ok result");
    }
    return std::get<E>(data_);
  }

  // Get value or default
  T ValueOr(const T& default_value) const & {
    return IsOk() ? std::get<T>(data_) : default_value;
  }

  T ValueOr(T&& default_value) && {
    return IsOk() ? std::move(std::get<T>(data_)) : std::move(default_value);
  }

  // Conversion operators for convenience
  explicit operator bool() const {
    return IsOk();
  }

  bool operator!() const {
    return IsError();
  }

 private:
  std::variant<T, E> data_;
};

// Specialization for void return type
template<typename E>
class Result<void, E> {
 public:
  // Construct success
  Result() : error_(""), has_error_(false) {}

  // Construct from error
  Result(const E& error) : error_(error), has_error_(true) {}
  Result(E&& error) : error_(std::move(error)), has_error_(true) {}

  bool IsOk() const { return !has_error_; }
  bool IsError() const { return has_error_; }

  const E& GetError() const {
    if (!has_error_) {
      throw std::runtime_error("Attempted to access error of ok result");
    }
    return error_;
  }

  explicit operator bool() const { return IsOk(); }
  bool operator!() const { return IsError(); }

 private:
  E error_;
  bool has_error_;
};

// Helper functions for creating results
template<typename T>
Result<T, Error> Ok(T&& value) {
  return Result<T, Error>(std::forward<T>(value));
}

inline Result<void, Error> Ok() {
  return Result<void, Error>();
}

template<typename T>
Result<T, Error> Err(const std::string& message) {
  return Result<T, Error>(Error(message));
}

template<typename T>
Result<T, Error> Err(int code, const std::string& message) {
  return Result<T, Error>(Error(code, message));
}

inline Result<void, Error> ErrVoid(const std::string& message) {
  return Result<void, Error>(Error(message));
}

inline Result<void, Error> ErrVoid(int code, const std::string& message) {
  return Result<void, Error>(Error(code, message));
}

}  // namespace utils
}  // namespace athena

#endif  // ATHENA_UTILS_ERROR_H_
