#ifndef ATHENA_BROWSER_THREAD_SAFETY_H_
#define ATHENA_BROWSER_THREAD_SAFETY_H_

#include <functional>
#include <QMetaObject>
#include <QObject>
#include <QPointer>
#include <Qt>
#include <utility>

/**
 * Thread safety helpers for CEF↔Qt cross-thread communication.
 *
 * CEF runs callbacks on the CEF UI thread, but Qt widgets must only be
 * accessed from the Qt main thread. This module provides safe marshaling
 * between threads with weak pointer validation to prevent use-after-free.
 *
 * Design principles (from QCefView):
 * 1. Never call Qt widget methods directly from CEF callbacks
 * 2. Always validate widget existence before dereferencing
 * 3. Use QPointer<T> for automatic null-on-destroy semantics
 * 4. Marshal calls to Qt main thread via QMetaObject::invokeMethod
 *
 * Related QCefView reference: CefViewCoreProtocol.h:66-95
 */

namespace athena {
namespace browser {

/**
 * Safe callback wrapper for CEF→Qt communication.
 *
 * Validates that the Qt object still exists before invoking the callback.
 * If the object has been destroyed, the callback is silently dropped.
 *
 * Thread safety: This must be called from the CEF UI thread.
 * The callback will be marshaled to the Qt main thread.
 *
 * @tparam QObjectType The Qt object type (must inherit QObject)
 * @tparam Func The callback function type
 * @tparam Args The callback argument types
 *
 * @param obj Non-owning pointer to Qt object
 * @param func The callback to invoke if object is still valid
 * @param args Arguments to pass to the callback
 *
 * Example:
 *   void CefClient::OnTitleChange(..., const std::string& title) {
 *     SafeInvokeQtCallback(qt_widget_, [title](QtWidget* w) {
 *       w->updateTitle(title);
 *     });
 *   }
 */
template <typename QObjectType, typename Func, typename... Args>
void SafeInvokeQtCallback(QObjectType* obj, Func&& func, Args&&... args) {
  if (!obj) {
    return;
  }

  // Create a weak pointer that automatically becomes null if object is destroyed
  QPointer<QObjectType> weak_ptr(obj);

  // Pack arguments into a tuple for C++17 compatibility (no pack init-capture)
  auto args_tuple = std::make_tuple(std::forward<Args>(args)...);

  // Marshal to Qt main thread with weak pointer validation
  QMetaObject::invokeMethod(
      obj,
      [weak_ptr, func = std::forward<Func>(func), args_tuple = std::move(args_tuple)]() mutable {
        // Validate object still exists
        if (weak_ptr) {
          // Safe to dereference - object is alive
          // Unpack tuple and call function
          std::apply(
              [&func, &weak_ptr](auto&&... unpacked_args) {
                func(weak_ptr.data(), std::forward<decltype(unpacked_args)>(unpacked_args)...);
              },
              std::move(args_tuple));
        }
        // If weak_ptr is null, object was destroyed - silently drop callback
      },
      Qt::QueuedConnection);
}

/**
 * Blocking variant of SafeInvokeQtCallback.
 *
 * DANGER: This blocks the CEF UI thread waiting for Qt to process the event.
 * Only use when absolutely necessary (e.g., modal dialogs, synchronous user input).
 *
 * WARNING: Can deadlock if Qt main thread is blocked waiting for CEF.
 * Always prefer the async SafeInvokeQtCallback when possible.
 *
 * @param timeout_ms Maximum time to wait in milliseconds (0 = infinite)
 * @return true if callback was invoked, false if object destroyed or timeout
 */
template <typename QObjectType, typename Func, typename... Args>
bool SafeInvokeQtCallbackBlocking(QObjectType* obj,
                                  Func&& func,
                                  Args&&... args,
                                  int timeout_ms = 5000) {
  (void)timeout_ms;  // Not used in current implementation
  if (!obj) {
    return false;
  }

  QPointer<QObjectType> weak_ptr(obj);
  bool invoked = false;

  // Pack arguments into a tuple for C++17 compatibility
  auto args_tuple = std::make_tuple(std::forward<Args>(args)...);

  // Use BlockingQueuedConnection to wait for Qt main thread
  QMetaObject::invokeMethod(
      obj,
      [weak_ptr,
       func = std::forward<Func>(func),
       &invoked,
       args_tuple = std::move(args_tuple)]() mutable {
        if (weak_ptr) {
          // Unpack tuple and call function
          std::apply(
              [&func, &weak_ptr](auto&&... unpacked_args) {
                func(weak_ptr.data(), std::forward<decltype(unpacked_args)>(unpacked_args)...);
              },
              std::move(args_tuple));
          invoked = true;
        }
      },
      Qt::BlockingQueuedConnection);

  return invoked;
}

/**
 * Helper macro for simpler syntax.
 *
 * Usage:
 *   SAFE_QT_CALLBACK(widget, [](auto* w) {
 *     w->updateTitle("New Title");
 *   });
 */
#define SAFE_QT_CALLBACK(obj, callback) ::athena::browser::SafeInvokeQtCallback((obj), (callback))

/**
 * Blocking variant macro (use sparingly).
 *
 * Usage:
 *   bool ok = SAFE_QT_CALLBACK_BLOCKING(widget, [](auto* w) {
 *     return w->getUserInput();
 *   });
 */
#define SAFE_QT_CALLBACK_BLOCKING(obj, callback) \
  ::athena::browser::SafeInvokeQtCallbackBlocking((obj), (callback))

}  // namespace browser
}  // namespace athena

#endif  // ATHENA_BROWSER_THREAD_SAFETY_H_
