#include "runtime/js_execution_utils.h"

namespace athena {
namespace runtime {

std::optional<JsExecutionResult> ParseJsExecutionResultString(const std::string& raw,
                                                              std::string& error_out) {
  if (raw.empty()) {
    error_out = "Renderer returned empty JavaScript result";
    return std::nullopt;
  }

  nlohmann::json parsed = nlohmann::json::parse(raw, nullptr, false);
  if (parsed.is_discarded() || !parsed.is_object()) {
    error_out = "Failed to parse JavaScript response";
    return std::nullopt;
  }

  JsExecutionResult result;
  result.success = parsed.value("success", false);
  result.type = parsed.value("type", std::string("unknown"));

  if (parsed.contains("result")) {
    result.value = parsed["result"];
  }

  if (parsed.contains("stringResult") && parsed["stringResult"].is_string()) {
    result.string_value = parsed["stringResult"].get<std::string>();
  }

  if (parsed.contains("error") && parsed["error"].is_object()) {
    const auto& error = parsed["error"];
    if (error.contains("message") && error["message"].is_string()) {
      result.error_message = error["message"].get<std::string>();
    }
    if (error.contains("stack") && error["stack"].is_string()) {
      result.error_stack = error["stack"].get<std::string>();
    }
  }

  return result;
}

bool JsonStringLooksLikeObject(const nlohmann::json& value) {
  if (!value.is_string()) {
    return false;
  }
  const std::string& str = value.get_ref<const std::string&>();
  if (str.empty()) {
    return false;
  }
  const char first = str.front();
  return first == '{' || first == '[';
}

}  // namespace runtime
}  // namespace athena
