#ifndef ATHENA_APP_HANDLER_H_
#define ATHENA_APP_HANDLER_H_

#include "cef_app.h"
#include "cef_browser_process_handler.h"
#include "cef_render_process_handler.h"
#include "wrapper/cef_message_router.h"

class AppHandler : public CefApp,
                   public CefBrowserProcessHandler,
                   public CefRenderProcessHandler {
 public:
  AppHandler();

  // CefApp methods
  CefRefPtr<CefBrowserProcessHandler> GetBrowserProcessHandler() override {
    return this;
  }
  CefRefPtr<CefRenderProcessHandler> GetRenderProcessHandler() override {
    return this;
  }

  // CefApp methods  
  void OnBeforeCommandLineProcessing(const CefString& process_type,
                                     CefRefPtr<CefCommandLine> command_line) override;
  
  // CefBrowserProcessHandler methods
  void OnContextInitialized() override;
  void OnRegisterCustomSchemes(CefRawPtr<CefSchemeRegistrar> registrar) override;

  // CefRenderProcessHandler methods
  void OnContextCreated(CefRefPtr<CefBrowser> browser,
                        CefRefPtr<CefFrame> frame,
                        CefRefPtr<CefV8Context> context) override;
  void OnContextReleased(CefRefPtr<CefBrowser> browser,
                         CefRefPtr<CefFrame> frame,
                         CefRefPtr<CefV8Context> context) override;
  bool OnProcessMessageReceived(CefRefPtr<CefBrowser> browser,
                                CefRefPtr<CefFrame> frame,
                                CefProcessId source_process,
                                CefRefPtr<CefProcessMessage> message) override;

 private:
  CefRefPtr<CefMessageRouterRendererSide> renderer_router_;
  IMPLEMENT_REFCOUNTING(AppHandler);
};

#endif  // ATHENA_APP_HANDLER_H_
