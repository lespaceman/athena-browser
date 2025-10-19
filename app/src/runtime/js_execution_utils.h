#ifndef ATHENA_RUNTIME_JS_EXECUTION_UTILS_H_
#define ATHENA_RUNTIME_JS_EXECUTION_UTILS_H_

#include <nlohmann/json.hpp>
#include <optional>
#include <string>

namespace athena {
namespace runtime {

struct JsExecutionResult {
  bool success{false};
  std::string type{"unknown"};
  nlohmann::json value{nullptr};
  std::string string_value;
  std::string error_message;
  std::string error_stack;
};

std::optional<JsExecutionResult> ParseJsExecutionResultString(const std::string& raw,
                                                              std::string& error_out);

bool JsonStringLooksLikeObject(const nlohmann::json& value);

}  // namespace runtime
}  // namespace athena

#endif  // ATHENA_RUNTIME_JS_EXECUTION_UTILS_H_
