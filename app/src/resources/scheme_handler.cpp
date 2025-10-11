#include "resources/scheme_handler.h"
#include "cef_parser.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace fs = std::filesystem;

AppSchemeHandler::AppSchemeHandler() : offset_(0) {}

bool AppSchemeHandler::Open(CefRefPtr<CefRequest> request,
                             bool& handle_request,
                             CefRefPtr<CefCallback> callback) {
  handle_request = true;
  
  CefURLParts url_parts;
  CefString url = request->GetURL();
  if (!CefParseURL(url, url_parts)) {
    return false;
  }
  
  // Extract path from URL (app://index.html -> index.html)
  std::string path = CefString(&url_parts.path);
  if (path.empty() || path == "/") {
    path = "/index.html";
  }
  
  // Remove leading slash
  if (!path.empty() && path[0] == '/') {
    path = path.substr(1);
  }
  
  // Load the resource
  if (!LoadResource(path)) {
    // Return 404 page
    mime_type_ = "text/html";
    data_ = "<!DOCTYPE html><html><head><title>404</title></head>"
            "<body><h1>404 - Not Found</h1><p>Resource not found: " + path + "</p></body></html>";
  }
  
  return true;
}

void AppSchemeHandler::GetResponseHeaders(CefRefPtr<CefResponse> response,
                                           int64_t& response_length,
                                           CefString& redirectUrl) {
  response->SetMimeType(mime_type_);
  response->SetStatus(200);
  
  // Set strict CSP headers for production
  CefResponse::HeaderMap headers;
  // Allow a more permissive CSP during dev (for Vite HMR) if DEV_URL is set.
  const char* dev = std::getenv("DEV_URL");
  if (dev && *dev) {
    headers.emplace("Content-Security-Policy",
      "default-src 'self'; "
      "script-src 'self' 'unsafe-eval' http://localhost:5173; "
      "style-src 'self' 'unsafe-inline'; "
      "img-src 'self' data: blob:; "
      "font-src 'self' data:; "
      "connect-src 'self' ws://localhost:5173 http://localhost:5173; "
      "frame-ancestors 'none'");
  } else {
    headers.emplace("Content-Security-Policy",
      "default-src 'self'; "
      "script-src 'self'; "
      "style-src 'self' 'unsafe-inline'; "
      "img-src 'self' data: blob:; "
      "font-src 'self' data:; "
      "connect-src 'self' ws: wss:; "
      "frame-ancestors 'none'");
  }
  headers.emplace("X-Content-Type-Options", "nosniff");
  headers.emplace("Cache-Control", "public, max-age=3600");
  response->SetHeaderMap(headers);
  
  response_length = data_.length();
}

bool AppSchemeHandler::Read(void* data_out,
                             int bytes_to_read,
                             int& bytes_read,
                             CefRefPtr<CefResourceReadCallback> callback) {
  bool has_data = false;
  bytes_read = 0;
  
  if (offset_ < data_.length()) {
    int transfer_size = std::min(bytes_to_read, static_cast<int>(data_.length() - offset_));
    memcpy(data_out, data_.c_str() + offset_, transfer_size);
    offset_ += transfer_size;
    bytes_read = transfer_size;
    has_data = true;
  }
  
  return has_data;
}

void AppSchemeHandler::Cancel() {
  // Clean up if needed
}

std::string AppSchemeHandler::GetMimeType(const std::string& path) {
  auto ext_pos = path.rfind('.');
  if (ext_pos != std::string::npos) {
    std::string ext = path.substr(ext_pos + 1);
    if (ext == "html" || ext == "htm") return "text/html";
    if (ext == "js" || ext == "mjs") return "application/javascript";
    if (ext == "css") return "text/css";
    if (ext == "json") return "application/json";
    if (ext == "map") return "application/json";
    if (ext == "png") return "image/png";
    if (ext == "jpg" || ext == "jpeg") return "image/jpeg";
    if (ext == "svg") return "image/svg+xml";
    if (ext == "ico") return "image/x-icon";
    if (ext == "woff") return "font/woff";
    if (ext == "woff2") return "font/woff2";
    if (ext == "ttf") return "font/ttf";
    if (ext == "wasm") return "application/wasm";
    if (ext == "txt") return "text/plain";
  }
  return "application/octet-stream";
}

bool AppSchemeHandler::LoadResource(const std::string& path) {
  // Look for resources in resources/web directory (production build output)
  fs::path resource_path = fs::path("resources/web") / path;
  
  // Also check executable directory
  if (!fs::exists(resource_path)) {
    // Get executable directory and check there
    char exe_path[1024];
#ifdef _WIN32
    GetModuleFileNameA(NULL, exe_path, sizeof(exe_path));
    fs::path exe_dir = fs::path(exe_path).parent_path();
#else
    ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
    if (len != -1) {
      exe_path[len] = '\0';
      fs::path exe_dir = fs::path(exe_path).parent_path();
      resource_path = exe_dir / "resources/web" / path;
    }
#endif
  }
  
  if (!fs::exists(resource_path)) {
    return false;
  }
  
  // Read file content
  std::ifstream file(resource_path, std::ios::binary);
  if (!file) {
    return false;
  }
  
  std::stringstream buffer;
  buffer << file.rdbuf();
  data_ = buffer.str();
  mime_type_ = GetMimeType(path);
  offset_ = 0;
  
  return true;
}

// Factory implementation
CefRefPtr<CefResourceHandler> AppSchemeHandlerFactory::Create(
    CefRefPtr<CefBrowser> browser,
    CefRefPtr<CefFrame> frame,
    const CefString& scheme_name,
    CefRefPtr<CefRequest> request) {
  return new AppSchemeHandler();
}
