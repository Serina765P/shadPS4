<!--
SPDX-FileCopyrightText: 2026 shadPS4 Emulator Project
SPDX-FileCopyrightText: 2026 Serina765P
SPDX-License-Identifier: GPL-2.0-or-later
-->

# shadPS4 - 《偶像大师》专供优化分支（二次修改版）

> [!NOTE]
> **开发声明**：本修改分支与原 fork 分支均**完全由 AI 协同开发**（原作者基于 Vibe Coding 诊断编写，本二次修改版基于 Codex 开发完成）。

本仓库是基于 PlayStation 4 模拟器 [shadPS4](https://github.com/shadps4-emu/shadPS4) 以及原 fork 作者 **水年沐华**（[Nyces / Muhua](https://github.com/Nyces)）针对《偶像大师》系列优化分支的**二次修改版本**。

原 shadPS4 主线 README 包含大量繁杂的开发与通用说明，作为《偶像大师》专供优化分支，本文档仅聚焦于**游戏支持现状、关键设置、两阶段修改要点及注意事项**。

---

## 🎮 支持的游戏与运行现状

本版本主要针对 PS4 平台上的两款《偶像大师》正作进行专项深度适配：

| 游戏名称 | 日版 ID | 繁中版 ID | 运行状态 |
| :--- | :--- | :--- | :--- |
| **《偶像大师 白金星光》**<br>(THE IDOLM@STER: Platinum Stars) | `CUSA03628` | `CUSA05706` | **✅ 可完美游玩 (Playable)** |
| **《偶像大师 星光舞台》**<br>(THE IDOLM@STER: Stella Stage) | `CUSA07753` | `CUSA09153` | **✅ 可完美游玩 (Playable)** |

> [!WARNING]
> 本分支包含大量针对偶像大师游戏特化的图形着色器与音频管线补丁，**可能无法正常运行其他通用 PS4 游戏**。如需游玩其他游戏，建议使用 shadPS4 官方主线版本。

---

## ⚙️ 关键使用方法与配置建议（必读）

为了获得稳定、正常的游玩体验，请务必注意以下设置：

1. **图形设置：必须开启“精确回读”（Precise Readback）**
   - 在配套启动器（如 QtLauncher）或全局/针对游戏的图形设置中，将 **Readback mode（回读模式）** 设置为 **`Precise`（精确）**。
   - **原因**：若未开启回读，长时间游玩或特定场景下可能会偶发模型顶点拉伸（如人物模型撕裂）或贴图错乱。
2. **纯手柄游玩体验**
   - 本二次修改版已彻底消除离线游玩时的阻塞弹窗，连接手柄后可全程脱离鼠标使用。
3. **必要系统固件（sys_modules）**
   - 模拟器正常运行依赖从正版 PS4 导出的固件模块。请将以下常见关键模块置于模拟器的 `sys_modules/` 目录下（如 `libSceAudiodec.sprx`、`libSceFont.sprx`、`libSceJob.sprx` 等）。
4. **已知偶发问题（原作者反馈）**
   - 特定过场播片时，可能偶发画面微小抽搐/跳帧现象（原作者已知，后续持续跟进）。
   - 若长时间连续游玩偶发轻微材质渲染瑕疵，通常不影响游戏主流程与正常存档。

---

## 🛠️ 修改内容与优化汇总

### 一、 本分支（二次修改版）新增改动

针对原 fork 在手柄游玩体验上的痛点，以及同步最新模拟器核心，进行了如下二次修改：

1. **静默/自动确认离线 PSN 错误弹窗**
   - 彻底解决了手柄游玩时频繁弹出 PSN 离线/登出提示、手柄无法关闭必须伸手拿鼠标去点确认的体验问题。
   - 在未配置 ShadNet 账户或离线状态下，模拟器将自动确认并静默跳过该网络警告；若配置了有效 ShadNet 账号，仍保留正常的登出提示流程。
2. **自动确认奖杯参数错误弹窗**
   - 当本地未注册或未挂载对应的奖杯数据时，自动确认相关参数告警，不再打断游戏主进程与手柄操作。
3. **同步并 Backport shadPS4 主线核心修复**
   - 同步了主线大量关键提交：
     - 扩大 Readback 故障处理窗口（#4839）
     - 修复纹理缓存镜像校验（#5110）
     - 释放嵌套间接缓冲任务帧内存（#5258）
     - 原始缓冲写入后正确失效缓存图像（#5092）
     - 恢复 Windows 内存映射区域分割后的保护属性（#5317）
     - 修复停止模拟时的线程死锁（#4859）
     - 重构数组模式下的 Mip 属性计算（#5001）与平铺纹理布局（#5196）
     - 覆盖呈现等待与片元纹理读取（#5231）
4. **底层管线与音频细节适配**
   - 补全 96-bit 瓦片着色管线、多采样实例校验适配；
   - 引入 WordFilter 句柄初始化逻辑以替换原有的零存根；
   - 调整 AudioOut 端口关闭等待逻辑，提升整体稳定性。
5. **完善 CI / GitHub Actions 自动构建**
   - 构建流水线全面升级，支持 Windows x64 与 Linux x64 自动编译、回归测试与产物打包。

---

### 二、 原 Fork（水年沐华分支）的核心修复（继承保留）

原 fork 作者针对游戏长期存在的渲染与音频痛点，进行了多项关键攻坚：

1. **模型顶点拉伸与 GPU 缓冲同步修复**
   - 重写 GS Copy Shader 解析机制，支持通用常量读取与多种标量操作码。
   - 增加 GS Copy 数据回退映射机制（Fallback GsCopyData），防止着色器解析失败导致崩溃。
   - 完善 GPU 缓冲修改追踪与页面对齐，补全 SSBO 读写内存屏障，从根本上缓解了角色顶点拉伸撕裂。
2. **音频卡顿、延迟与 S4U Live 动作冻结修复**
   - **AJM 批处理优化**：引入滑动保留窗口与批处理状态维护，解决槽位耗尽静音；修正 AT9 多帧解码边界，杜绝 S4U Live 舞蹈卡死。
   - **SDL AudioOut 锁优化**：移除内部持有端口锁时的 Sleep，改为按端口细粒度加锁，消除音频卡顿与主线程阻塞。
   - **线程优先级提升**：将 AJM 解码与音频输出线程优先级提升至 VeryHigh，高负载下仍能保持约 21ms 的稳定音频节奏。
3. **Vulkan 编译与调度调优**
   - 针对部分着色器禁用死锁优化，主信号量引入超时刷新机制，提高稳定性。

---

## 🚀 下载与使用

请前往本仓库的 [Releases 页面](https://github.com/Serina765P/shadPS4/releases) 下载最新的 Pre-release 构建版本：

- **Windows 用户**：下载 `shadps4-idolmaster-windows-x64-*.zip`，解压后搭配 [QtLauncher](https://github.com/shadps4-emu/shadps4-qtlauncher/releases) 启动，或直接运行 `shadPS4.exe` 引导游戏。
- **Linux 用户**：下载 `shadps4-idolmaster-linux-x64-*.tar.gz` 解压使用。

---

## 🤝 致谢与原作者联系方式

- 感谢 [shadPS4](https://github.com/shadps4-emu/shadPS4) 官方团队及全体开源贡献者。
- 感谢原 fork 作者 **水年沐华**（Nyces）前期的反复测试、日志诊断与代码贡献！
  - Bilibili：[https://space.bilibili.com/3461562899630581](https://space.bilibili.com/3461562899630581)
  - Telegram：[https://t.me/nny_mhua](https://t.me/nny_mhua)
