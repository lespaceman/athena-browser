/**
 * Example: Using Claude Agent SDK from C++ via Node Runtime
 *
 * This demonstrates how to call Claude SDK endpoints from the Qt application.
 */

#include "runtime/node_runtime.h"
#include <iostream>
#include <string>

using namespace athena;

// Helper to format JSON (simplified - use proper JSON library in production)
std::string makeJson(const std::string& key, const std::string& value) {
  return "{\"" + key + "\": \"" + value + "\"}";
}

std::string makeJson(const std::string& key1, const std::string& value1,
                     const std::string& key2, const std::string& value2) {
  return "{\"" + key1 + "\": \"" + value1 + "\", \"" +
         key2 + "\": \"" + value2 + "\"}";
}

int main() {
  // Initialize Node runtime
  runtime::NodeRuntimeConfig config;
  config.runtime_script_path = "/path/to/athena-browser/node-runtime/server.js";

  auto node_runtime = std::make_unique<runtime::NodeRuntime>(config);

  auto init_result = node_runtime->Initialize();
  if (!init_result) {
    std::cerr << "Failed to initialize Node runtime: "
              << init_result.GetError().Message() << std::endl;
    return 1;
  }

  std::cout << "Node runtime initialized at: "
            << node_runtime->GetSocketPath() << std::endl;

  // Example 1: Analyze code
  std::cout << "\n=== Example 1: Analyze Code ===" << std::endl;

  std::string analyze_request = makeJson(
      "filePath", "/tmp/test_code.cpp",
      "question", "What does this code do?"
  );

  auto analyze_result = node_runtime->Call(
      "POST", "/v1/claude/analyze-code", analyze_request
  );

  if (analyze_result) {
    std::cout << "Analysis: " << analyze_result.Value() << std::endl;
  } else {
    std::cerr << "Error: " << analyze_result.GetError().Message() << std::endl;
  }

  // Example 2: Generate code
  std::cout << "\n=== Example 2: Generate Code ===" << std::endl;

  std::string generate_request = makeJson(
      "spec", "Create a simple C++ class for a counter with increment/decrement methods",
      "outputPath", "/tmp/counter.cpp"
  );

  auto generate_result = node_runtime->Call(
      "POST", "/v1/claude/generate-code", generate_request
  );

  if (generate_result) {
    std::cout << "Generated: " << generate_result.Value() << std::endl;
  } else {
    std::cerr << "Error: " << generate_result.GetError().Message() << std::endl;
  }

  // Example 3: Search code
  std::cout << "\n=== Example 3: Search Code ===" << std::endl;

  std::string search_request = makeJson(
      "pattern", "NodeRuntime",
      "globPattern", "**/*.cpp"
  );

  auto search_result = node_runtime->Call(
      "POST", "/v1/claude/search-code", search_request
  );

  if (search_result) {
    std::cout << "Search results: " << search_result.Value() << std::endl;
  } else {
    std::cerr << "Error: " << search_result.GetError().Message() << std::endl;
  }

  // Example 4: Run command
  std::cout << "\n=== Example 4: Run Command ===" << std::endl;

  std::string command_request = makeJson("command", "uname -a");

  auto command_result = node_runtime->Call(
      "POST", "/v1/claude/run-command", command_request
  );

  if (command_result) {
    std::cout << "Command output: " << command_result.Value() << std::endl;
  } else {
    std::cerr << "Error: " << command_result.GetError().Message() << std::endl;
  }

  // Example 5: Refactor code
  std::cout << "\n=== Example 5: Refactor Code ===" << std::endl;

  std::string refactor_request = makeJson(
      "filePath", "/tmp/test_code.cpp",
      "instructions", "Add error handling and improve formatting"
  );

  auto refactor_result = node_runtime->Call(
      "POST", "/v1/claude/refactor-code", refactor_request
  );

  if (refactor_result) {
    std::cout << "Refactored: " << refactor_result.Value() << std::endl;
  } else {
    std::cerr << "Error: " << refactor_result.GetError().Message() << std::endl;
  }

  // Example 6: General query with custom options
  std::cout << "\n=== Example 6: General Query ===" << std::endl;

  std::string query_request = R"({
    "prompt": "What are the best practices for error handling in C++?",
    "options": {
      "allowedTools": ["WebSearch", "WebFetch"],
      "model": "claude-sonnet-4-5"
    }
  })";

  auto query_result = node_runtime->Call(
      "POST", "/v1/claude/query", query_request
  );

  if (query_result) {
    std::cout << "Response: " << query_result.Value() << std::endl;
  } else {
    std::cerr << "Error: " << query_result.GetError().Message() << std::endl;
  }

  // Shutdown
  node_runtime->Shutdown();
  std::cout << "\nNode runtime shut down successfully" << std::endl;

  return 0;
}
