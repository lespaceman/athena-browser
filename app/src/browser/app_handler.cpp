#include "browser/app_handler.h"

#include "cef_command_line.h"
#include "cef_scheme.h"
#include "resources/scheme_handler.h"
#include "wrapper/cef_helpers.h"
// For message router renderer side
#include "wrapper/cef_message_router.h"

#include <cstdio>

AppHandler::AppHandler() {}

void AppHandler::OnBeforeCommandLineProcessing(const CefString& process_type,
                                               CefRefPtr<CefCommandLine> command_line) {
  // Empty process_type means browser process
  if (process_type.empty()) {
#if defined(OS_LINUX)
    // Platform-specific flags for Linux

    // Force X11 platform for proper child window embedding
    command_line->AppendSwitchWithValue("ozone-platform", "x11");

    // Use ANGLE with OpenGL ES/EGL for better OSR compatibility
    // Reference: https://github.com/chromiumembedded/cef/issues/3953
    // QCefView finding: Recent CEF versions on Linux need this for OSR
    command_line->AppendSwitchWithValue("use-angle", "gl-egl");

    // Use in-process GPU to avoid window handle issues
    command_line->AppendSwitch("in-process-gpu");

    // Disable GPU sandbox (often causes issues on Linux)
    command_line->AppendSwitch("disable-gpu-sandbox");

    // Use software rendering as fallback
    command_line->AppendSwitch("disable-gpu-compositing");

    // Logging for debugging
    command_line->AppendSwitch("enable-logging");
    command_line->AppendSwitchWithValue("v", "1");
#elif defined(OS_WIN)
    // Platform-specific flags for Windows
    // TODO: Add Windows-specific flags if needed
#elif defined(OS_MAC)
    // Platform-specific flags for macOS
    // TODO: Add macOS-specific flags if needed
#endif
  }
}

void AppHandler::OnContextInitialized() {
  CEF_REQUIRE_UI_THREAD();

  // Register the custom scheme handler factory for app://
  CefRegisterSchemeHandlerFactory("app", "", new AppSchemeHandlerFactory());
}

void AppHandler::OnRegisterCustomSchemes(CefRawPtr<CefSchemeRegistrar> registrar) {
  // Register app:// as a standard, secure scheme with CORS support
  registrar->AddCustomScheme(
      "app",
      CEF_SCHEME_OPTION_STANDARD | CEF_SCHEME_OPTION_SECURE | CEF_SCHEME_OPTION_CORS_ENABLED);
}

void AppHandler::OnContextCreated(CefRefPtr<CefBrowser> browser,
                                  CefRefPtr<CefFrame> frame,
                                  CefRefPtr<CefV8Context> context) {
  CEF_REQUIRE_RENDERER_THREAD();
  if (!renderer_router_) {
    CefMessageRouterConfig config;
    renderer_router_ = CefMessageRouterRendererSide::Create(config);
  }
  renderer_router_->OnContextCreated(browser, frame, context);

  // Inject a minimal window.Native API using cefQuery from the message router.
  const char* inject = R"JS((function(){
    try {
      var g = window.Native || {};
      g.getVersion = function(){
        return new Promise(function(resolve, reject){
          if (typeof window.cefQuery !== 'function') { return reject(new Error('cefQuery unavailable')); }
          window.cefQuery({ request: 'getVersion', onSuccess: resolve, onFailure: function(code,msg){ reject(new Error(msg||String(code))); } });
        });
      };
      window.Native = g;
    } catch(e) { /* noop */ }
  })())JS";
  frame->ExecuteJavaScript(inject, frame->GetURL(), 0);
}

void AppHandler::OnContextReleased(CefRefPtr<CefBrowser> browser,
                                   CefRefPtr<CefFrame> frame,
                                   CefRefPtr<CefV8Context> context) {
  CEF_REQUIRE_RENDERER_THREAD();
  if (renderer_router_)
    renderer_router_->OnContextReleased(browser, frame, context);
}

bool AppHandler::OnProcessMessageReceived(CefRefPtr<CefBrowser> browser,
                                          CefRefPtr<CefFrame> frame,
                                          CefProcessId source_process,
                                          CefRefPtr<CefProcessMessage> message) {
  CEF_REQUIRE_RENDERER_THREAD();
  if (renderer_router_ &&
      renderer_router_->OnProcessMessageReceived(browser, frame, source_process, message)) {
    return true;
  }

  if (!message || !frame) {
    return false;
  }

  const std::string name = message->GetName();
  if (name != "Athena.ExecuteJavaScript") {
    return false;
  }

  CefRefPtr<CefListValue> args = message->GetArgumentList();
  if (!args || args->GetSize() < 2) {
    return false;
  }

  const std::string request_id = args->GetString(0);
  const std::string code = args->GetString(1);

  auto escape_json = [](const std::string& input) -> std::string {
    std::string out;
    out.reserve(input.size());
    for (char c : input) {
      switch (c) {
        case '\\':
          out += "\\\\";
          break;
        case '"':
          out += "\\\"";
          break;
        case '\n':
          out += "\\n";
          break;
        case '\r':
          out += "\\r";
          break;
        case '\t':
          out += "\\t";
          break;
        default:
          if (static_cast<unsigned char>(c) < 0x20) {
            char buffer[7];
            std::snprintf(buffer, sizeof(buffer), "\\u%04x", c);
            out += buffer;
          } else {
            out += c;
          }
          break;
      }
    }
    return out;
  };

  std::string payload =
      R"({"success":false,"error":{"message":"Unable to enter V8 context","stack":""}})";
  CefRefPtr<CefV8Context> context = frame->GetV8Context();
  if (context && context->Enter()) {
    CefRefPtr<CefV8Value> retval;
    CefRefPtr<CefV8Exception> exception;

    const std::string script =
        "(function(){\n"
        "  const __athenaSerialize = (value) => {\n"
        "    try {\n"
        "      const seen = new WeakSet();\n"
        "      return JSON.parse(JSON.stringify(value, (key, val) => {\n"
        "        if (typeof val === 'bigint') { return val.toString(); }\n"
        "        if (typeof val === 'function' || typeof val === 'symbol') { return undefined; }\n"
        "        if (typeof val === 'object' && val !== null) {\n"
        "          if (seen.has(val)) { return '[Circular]'; }\n"
        "          seen.add(val);\n"
        "        }\n"
        "        return val;\n"
        "      }));\n"
        "    } catch (err) {\n"
        "      if (typeof value === 'undefined') { return null; }\n"
        "      return String(value);\n"
        "    }\n"
        "  };\n"
        "  try {\n"
        "    const __result = (function(){\n" +
        code +
        "\n"
        "    })();\n"
        "    const __type = (() => {\n"
        "      if (Array.isArray(__result)) return 'array';\n"
        "      if (__result === null) return 'null';\n"
        "      return typeof __result;\n"
        "    })();\n"
        "    return JSON.stringify({\n"
        "      success: true,\n"
        "      type: __type,\n"
        "      result: __athenaSerialize(__result),\n"
        "      stringResult: typeof __result === 'string' ? __result : null\n"
        "    });\n"
        "  } catch (error) {\n"
        "    return JSON.stringify({\n"
        "      success: false,\n"
        "      error: {\n"
        "        message: error && error.message ? String(error.message) : String(error),\n"
        "        stack: error && error.stack ? String(error.stack) : ''\n"
        "      }\n"
        "    });\n"
        "  }\n"
        "})();";

    bool ok = context->Eval(script, frame->GetURL(), 0, retval, exception);
    if (!ok || !retval.get() || !retval->IsString()) {
      std::string message_text = "JavaScript execution failed";
      std::string stack_text = "";
      if (exception) {
        message_text = exception->GetMessage().ToString();
        // Note: CEF V8Exception doesn't have GetStackTrace() method
        // Stack trace is typically included in the message itself
      }
      payload = std::string("{\"success\":false,\"error\":{\"message\":\"") +
                escape_json(message_text) + "\",\"stack\":\"" + escape_json(stack_text) + "\"}}";
    } else {
      payload = retval->GetStringValue();
    }

    context->Exit();
  }

  CefRefPtr<CefProcessMessage> response =
      CefProcessMessage::Create("Athena.ExecuteJavaScriptResult");
  CefRefPtr<CefListValue> response_args = response->GetArgumentList();
  response_args->SetString(0, request_id);
  response_args->SetString(1, payload);

  frame->SendProcessMessage(PID_BROWSER, response);
  return true;
}
