#ifndef ATHENA_SCHEME_HANDLER_H_
#define ATHENA_SCHEME_HANDLER_H_

#include "cef_scheme.h"
#include <string>

class AppSchemeHandler : public CefResourceHandler {
 public:
  AppSchemeHandler();

  // CefResourceHandler methods
  bool Open(CefRefPtr<CefRequest> request,
            bool& handle_request,
            CefRefPtr<CefCallback> callback) override;
  
  void GetResponseHeaders(CefRefPtr<CefResponse> response,
                          int64_t& response_length,
                          CefString& redirectUrl) override;
  
  bool Read(void* data_out,
            int bytes_to_read,
            int& bytes_read,
            CefRefPtr<CefResourceReadCallback> callback) override;
  
  void Cancel() override;

 private:
  std::string mime_type_;
  std::string data_;
  size_t offset_;
  
  std::string GetMimeType(const std::string& path);
  bool LoadResource(const std::string& path);
  
  IMPLEMENT_REFCOUNTING(AppSchemeHandler);
};

class AppSchemeHandlerFactory : public CefSchemeHandlerFactory {
 public:
  CefRefPtr<CefResourceHandler> Create(CefRefPtr<CefBrowser> browser,
                                        CefRefPtr<CefFrame> frame,
                                        const CefString& scheme_name,
                                        CefRefPtr<CefRequest> request) override;
  
 private:
  IMPLEMENT_REFCOUNTING(AppSchemeHandlerFactory);
};

#endif  // ATHENA_SCHEME_HANDLER_H_