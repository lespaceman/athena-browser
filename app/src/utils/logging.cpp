#include "utils/logging.h"

#include <cstdlib>
#include <ctime>

namespace athena {
namespace utils {

// Helper function to parse LOG_LEVEL environment variable
static LogLevel ParseLogLevel(const char* level_str) {
  if (!level_str) {
    return LogLevel::kInfo;  // Default
  }

  std::string level(level_str);
  // Convert to lowercase for case-insensitive comparison
  for (char& c : level) {
    c = std::tolower(c);
  }

  if (level == "debug") return LogLevel::kDebug;
  if (level == "info") return LogLevel::kInfo;
  if (level == "warn") return LogLevel::kWarn;
  if (level == "error") return LogLevel::kError;
  if (level == "fatal") return LogLevel::kFatal;

  // Default to info if unrecognized
  return LogLevel::kInfo;
}

Logger::Logger(const std::string& name)
    : name_(name),
      level_(ParseLogLevel(std::getenv("LOG_LEVEL"))),
      console_output_(true),
      file_output_(false) {}

Logger::~Logger() {
  if (file_stream_ && file_stream_->is_open()) {
    file_stream_->close();
  }
}

void Logger::SetLevel(LogLevel level) {
  std::lock_guard<std::mutex> lock(mutex_);
  level_ = level;
}

void Logger::SetOutputFile(const std::string& filepath) {
  std::lock_guard<std::mutex> lock(mutex_);
  output_file_ = filepath;

  if (!filepath.empty()) {
    file_stream_ = std::make_unique<std::ofstream>(filepath, std::ios::app);
    if (!file_stream_->is_open()) {
      std::cerr << "Failed to open log file: " << filepath << std::endl;
      file_stream_.reset();
    }
  }
}

void Logger::EnableConsoleOutput(bool enable) {
  std::lock_guard<std::mutex> lock(mutex_);
  console_output_ = enable;
}

void Logger::EnableFileOutput(bool enable) {
  std::lock_guard<std::mutex> lock(mutex_);
  file_output_ = enable;
}

void Logger::Debug(const std::string& message) {
  Log(LogLevel::kDebug, message);
}

void Logger::Info(const std::string& message) {
  Log(LogLevel::kInfo, message);
}

void Logger::Warn(const std::string& message) {
  Log(LogLevel::kWarn, message);
}

void Logger::Error(const std::string& message) {
  Log(LogLevel::kError, message);
}

void Logger::Fatal(const std::string& message) {
  Log(LogLevel::kFatal, message);
}

void Logger::Log(LogLevel level, const std::string& message) {
  if (level < level_) {
    return;
  }

  std::string log_line = FormatLogLine(level, message);

  std::lock_guard<std::mutex> lock(mutex_);

  if (console_output_) {
    if (level >= LogLevel::kError) {
      std::cerr << log_line << std::endl;
    } else {
      std::cout << log_line << std::endl;
    }
  }

  if (file_output_ && file_stream_ && file_stream_->is_open()) {
    *file_stream_ << log_line << std::endl;
    file_stream_->flush();
  }
}

std::string Logger::FormatLogLine(LogLevel level, const std::string& message) {
  std::ostringstream oss;
  oss << "[" << CurrentTimestamp() << "] "
      << "[" << name_ << "] "
      << "[" << LevelToString(level) << "] " << message;
  return oss.str();
}

std::string Logger::LevelToString(LogLevel level) {
  switch (level) {
    case LogLevel::kDebug:
      return "DEBUG";
    case LogLevel::kInfo:
      return "INFO";
    case LogLevel::kWarn:
      return "WARN";
    case LogLevel::kError:
      return "ERROR";
    case LogLevel::kFatal:
      return "FATAL";
    default:
      return "UNKNOWN";
  }
}

std::string Logger::CurrentTimestamp() {
  auto now = std::chrono::system_clock::now();
  auto now_time_t = std::chrono::system_clock::to_time_t(now);
  auto now_ms =
      std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

  std::tm tm_buf;
#if defined(_WIN32)
  localtime_s(&tm_buf, &now_time_t);
#else
  localtime_r(&now_time_t, &tm_buf);
#endif

  std::ostringstream oss;
  oss << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S") << '.' << std::setfill('0') << std::setw(3)
      << now_ms.count();
  return oss.str();
}

}  // namespace utils
}  // namespace athena
