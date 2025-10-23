#include "utils/logging.h"

#include <fstream>
#include <gtest/gtest.h>
#include <sstream>
#include <thread>
#include <vector>

using namespace athena::utils;

TEST(LoggerTest, DefaultConstructor) {
  Logger logger("test");
  EXPECT_EQ(logger.GetName(), "test");
  EXPECT_EQ(logger.GetLevel(), LogLevel::kInfo);
}

TEST(LoggerTest, SetLevel) {
  Logger logger("test");
  logger.SetLevel(LogLevel::kDebug);
  EXPECT_EQ(logger.GetLevel(), LogLevel::kDebug);

  logger.SetLevel(LogLevel::kError);
  EXPECT_EQ(logger.GetLevel(), LogLevel::kError);
}

TEST(LoggerTest, SimpleLogging) {
  Logger logger("test");
  logger.EnableConsoleOutput(false);

  // These should not crash
  logger.Debug("Debug message");
  logger.Info("Info message");
  logger.Warn("Warning message");
  logger.Error("Error message");
}

TEST(LoggerTest, FormattedLogging) {
  Logger logger("test");
  logger.EnableConsoleOutput(false);

  // These should not crash
  logger.Debug("Value: {}", 42);
  logger.Info("String: {}, Int: {}", "test", 123);
  logger.Warn("Multiple: {}, {}, {}", 1, 2, 3);
}

TEST(LoggerTest, LevelFiltering) {
  Logger logger("test");
  logger.SetLevel(LogLevel::kWarn);
  logger.EnableConsoleOutput(false);

  // These should be filtered out (below WARN level)
  logger.Debug("Should not appear");
  logger.Info("Should not appear");

  // These should pass through
  logger.Warn("Should appear");
  logger.Error("Should appear");
}

TEST(LoggerTest, FileOutput) {
  const std::string test_file = "/tmp/athena_test_log.txt";

  // Remove old test file
  std::remove(test_file.c_str());

  {
    Logger logger("test");
    logger.SetLevel(LogLevel::kInfo);
    logger.EnableConsoleOutput(false);
    logger.EnableFileOutput(true);
    logger.SetOutputFile(test_file);

    logger.Info("Test message");
    logger.Error("Error message");
  }

  // Check file was created and contains messages
  std::ifstream file(test_file);
  ASSERT_TRUE(file.is_open());

  std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

  EXPECT_TRUE(content.find("Test message") != std::string::npos);
  EXPECT_TRUE(content.find("Error message") != std::string::npos);
  EXPECT_TRUE(content.find("[test]") != std::string::npos);
  EXPECT_TRUE(content.find("[INFO]") != std::string::npos);
  EXPECT_TRUE(content.find("[ERROR]") != std::string::npos);

  // Cleanup
  std::remove(test_file.c_str());
}

TEST(LoggerTest, LogLevelEnvironmentVariable) {
  // Test default level (no env var)
  {
    unsetenv("LOG_LEVEL");
    Logger logger("test");
    EXPECT_EQ(logger.GetLevel(), LogLevel::kInfo);
  }

  // Test debug level
  {
    setenv("LOG_LEVEL", "debug", 1);
    Logger logger("test");
    EXPECT_EQ(logger.GetLevel(), LogLevel::kDebug);
  }

  // Test warn level
  {
    setenv("LOG_LEVEL", "warn", 1);
    Logger logger("test");
    EXPECT_EQ(logger.GetLevel(), LogLevel::kWarn);
  }

  // Test error level
  {
    setenv("LOG_LEVEL", "error", 1);
    Logger logger("test");
    EXPECT_EQ(logger.GetLevel(), LogLevel::kError);
  }

  // Test case insensitivity
  {
    setenv("LOG_LEVEL", "DEBUG", 1);
    Logger logger("test");
    EXPECT_EQ(logger.GetLevel(), LogLevel::kDebug);
  }

  // Test invalid value (should default to info)
  {
    setenv("LOG_LEVEL", "invalid", 1);
    Logger logger("test");
    EXPECT_EQ(logger.GetLevel(), LogLevel::kInfo);
  }

  // Cleanup
  unsetenv("LOG_LEVEL");
}

TEST(LoggerTest, ThreadSafety) {
  Logger logger("test");
  logger.EnableConsoleOutput(false);

  // Simple thread safety test - should not crash
  std::vector<std::thread> threads;
  for (int i = 0; i < 10; ++i) {
    threads.emplace_back([&logger, i]() {
      for (int j = 0; j < 100; ++j) {
        logger.Info("Thread {} message {}", i, j);
      }
    });
  }

  for (auto& thread : threads) {
    thread.join();
  }
}
