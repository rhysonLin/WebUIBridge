# WebUIBridge

[English README](README.md)

## 简介
`WebUIBridge` 是一个可复用的 Unreal Engine Runtime Plugin，用于在 `UWebBrowser` 与网页 JavaScript 之间建立统一事件桥接。

网页侧只允许使用这一种消息格式：

```json
{
  "type": "EventName",
  "data": {}
}
```

其中 `data` 支持任意 JSON 值：对象、数组、字符串、数字、布尔值或 `null`。

## 特性
- Runtime Plugin，可直接复用于其他项目
- 自动向页面注入 `window.ueBridge`
- 单一事件协议：`{ type, data }`
- 内置点击采集，按钮、链接、带 `id` 或 `data-ue-click` 的元素会通过同一事件通道上报
- 包含 `Automation` 测试

## 安装
1. 将 `Plugins/WebUIBridge` 复制到你的项目 `Plugins` 目录。
2. 确认项目已启用 `WebBrowserWidget` 插件。
3. 在 `.uproject` 中启用 `WebUIBridge`。
4. 如果 C++ 模块要直接引用，给对应 `Build.cs` 增加 `"WebUIBridge"` 依赖。

## Blueprint / C++ 用法
推荐直接使用组件方式：

1. 在 UMG 中添加 `Web Browser With Bridge`
2. 按需设置 `BridgeObjectName`
3. 直接在组件上绑定 `OnBrowserEvent`
4. 像平常一样调用默认的 `LoadURL(...)`，不需要额外调用 bridge 初始化

原来的手动桥接方式仍然保留：

创建一个 `UWebUIBridge`，绑定浏览器后调用：

```cpp
Bridge->SetupBridge(WebBrowser, TEXT("ueBridge"));
```

监听事件：
- `OnBrowserEvent(EventName, PayloadJson)`

说明：
- `OnBrowserEvent` 的 `EventName` 对应消息里的 `type`
- `PayloadJson` 对应消息里的 `data`
- 组件会在正常生命周期里自动安装 bridge，因此不需要额外 Setup 调用
- `Web Browser With Bridge` 内部会自动持有一个 `UWebUIBridge`

## 网页侧 API
插件注入后可直接调用：

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

点击自动事件示例：

```html
<button id="StartButton" data-ue-click="StartGame">Start</button>
```

点击后 UE 侧会收到：

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

## 运行测试
内置自动化测试：
- `WebUIBridge.Protocol.UnifiedMessage`
- `WebUIBridge.Protocol.GenericClickType`
- `WebUIBridge.Protocol.ScriptApi`

可在 Unreal Editor 的 Automation 面板运行，或命令行运行：

```bat
UnrealEditor-Cmd.exe YourProject.uproject -ExecCmds="Automation RunTests WebUIBridge.Protocol" -unattended -nop4 -nosplash -NullRHI
```
