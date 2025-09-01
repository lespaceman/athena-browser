#include "app_handler.h"
#include "scheme_handler.h"
#include "cef_scheme.h"
#include "wrapper/cef_helpers.h"

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
