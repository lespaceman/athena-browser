#ifndef ATHENA_UTILS_LOGGING_H_
#define ATHENA_UTILS_LOGGING_H_

#include <string>
#include <memory>
#include <sstream>
#include <iostream>
#include <fstream>
#include <chrono>
#include <iomanip>
#include <mutex>

namespace athena {
namespace utils {

enum class LogLevel {
  DEBUG = 0,
  INFO = 1,
  WARN = 2,
  ERROR = 3,
  FATAL = 4
};

class Logger {
 public:
  explicit Logger(const std::string& name);
  ~Logger();

  // Configuration
  void SetLevel(LogLevel level);
  void SetOutputFile(const std::string& filepath);
  void EnableConsoleOutput(bool enable);
  void EnableFileOutput(bool enable);

  // Logging methods
  void Debug(const std::string& message);
  void Info(const std::string& message);
  void Warn(const std::string& message);
  void Error(const std::string& message);
  void Fatal(const std::string& message);

  // Template methods for formatting
  template<typename... Args>
  void Debug(const std::string& format, Args&&... args) {
    Log(LogLevel::DEBUG, Format(format, std::forward<Args>(args)...));
  }

  template<typename... Args>
  void Info(const std::string& format, Args&&... args) {
    Log(LogLevel::INFO, Format(format, std::forward<Args>(args)...));
  }

  template<typename... Args>
  void Warn(const std::string& format, Args&&... args) {
    Log(LogLevel::WARN, Format(format, std::forward<Args>(args)...));
  }

  template<typename... Args>
  void Error(const std::string& format, Args&&... args) {
    Log(LogLevel::ERROR, Format(format, std::forward<Args>(args)...));
  }

  template<typename... Args>
  void Fatal(const std::string& format, Args&&... args) {
    Log(LogLevel::FATAL, Format(format, std::forward<Args>(args)...));
  }

  LogLevel GetLevel() const { return level_; }
  const std::string& GetName() const { return name_; }

 private:
  void Log(LogLevel level, const std::string& message);
  std::string FormatLogLine(LogLevel level, const std::string& message);
  std::string LevelToString(LogLevel level);
  std::string CurrentTimestamp();

  // Simple format implementation
  template<typename T>
  std::string Format(const std::string& format, T&& value) {
    size_t pos = format.find("{}");
    if (pos == std::string::npos) {
      return format;
    }
    std::ostringstream oss;
    oss << format.substr(0, pos) << value << format.substr(pos + 2);
    return oss.str();
  }

  template<typename T, typename... Args>
  std::string Format(const std::string& format, T&& value, Args&&... args) {
    size_t pos = format.find("{}");
    if (pos == std::string::npos) {
      return format;
    }
    std::ostringstream oss;
    oss << format.substr(0, pos) << value;
    return oss.str() + Format(format.substr(pos + 2), std::forward<Args>(args)...);
  }

  std::string name_;
  LogLevel level_;
  bool console_output_;
  bool file_output_;
  std::string output_file_;
  std::unique_ptr<std::ofstream> file_stream_;
  mutable std::mutex mutex_;
};

// Global logger instance
Logger* GetGlobalLogger();
void SetGlobalLogger(Logger* logger);

}  // namespace utils
}  // namespace athena

#endif  // ATHENA_UTILS_LOGGING_H_
