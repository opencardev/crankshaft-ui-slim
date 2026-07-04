# Android Auto Projection

## 1. Purpose and Scope

- Receive decoded H.264 video frames from `crankshaft-core` and render them on the display.
- Forward touch and key input events from QML to the Android Auto channel in `crankshaft-core`.
- Publish the physical display resolution to `crankshaft-core` so touch coordinates can be correctly scaled (this is its only purpose — it does not affect the video stream or GStreamer pipeline).

Out of scope:

- H.264 decoding (owned by `GStreamerVideoDecoder` in `crankshaft-core`).
- AASDK channel management and phone negotiation (owned by `RealAndroidAutoService` in `crankshaft-core`).
- Display mode / HDMI configuration.

## 2. Ownership and Boundaries

- **Primary owners:** `TouchEventForwarder` (C++), `AAProjectionView.qml`, `main.qml`.
- **Upstream:** `crankshaft-core` WebSocket server — sends decoded frame URLs and channel status; receives touch/key events and the display resolution update.
- **Downstream:** Physical display via Qt `QVideoWidget` or `Image` QML item.

## 3. Architecture Summary

The projection pipeline has two distinct resolution concepts that must not be conflated:

- **Stream / negotiated resolution** — the resolution the phone encodes H.264 at and the resolution `GStreamerVideoDecoder` is initialized with. Fixed at 1920×1080 (configurable in `crankshaft-core`). Owned entirely by `crankshaft-core`.
- **Display / UI resolution** — the physical screen size. Sent once to `crankshaft-core` as a WebSocket message to calibrate touch scaling. Has no effect on the video pipeline.

Decoded frames arrive as QML image provider URLs. The QML `Image` item scales them to fit the physical display using `fillMode: PreserveAspectFit`. Touch events are mapped back from display coordinates into the stream coordinate space by `TouchEventForwarder`.

## 4. Component Map

| Component | File(s) | Responsibility |
| --- | --- | --- |
| Touch and resolution forwarder | `src/TouchEventForwarder.h`, `src/TouchEventForwarder.cpp` | Scale touch/key events; publish display resolution to core once per genuine resize |
| AA QML projection view | `src/qml/AAProjectionView.qml` | Projection `Image` + touch handlers for the dedicated AA view |
| Main layout projection surface | `src/qml/main.qml` | Inline projection surface in the main UI layout |
| AA facade | `src/AndroidAutoFacade.h`, `src/AndroidAutoFacade.cpp` | QML-exposed bridge: relays `projectionFrameUrl`, status signals, and control calls to/from `crankshaft-core` |

## 5. Runtime Sequence

1. `AndroidAutoFacade` connects to the `crankshaft-core` WebSocket and subscribes to `channel-status` and frame-ready topics.
2. On connection, `Component.onCompleted` / `onVisibleChanged` in the QML surfaces call `updateTouchForwarderDisplaySize()`, which publishes the physical display resolution to core once.
3. When `crankshaft-core` signals a new frame, `AndroidAutoFacade::projectionFrameUrl` changes, triggering the QML `Image` to re-render.
4. Qt scales the decoded frame (1920×1080) to the physical display using `fillMode: PreserveAspectFit`.
5. Touch/key events from the QML surface are forwarded via `TouchEventForwarder` to core, with coordinates scaled from display space to stream space.

## 6. Data and Control Flows

### 6.1 Inbound Inputs

- Decoded video frame URL updates from `AndroidAutoFacade::projectionFrameUrl` (emitted per decoded frame by core).
- Channel status updates from `crankshaft-core` WebSocket (`channel-status` topic).
- QML touch events (`MultiPointTouchArea`, mouse area, key press/release).

### 6.2 Processing

- `TouchEventForwarder::setDisplaySize()` is called when the QML projection surface layout changes (`onPaintedWidth/HeightChanged`, `onWidth/HeightChanged`, `onVisibleChanged`, `Component.onCompleted`).
  - If the resolved display resolution has changed since the last publish, it sends `android-auto/display/resolution` to `crankshaft-core`.
  - If unchanged, the publish is suppressed via `m_lastPublishedResolution` guard.
- `TouchEventForwarder::scaleCoordinates()` maps display-space touch points to stream-space AA coordinates using `m_displaySize` and `m_androidAutoSize`.

### 6.3 Outbound Outputs

- WebSocket publish to `android-auto/display/resolution` — sent once at startup and once per genuine display resize. Used by core for touch scaling only.
- WebSocket publish to `android-auto/touch` and `android-auto/key` — scaled touch/key events forwarded to the AA input channel.

## 7. Configuration

| Key | Default | Effect | Notes |
| --- | --- | --- | --- |
| Physical display size | Auto-detected from `QGuiApplication::primaryScreen()` | Used for touch scaling and the one-time display-resolution publish | Detected in `TouchEventForwarder::setDisplaySize()` |

Stream resolution and DPI are configured in `crankshaft-core` — see `ANDROID_AUTO.md §13.6`.

## 8. External Dependencies

- Qt 6 (`QML`, `Quick`, `Multimedia`, `WebSockets`).
- `crankshaft-core` WebSocket server running and reachable on `localhost`.
- `QGuiApplication::primaryScreen()` must return a valid screen at startup for display-size detection.

## 9. Observability and Diagnostics

Log tags to watch in `journalctl -u crankshaft-ui-slim`:

- `[TouchEventForwarder]` — display-size changes and resolution publishes.
- `[AndroidAutoFacade]` — connection state and frame-URL updates.
- `[CoreClient]` — WebSocket connection and message delivery.

Expected healthy log sequence at session start:

```
[TouchEventForwarder] Display size changed to: 1920x1080
[TouchEventForwarder] Publishing display resolution to core: 1920x1080
[AndroidAutoFacade] Connection established
[AndroidAutoFacade] Video active
```

**Resolution storm diagnostic**: if `crankshaft-core` journal shows `Display resolution set to 1920x1080` repeating more than twice per second, the `onProjectionFrameChanged` connection has been re-added in QML. See §10.

## 10. Failure Modes and Recovery

| Symptom | Likely Cause | Detection | Recovery |
| --- | --- | --- | --- |
| Repeated `Display resolution set to NxM` in core journal (>2/s) | `onProjectionFrameChanged` connected to `updateTouchForwarderDisplaySize()` in QML | Core journal flooded with identical resolution messages | Remove `onProjectionFrameChanged` Connections block from `AAProjectionView.qml` and `main.qml`; check `m_lastPublishedResolution` guard is present in `TouchEventForwarder::setDisplaySize()` |
| Touch events land at wrong position | `m_displaySize` or `m_androidAutoSize` not updated after window resize | Touch drift visible when display is rotated or resized | Ensure `onPaintedWidth/HeightChanged` handlers call `updateTouchForwarderDisplaySize()` |
| Projection shows wrong aspect ratio | `fillMode` changed from `PreserveAspectFit` | Stretched or cropped video | Restore `fillMode: Image.PreserveAspectFit` on the projection `Image` item |
| Black projection surface despite active connection | Frame URL not updating | No video frames logged in core | Check `GStreamerVideoDecoder` is initialized; check `videoFrameReady` signal connection chain |

## 11. Security and Safety Notes

- Touch events passed to `crankshaft-core` contain display coordinates only; no raw input device data is exposed.
- The WebSocket connection is local (`localhost`) and unauthenticated; no cross-origin or network exposure.

## 12. Testing Strategy

- **Unit tests:** `src/tests/test_core_mock_integration.cpp` — `testDisplayResolutionPublishingUsesScreenAwareScaling()` validates that `TouchEventForwarder::resolvePublishedDisplayResolution()` prefers the physical screen size over the rendered size.
- **Manual:** start core + UI, connect phone, observe journal for absence of resolution storm, verify projected video fills screen with correct aspect ratio, verify touch lands accurately.

## 13. Known Pitfalls

### onProjectionFrameChanged must not call updateTouchForwarderDisplaySize()

`onProjectionFrameChanged` fires on every decoded video frame (~30 fps). Connecting it to `updateTouchForwarderDisplaySize()` → `setDisplaySize()` → WebSocket publish causes ~16 identical `android-auto/display/resolution` messages per second to `crankshaft-core`. This was the root cause of the resolution storm that caused HDMI flicker (fixed 2026-07-04, `bugfix/resolution_storm`).

Genuine resize triggers — `onPaintedWidth/HeightChanged`, `onWidth/HeightChanged`, `Component.onCompleted`, `onVisibleChanged` — fire only when the layout actually changes, which is the correct trigger.

The `m_lastPublishedResolution` deduplication guard in `TouchEventForwarder::setDisplaySize()` provides a defensive backstop against any future reconnection of this signal.

### Stream resolution is not the display resolution

Do not send `m_resolution` from `crankshaft-core` to the QML surface and expect it to match the physical display. The stream resolution (1920×1080) and the physical display (e.g. 1024×600) are independent. Qt's `PreserveAspectFit` handles the mapping; no manual scaling is required in QML.

## 14. Change Log Notes

- 2026-07-04: Created. Documents projection architecture, resolution separation, resolution-storm fix, and touch scaling design.
