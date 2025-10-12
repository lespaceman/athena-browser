/**
 * GTK Window Event Callbacks
 *
 * Internal header for GTK event callback registration.
 * All callbacks forward to GtkWindow methods via user_data pointer.
 */
#ifndef ATHENA_PLATFORM_GTK_WINDOW_CALLBACKS_H_
#define ATHENA_PLATFORM_GTK_WINDOW_CALLBACKS_H_

#include <gtk/gtk.h>

namespace athena {
namespace platform {

class GtkWindow;

/**
 * Callback registration helpers.
 * Groups related callbacks for easier setup.
 */
namespace callbacks {

/**
 * Register window lifecycle callbacks.
 * - delete-event
 * - destroy
 * - key-press-event (for Ctrl+T, Ctrl+W, etc.)
 */
void RegisterWindowCallbacks(GtkWidget* window, GtkWindow* self);

/**
 * Register OpenGL callbacks.
 * - realize (GL context creation)
 * - render (frame rendering)
 * - size-allocate (window resize)
 */
void RegisterGLCallbacks(GtkWidget* gl_area, GtkWindow* self);

/**
 * Register input event callbacks.
 * - button-press-event
 * - button-release-event
 * - motion-notify-event
 * - scroll-event
 * - leave-notify-event
 * - key-press-event
 * - key-release-event
 * - focus-in-event
 * - focus-out-event
 */
void RegisterInputCallbacks(GtkWidget* gl_area, GtkWindow* self);

/**
 * Register toolbar button callbacks.
 * - back, forward, reload, stop buttons
 * - address bar activation
 * - new tab button
 */
void RegisterToolbarCallbacks(GtkWidget* back_btn,
                               GtkWidget* forward_btn,
                               GtkWidget* reload_btn,
                               GtkWidget* stop_btn,
                               GtkWidget* address_entry,
                               GtkWidget* new_tab_btn,
                               GtkWindow* self);

/**
 * Register tab event callbacks.
 * - switch-page (tab switching)
 */
void RegisterTabCallbacks(GtkWidget* notebook, GtkWindow* self);

}  // namespace callbacks
}  // namespace platform
}  // namespace athena

#endif  // ATHENA_PLATFORM_GTK_WINDOW_CALLBACKS_H_
