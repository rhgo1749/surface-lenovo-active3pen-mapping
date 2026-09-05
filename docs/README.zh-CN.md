# Surface Pen Mapper — 使用指南

[한국어](README.ko.md) · [English](README.en.md) · [日本語](README.ja.md)

Surface Pen Mapper 是一个轻量级 Windows 工具，可以把**两个手写笔侧键扩展为四个可独立配置的手势**。

目前已在 **Surface Pro 12 + Lenovo Active Pen 3（MPP）** 上完成实机验证。

## 1. 安装

1. 打开 GitHub **Releases**，下载最新的 `surface-pen-map-arm64.zip`。
2. 解压到任意文件夹。
3. 运行 `surface-pen-map.exe`。

无需安装程序，也不需要额外的 Wacom 平板驱动。

首次启动时 Windows SmartScreen 可能会要求确认。请先确认文件来自本仓库的 GitHub Releases 页面。

## 2. 设置手写笔按钮

两个侧键分别支持两种操作方式：

| 手势 | 操作方法 |
| --- | --- |
| 上侧键点击 | 不触碰屏幕，按下后松开 |
| 上侧键 + 点击屏幕 | 按住上侧键，再用笔尖点击屏幕 |
| 下侧键点击 | 不触碰屏幕，按下后松开 |
| 下侧键 + 点击屏幕 | 按住下侧键，再用笔尖点击屏幕 |

每个手势都可以单独设置为：

- 后退 / 前进
- 左键 / 右键 / 中键点击
- 任意支持的按键或快捷键
- 不执行额外操作

## 3. 设置按键或快捷键

在动作中选择 **Key / shortcut**，点击右侧输入框，然后直接按下想要设置的按键。

例如：

- `Enter`
- `Esc`
- `Tab`
- `Space`
- `Delete`
- 方向键
- `F1` ～ `F24`
- `Ctrl+Z`
- `Ctrl+Shift+T`
- `Alt+Left`
- `Win+D`

`Ctrl+Alt+Delete` 是 Windows 保留的安全注意序列，无法映射。

## 4. 推荐设置示例

如果想尽量不用鼠标，只用手写笔操作 Windows，可以先尝试：

| 手势 | 推荐动作 |
| --- | --- |
| 上侧键点击 | Enter |
| 上侧键 + 点击屏幕 | 不执行额外操作 |
| 下侧键点击 | 后退 |
| 下侧键 + 点击屏幕 | Esc |

之后可以按照自己的工作流自由调整。

## 5. 应用设置与开机启动

修改设置后点击 **Apply**，新的映射会立即生效。

启用 **Start mapper when I sign in to Windows** 后，程序会在下次登录 Windows 时自动在后台启动。

关闭设置窗口不会退出程序，而是隐藏到系统托盘。点击托盘图标即可重新打开设置。

## 注意事项

本工具不会替换 Windows 原本的手写笔驱动，而是在原有行为之外发送额外操作。

因此，**按住上侧键再点击屏幕时，Windows 原生右键行为仍可能同时触发。** 在部分手写应用中，下侧键原本的橡皮擦行为也可能继续保留。

只有遇到问题时再查看 [Troubleshooting](TROUBLESHOOTING.md)。