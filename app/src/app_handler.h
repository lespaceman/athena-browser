#ifndef ATHENA_APP_HANDLER_H_
#define ATHENA_APP_HANDLER_H_

#include "cef_app.h"
#include "cef_browser_process_handler.h"
#include "cef_render_process_handler.h"

class AppHandler : public CefApp,
                   public CefBrowserProcessHandler {
 public:
  AppHandler();

  // CefApp methods
  CefRefPtr<CefBrowserProcessHandler> GetBrowserProcessHandler() override {
    return this;
  }

  // CefBrowserProcessHandler methods
  void OnContextInitialized() override;
  void OnRegisterCustomSchemes(CefRawPtr<CefSchemeRegistrar> registrar) override;

 private:
  IMPLEMENT_REFCOUNTING(AppHandler);
};

#endif  // ATHENA_APP_HANDLER_H_