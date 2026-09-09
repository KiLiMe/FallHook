# FallHook

[English](README.en.md) | 简体中文

FallHook 是《辐射 4》(Fallout 4) 的 F4SE/CommonLibF4 插件，提供免源码的运行时翻译与 hook 支持。

它在运行时加载 xTranslator XML 翻译并应用到游戏内文本，无需随附原始/插件源字符串，
也无需重新构建出新的插件。

目标插件输出：

```text
Data/F4SE/Plugins/FallHook.dll
```

## 运行时支持

| 运行时 | 源码树 | 说明 |
| --- | --- | --- |
| Fallout 4 1.10.163 | `src/110163/`、`include/110163/` | 独立模块 |
| Fallout 4 1.11.191 | `src/111191/`、`include/111191/` | AE 模块 |
| Fallout 4 1.11.240 | 复用 1.11.191 | 由 AE 模块覆盖，无独立源码树 |

`src/<runtime>/` 与 `include/<runtime>/` 下的两棵运行时源码树各自持有 hook 偏移与运行时
布局假设。FallHook 通过 Address Library(`REL::ID`) 解析每一个 hook，因此针对 1.11.191
编写的 AE(1.11.x) 模块无需改动代码，即可在 1.11.240 等更新的 AE 补丁上运行 ——
这正是没有 `src/111240/` 目录的原因。

## 安装

1. 为你的《辐射 4》版本安装 [F4SE](https://f4se.silverlock.org/)。
2. 将 `FallHook.dll` 与 `FallHook.ini` 复制到：

   ```text
   <Fallout 4>/Data/F4SE/Plugins/
   ```

3. 将 xTranslator XML 文件放入：

   ```text
   <Fallout 4>/Data/F4SE/Plugins/FallHook/
   ```

## 配置

配置文件位于 `Data/F4SE/Plugins/FallHook.ini`(仓库内为 `config/FallHook.ini`)。常用项：

| 小节 | 键 | 默认值 | 用途 |
| --- | --- | --- | --- |
| `[XML]` | `LoadOrderMode` | `plugin` | `plugin` 按激活插件顺序；`filename` 按全局文件名升序 |
| `[InGameTextHook]` | `Enable` | `false` | 游戏内文本 hook |
| `[Watchdog]` | `Enable` | `false` | 加载/hook 缓慢或卡死告警 |
| `[ActivityWatchdog]` | `Enable` | `false` | 逐 hook 详细活动日志，用于卡顿归因 |
| `[Logging]` | `EnableDebugLog` | `false` | 调试跟踪日志 |

## 构建

前置条件：

- Visual Studio 2022
- CMake 3.21+
- vcpkg，并设置 `VCPKG_ROOT`
- [CommonLibF4](https://github.com/libxse/commonlibf4)，该 checkout 中需包含 `lib/commonlib-shared`

默认情况下，CMake 期望 CommonLibF4 位于 `../CommonLibF4`。如需覆盖：

```powershell
cmake --preset fo4 -DCOMMONLIBF4_DIR=<path-to-CommonLibF4>
cmake --build build --config Release --target FallHook
```

默认部署副本输出到 `build/deploy/F4SE/Plugins`。

## 测试

```powershell
cmake --build build --config Release --target FallHookCoreTests
ctest --test-dir build -C Release --output-on-failure
```

## 持续集成

| 工作流 | 触发条件 | 结果 |
| --- | --- | --- |
| `build` | push / PR 到 `main` | 构建插件、运行单元测试、上传 `FallHook` 产物 |
| `release` | 推送 `v*` 标签 | 构建、测试、打包 `FallHook-<tag>.zip` 并发布 GitHub Release |

## 许可

GPL-3.0。详见 `LICENSE` 与 `NOTICE.md`。
