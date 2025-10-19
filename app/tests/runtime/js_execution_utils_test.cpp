#include "runtime/js_execution_utils.h"
#include <gtest/gtest.h>

namespace athena {
namespace runtime {

TEST(JsExecutionUtilsTest, ParsesSuccessfulObjectResult) {
  std::string error;
  auto result = ParseJsExecutionResultString(
      R"({"success":true,"type":"object","result":{"foo":42},"stringResult":null})",
      error);

  ASSERT_TRUE(result.has_value());
  EXPECT_TRUE(result->success);
  EXPECT_EQ(result->type, "object");
  ASSERT_TRUE(result->value.is_object());
  EXPECT_EQ(result->value["foo"], 42);
  EXPECT_TRUE(result->string_value.empty());
  EXPECT_TRUE(result->error_message.empty());
}

TEST(JsExecutionUtilsTest, ParsesStringResultAndPreservesPreview) {
  std::string error;
  auto result = ParseJsExecutionResultString(
      R"({"success":true,"type":"string","result":"{\"hello\":\"world\"}","stringResult":"{\"hello\":\"world\"}"})",
      error);

  ASSERT_TRUE(result.has_value());
  EXPECT_TRUE(result->success);
  EXPECT_EQ(result->type, "string");
  ASSERT_TRUE(result->value.is_string());
  EXPECT_EQ(result->value.get<std::string>(), "{\"hello\":\"world\"}");
  EXPECT_EQ(result->string_value, "{\"hello\":\"world\"}");
}

TEST(JsExecutionUtilsTest, ReportsParseErrorForEmptyPayload) {
  std::string error;
  auto result = ParseJsExecutionResultString("", error);

  EXPECT_FALSE(result.has_value());
  EXPECT_FALSE(error.empty());
}

TEST(JsExecutionUtilsTest, DetectsLikelyJsonStrings) {
  EXPECT_TRUE(JsonStringLooksLikeObject(nlohmann::json::parse("\"{}\"")));
  EXPECT_TRUE(JsonStringLooksLikeObject(nlohmann::json::parse("\"[]\"")));
  EXPECT_FALSE(JsonStringLooksLikeObject(nlohmann::json::parse("\"hello\"")));
  EXPECT_FALSE(JsonStringLooksLikeObject(nlohmann::json::parse("123")));
}

}  // namespace runtime
}  // namespace athena
