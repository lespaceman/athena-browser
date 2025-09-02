#include "app_handler.h"
#include "scheme_handler.h"
#include "cef_scheme.h"
#include "wrapper/cef_helpers.h"
// For message router renderer side
#include "wrapper/cef_message_router.h"

AppHandler::AppHandler() {}

void AppHandler::OnContextInitialized() {
  CEF_REQUIRE_UI_THREAD();
  
  // Register the custom scheme handler factory for app://
  CefRegisterSchemeHandlerFactory("app", "", new AppSchemeHandlerFactory());
}

void AppHandler::OnRegisterCustomSchemes(CefRawPtr<CefSchemeRegistrar> registrar) {
  // Register app:// as a standard, secure scheme with CORS support
  registrar->AddCustomScheme("app", 
    CEF_SCHEME_OPTION_STANDARD | 
    CEF_SCHEME_OPTION_SECURE | 
    CEF_SCHEME_OPTION_CORS_ENABLED);
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
  if (renderer_router_ && renderer_router_->OnProcessMessageReceived(browser, frame, source_process, message))
    return true;
  return false;
}
