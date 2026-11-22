# Native macOS desktop integration stays app-side

Decision (gap note `decide-macos-desktop`, Option 1): afterhours does not grow
a menu-bar, status-item, notification, global-hotkey or deep-link API. Only
two desktop-style apps (hanabi, floatinghotel) want that surface, both already
ship working app-side Objective-C++, and none of it can be exercised headless.
This page lists what the library already hands an app-side `.mm` file, so the
next desktop app copies a bridge instead of re-deriving the seams. If a third
desktop-style app shows up, revisit this as a separate `afterhours-desktop`
repo (Option 3) rather than growing the game library.

## What the library does not have

There is no `macos_desktop` plugin and no library API for an application menu,
`NSStatusItem`, `UNUserNotificationCenter`, Carbon/global hotkeys, Spotlight
or Apple-event URL handling. Grepping `src/` finds only two macOS-native
precedents, both deliberately narrow: `src/plugins/native_dialogs_macos.mm`
(file panels) and `src/gestures_macos.h` (trackpad pinch). `tools/mk_bundle.sh`
covers `.app` packaging only (see below). Everything else on this page is a
hook that already exists for other reasons.

## The hooks an app-side `.mm` bridge actually uses

### Window handle (sokol)

The vendored sokol_app already exposes it; afterhours adds no wrapper:

```objc
#import <AppKit/AppKit.h>
// vendor/sokol/sokol_app.h: sapp_macos_get_window()
NSWindow *window = (__bridge NSWindow *)sapp_macos_get_window();
```

Call it only after the window exists (from your frame callback or later, never
before `sapp_run` reaches its init callback) and treat the result as borrowed:
sokol owns the window. On the raylib backend there is no equivalent afterhours
accessor — `GetWindowHandle()` is raylib's, and `src/gestures_macos.h` already
warns not to assume what it hands back on macOS. A bridge that needs an
`NSWindow` is a sokol-backend bridge today; say so in its header.

### Waking the frame loop from a native callback

`afterhours::frame_loop::request_frame()` (`src/frame_loop.h`) is the wake-up.
It is mutex/condition-variable state, so an AppKit, Carbon or notification
callback may call it from its own thread. It matters when the app runs
`frame_loop::Mode::OnDemand`: without a request, a menu pick or hotkey that
arrives while the app is idle changes state nobody draws until the next
unrelated frame. In `Continuous` mode it is harmless bookkeeping.

### Delivering a native event into the app

There is no `jobs::Pool` in the library today (the Tier 1 background-jobs gap
is still open); do not write a bridge against one. The pattern both shipped
bridges use, and the one `native_dialogs::Queue` and `file_watcher`'s `drain`
already model in-tree, is:

1. The native callback records a small owned payload (menu id, URL string,
   hotkey id) in an app-owned mutex/atomic slot or bounded queue, then calls
   `frame_loop::request_frame()`. It never touches ECS state, entities or UI
   from the callback thread.
2. A normal system (or the top of the frame callback) drains that queue on
   the app thread and acts on it.

Bound the queue the way hanabi bounds its pending notification to one newest
payload: a native callback that can fire while the user is away deciding on
an authorization prompt must not accumulate an unbounded replay burst.

### Packaging is already solved

Notifications, deep links and Spotlight all key off bundle identity, which is
`tools/mk_bundle.sh`, not new code (README, "Packaging a desktop app"):

- `--id com.example.myapp` — the stable bundle id notifications and
  CoreSpotlight donation hang off; keep it fixed across rebuilds.
- `--url-scheme myapp` (repeatable) — writes `CFBundleURLTypes`, which is what
  makes the system deliver `myapp://` opens to your Apple-event handler.
- `--entitlements FILE --sign -` (or a real identity) and `--plist-extra FILE`
  for anything the script does not model — the script deliberately does not
  grow a flag per plist key.

## Reference bridge: hanabi

Hanabi ships all of this app-side and is the copy-from example:

| Feature | App-side home |
|---|---|
| Status item + its menu | `src/menubar.mm` (`NSStatusItem`) |
| Notifications (async authorization, one pending payload, foreground presentation) | `src/native_extras.mm` (`native_notifications_start`, `native_notify`) |
| Global hotkey, focus-gated | `src/native_extras.mm` (`HotkeyFocusObserver`, Carbon `RegisterEventHotKey`) |
| Deep links (`hanabi://`) | `src/native_extras.mm` (`handleGetURLEvent`), drained in `src/main.cpp` (`native_take_open_thread`) |
| Install boundary | windowed `app_frame` only, so headless tests/captures never prompt, register a hotkey or donate to Spotlight |

floatinghotel is the smaller data point: it still draws its own menu bar in
`src/ecs/menu_bar_system.h`. That is in-window UI, not a native menu, and it
stays app-side too.

## Rules the two shipped bridges learned the hard way

- **Install native extras only from the windowed path.** Headless tests,
  screenshots and unit executables must never request notification
  authorization, register a global hotkey or install a status item. Gate on
  the same windowed-frame entry the app uses for its first real frame, and
  check bundle identity before calling the platform.
- **Notification authorization is asynchronous and durable.** Request it,
  read the settings back, and distinguish not-determined / denied /
  authorized / provisional. Denied is a final answer, not a transient error:
  do not re-prompt or fall back to a deprecated API. Hold at most the newest
  payload while the answer is outstanding.
- **Gate global hotkeys on app activation.** A Carbon hotkey consumes its
  chord process-wide, including while your app is in the background (hanabi's
  Cmd+Shift+N once stole Chrome's new-incognito chord). Register on
  `NSApplicationDidBecomeActiveNotification`, unregister on resign-active.
- **A native menu is invisible to afterhours input and E2E.** AppKit resolves
  `keyEquivalent` before the sokol view sees the key, and an `NSMenu` has no
  afterhours entity for `assert_ui` to find. Deliver picks through the drain
  above, and do not expect E2E to drive the native menu.
- **Foreground notification presentation is a separate switch** from whether
  the request carries a sound; set both deliberately.

## What would change this decision

A third desktop-style app needing the same surface. At that point extract the
two shipped bridges into a separate `afterhours-desktop` repo that vendors
afterhours (Option 3) — the game library keeps no permanent platform surface
nobody can test headless, and Windows/Linux equivalents stop being this
library's problem.
