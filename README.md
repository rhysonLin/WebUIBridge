# WebUIBridge

[中文文档](README_CN.md)

## Overview
`WebUIBridge` is a reusable Unreal Engine runtime plugin that bridges `UWebBrowser` and JavaScript with one unified event protocol.

The web side sends messages in this format:

```json
{
  "type": "EventName",
  "data": {}
}
```

`data` can be any JSON value: object, array, string, number, boolean, or `null`.

## Features
- Reusable runtime plugin
- Automatically injects `window.ueBridge`
- Single event protocol: `{ type, data }`
- Built-in click capture for `button`, `a`, `[id]`, and `[data-ue-click]`, reported through the same event channel
- Includes `Automation` tests

## Installation
1. Copy `Plugins/WebUIBridge` into your project's `Plugins` directory.
2. Make sure the `WebBrowserWidget` plugin is enabled.
3. Enable `WebUIBridge` in your `.uproject`.
4. If you use it from C++, add `"WebUIBridge"` to your module dependencies in `Build.cs`.

## Blueprint / C++ Usage
Recommended component usage:

1. Add `Web Browser With Bridge` in UMG.
2. Set `BridgeObjectName` if needed.
3. Bind `OnBrowserEvent` directly on the widget.
4. Call the normal `LoadURL(...)`. No extra bridge setup is required.

Manual bridge usage is still supported:

Create a `UWebUIBridge`, bind a browser, and call:

```cpp
Bridge->SetupBridge(WebBrowser, TEXT("ueBridge"));
```

Listen to:
- `OnBrowserEvent(EventName, PayloadJson)`

Notes:
- `EventName` maps to the incoming message `type`
- `PayloadJson` maps to the incoming message `data`
- The widget auto-installs the bridge during its normal lifecycle, so no extra setup call is needed
- `Web Browser With Bridge` internally owns a `UWebUIBridge`

## Web API
After injection, the page can call:

```js
window.ueBridge.emit({
  type: "Login",
  data: { userId: 1001, token: "abc" }
});

window.ueBridge.sendEvent("InventoryUpdate", {
  page: 3,
  items: [{ id: 1, count: 2 }]
});
```

Automatic click event example:

```html
<button id="StartButton" data-ue-click="StartGame">Start</button>
```

UE will receive:

```json
{
  "type": "click",
  "data": {
    "id": "StartButton",
    "tag": "button",
    "text": "Start",
    "href": "",
    "event": "StartGame"
  }
}
```

## Running Tests
Included automation tests:
- `WebUIBridge.Protocol.UnifiedMessage`
- `WebUIBridge.Protocol.GenericClickType`
- `WebUIBridge.Protocol.ScriptApi`

Run them from the Unreal Editor Automation panel, or from command line:

```bat
UnrealEditor-Cmd.exe YourProject.uproject -ExecCmds="Automation RunTests WebUIBridge.Protocol" -unattended -nop4 -nosplash -NullRHI
```

## Publishing Notes
- Keep `README.md`, `README_CN.md`, `WebUIBridge.uplugin`, and `Source` in your GitHub release
- Do not commit `Binaries`, `Intermediate`, or `DerivedDataCache`
