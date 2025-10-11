#include <gtest/gtest.h>
#include "utils/error.h"

using namespace athena::utils;

// Error class tests
TEST(ErrorTest, MessageConstructor) {
  Error err("Test error");
  EXPECT_EQ(err.Message(), "Test error");
  EXPECT_EQ(err.Code(), 0);
}

TEST(ErrorTest, CodeAndMessageConstructor) {
  Error err(42, "Test error with code");
  EXPECT_EQ(err.Message(), "Test error with code");
  EXPECT_EQ(err.Code(), 42);
}

TEST(ErrorTest, ToString) {
  Error err1("Simple error");
  EXPECT_EQ(err1.ToString(), "Error: Simple error");

  Error err2(404, "Not found");
  EXPECT_EQ(err2.ToString(), "Error(404): Not found");
}

// Result<T> tests with value type
TEST(ResultTest, ConstructFromValue) {
  Result<int> result(42);
  EXPECT_TRUE(result.IsOk());
  EXPECT_FALSE(result.IsError());
  EXPECT_EQ(result.Value(), 42);
}

TEST(ResultTest, ConstructFromError) {
  Result<int> result(Error("Failed"));
  EXPECT_FALSE(result.IsOk());
  EXPECT_TRUE(result.IsError());
  EXPECT_EQ(result.GetError().Message(), "Failed");
}

TEST(ResultTest, ValueThrowsOnError) {
  Result<int> result(Error("Failed"));
  EXPECT_THROW(result.Value(), std::runtime_error);
}

TEST(ResultTest, ErrorThrowsOnValue) {
  Result<int> result(42);
  EXPECT_THROW(result.GetError(), std::runtime_error);
}

TEST(ResultTest, ValueOr) {
  Result<int> ok_result(42);
  Result<int> err_result(Error("Failed"));

  EXPECT_EQ(ok_result.ValueOr(0), 42);
  EXPECT_EQ(err_result.ValueOr(0), 0);
}

TEST(ResultTest, BoolConversion) {
  Result<int> ok_result(42);
  Result<int> err_result(Error("Failed"));

  EXPECT_TRUE(ok_result);
  EXPECT_FALSE(!ok_result);

  EXPECT_FALSE(err_result);
  EXPECT_TRUE(!err_result);
}

TEST(ResultTest, MoveSemantics) {
  Result<std::string> result(std::string("test"));
  std::string value = std::move(result).Value();
  EXPECT_EQ(value, "test");
}

// Result<void> tests
TEST(ResultVoidTest, ConstructSuccess) {
  Result<void> result;
  EXPECT_TRUE(result.IsOk());
  EXPECT_FALSE(result.IsError());
}

TEST(ResultVoidTest, ConstructFromError) {
  Result<void> result(Error("Failed"));
  EXPECT_FALSE(result.IsOk());
  EXPECT_TRUE(result.IsError());
  EXPECT_EQ(result.GetError().Message(), "Failed");
}

TEST(ResultVoidTest, ErrorThrowsOnSuccess) {
  Result<void> result;
  EXPECT_THROW(result.GetError(), std::runtime_error);
}

TEST(ResultVoidTest, BoolConversion) {
  Result<void> ok_result;
  Result<void> err_result(Error("Failed"));

  EXPECT_TRUE(ok_result);
  EXPECT_FALSE(!ok_result);

  EXPECT_FALSE(err_result);
  EXPECT_TRUE(!err_result);
}

// Helper function tests
TEST(ResultHelpersTest, Ok) {
  auto result = Ok(42);
  EXPECT_TRUE(result.IsOk());
  EXPECT_EQ(result.Value(), 42);
}

TEST(ResultHelpersTest, OkVoid) {
  auto result = Ok();
  EXPECT_TRUE(result.IsOk());
}

TEST(ResultHelpersTest, ErrWithMessage) {
  auto result = Err<int>("Failed");
  EXPECT_TRUE(result.IsError());
  EXPECT_EQ(result.GetError().Message(), "Failed");
}

TEST(ResultHelpersTest, ErrWithCode) {
  auto result = Err<int>(404, "Not found");
  EXPECT_TRUE(result.IsError());
  EXPECT_EQ(result.GetError().Code(), 404);
  EXPECT_EQ(result.GetError().Message(), "Not found");
}

TEST(ResultHelpersTest, ErrVoid) {
  auto result = ErrVoid("Failed");
  EXPECT_TRUE(result.IsError());
  EXPECT_EQ(result.GetError().Message(), "Failed");
}

TEST(ResultHelpersTest, ErrVoidWithCode) {
  auto result = ErrVoid(500, "Internal error");
  EXPECT_TRUE(result.IsError());
  EXPECT_EQ(result.GetError().Code(), 500);
  EXPECT_EQ(result.GetError().Message(), "Internal error");
}

// Practical usage tests
Result<int> Divide(int a, int b) {
  if (b == 0) {
    return Err<int>("Division by zero");
  }
  return Ok(a / b);
}

TEST(ResultUsageTest, SuccessPath) {
  auto result = Divide(10, 2);
  EXPECT_TRUE(result.IsOk());
  EXPECT_EQ(result.Value(), 5);
}

TEST(ResultUsageTest, ErrorPath) {
  auto result = Divide(10, 0);
  EXPECT_TRUE(result.IsError());
  EXPECT_EQ(result.GetError().Message(), "Division by zero");
}

Result<void> ValidatePositive(int value) {
  if (value <= 0) {
    return ErrVoid("Value must be positive");
  }
  return Ok();
}

TEST(ResultUsageTest, VoidSuccess) {
  auto result = ValidatePositive(10);
  EXPECT_TRUE(result.IsOk());
}

TEST(ResultUsageTest, VoidError) {
  auto result = ValidatePositive(-5);
  EXPECT_TRUE(result.IsError());
  EXPECT_EQ(result.GetError().Message(), "Value must be positive");
}
