# v0.4.4：AV1 / Opus 播放与硬件解码

日期：2026-09-30。基于 v0.4.3（`d879c9e`）。本轮按用户要求提供 APK，不连接手机；Android 的本版运行状态为 **未验证**，不能沿用旧版 H.264 的真机结果声称 AV1 已通过。

## 问题与实现

- Qt 6.11.2 Android FFmpeg 后端的 `availableHWCodecs()` 将系统 MediaCodec MIME 转成 FFmpeg codec ID 时，只有 avc/hevc/vp8/vp9，漏掉 av01。即使系统报告 AV1 硬件解码器，也会被该过滤器漏掉。补齐 `av01 → AV_CODEC_ID_AV1`。来源：[固定版本 Qt 源码](https://github.com/qt/qtmultimedia/blob/v6.11.2/src/plugins/multimedia/ffmpeg/qffmpegcodecstorage.cpp)。
- 用匹配的 Qt 源码和 FFmpeg 7.1.5 头文件重编译 codec-storage，链接 Qt kit 的其余实现，生成独立的 LGPL 动态插件；不修改安装的 Qt。下载 SHA256 在 `android-dependencies.lock.json`，补丁过程在 `cmake/AndroidMediaPlugin.cmake`。链接图确认使用新对象，没有抽取静态库中的旧 codec-storage 对象。
- Gradle 在原生库合并前替换插件。交付前校验 APK 内插件与编译产物的 SHA256 一致，避免 Qt 部署流程带回旧插件。
- Qt 配套 FFmpeg 已包含 Opus CPU 解码器。扩大文件选择和 Windows 拖放支持，允许直接打开 `.opus/.ogg` 等音频；音频显示占位画面并禁用美颜入口，视频扫描范围不变。没有声称 Opus 使用 GPU/MediaCodec 硬解。
- 播放诊断新增音频解码帧数、采样峰值和实际音视频编码；仅有画面不能再算音轨通过。保留默认磨皮 20、美白 20、冷暖/色调 0。

没有取得用户的原始失败文件，因此不能断言所有 Opus 文件失败都由同一个问题引起。Windows 旧版基线本来就能播放本轮 AV1 + Opus 样片，本次没有把已有的 Windows 硬解能力当作新实现。

## Windows 实测

环境：Windows x64，Qt 6.11.2 / FFmpeg 7.1.5；本机 NVIDIA RTX 4070 Laptop GPU + Intel UHD。使用便携目录启动，默认美颜开启。生成的 12 秒测试片每项诊断运行 8 秒，日志必须包含实际选择的 D3D11VA 格式（`Selected format ... for hw 7`），仅枚举到硬件不算通过。

| 素材 | 实测结果 | 末段显示帧率 |
|---|---|---|
| AV1 8 位 + Opus，720p30 WebM | D3D11VA 出画面，Opus 解出非静音采样，通过 | 29.97 fps |
| AV1 8 位 + Opus，720p30 MKV | 同上，通过 | 30.03 fps |
| AV1 8 位 + Opus，720p30 MP4 | 同上，通过 | 30.00 fps |
| AV1 10 位 SDR + Opus，1080p30 MP4 | 同上，通过；保留 1920×1080 输出 | 30.00 fps |
| HEVC + Opus，720p30 MKV | D3D11VA 和 Opus，通过 | 29.97 fps |
| VP9 + Opus，720p30 WebM | D3D11VA 和 Opus，通过 | 30.00 fps |
| 独立 Opus 音频 | 解码帧、非零采样、占位画面、拖放入口通过 | 不适用 |

各项取得约 35 万音频采样帧，峰值约 0.126。视频案例最终音视频 PTS 差在 −13.5 至 +26.5 ms，属于短测诊断数据，不等同于物理声画延迟、长时间漂移或扬声器听感测量。

首轮 HEVC 汇总脚本把 Qt 返回的 `H265` 错写成期望值 `H.265`；修正断言后单项复测通过。其原始播放检查始终通过。首轮交互回归也发现“重新打开视频关闭面板”的新增行为破坏原流程；现仅在加载纯音频时收起面板，保留视频流程。

最终便携包的 AV1 + Opus 交互回归 **57/57 通过**，覆盖暂停调参、对比、参数归零、全屏、面板操作、倍速长按、滑动、续播、返回列表暂停、底部悬停和拖放。最终纯 Opus 音频复测通过，截图确认音频占位正常。CTest 的 PipelineTests、FrameConversionTests、LibraryHistoryTests 三个套件均通过。

## Android 包检查

- Release 原生构建与 APK 打包通过；版本名 0.4.4，versionCode 404，最低 API 28，仅 arm64-v8a。
- 86 个原生库的 ARM64 格式、16 KB PT_LOAD 对齐和 DT_NEEDED 依赖检查通过；APK 的 zipalign 16 KB 检查通过。
- 修复插件在 APK 内的 SHA256 与编译产物一致；导出 Qt 插件入口完整。配套 libavcodec 包含 `av1_mediacodec` 和 `opus`。
- 使用既有本机测试密钥签名，apksigner v2 验证通过。原生构建为 Release，安装包仍是 debug 签名测试包。
- **未执行** 手机安装、启动、AV1/Opus 播放、MediaCodec 实际解码器选择、手机性能及物理 16 KB 页设备运行。

## 范围与复现

AV1 硬解取决于设备、系统驱动、分辨率和 profile；当前 Qt 配套 FFmpeg 未包含 libdav1d/libaom AV1 软件回退，不保证没有 AV1 硬解的设备可以播放。10 位 BT.709 SDR 样片成功不表示 HDR 色彩保真。没有测试 AV1 4K、长时间温控、网络直播或用户原始失败文件。

```powershell
.\scripts\prepare-codec-media.ps1
.\scripts\verify-codecs.ps1 -RequireHardware
# 未来连接手机后，分别检查实际 MediaCodec 日志与音频计数
.\scripts\test-android.ps1 -Video .\test-media\codecs\av1-opus.webm -Seconds 15
.\scripts\test-android.ps1 -Video .\test-media\codecs\audio.opus -Seconds 15
```

原始证据留在忽略目录 `artifacts/v044`：`windows-before.*` 是旧版基线，`windows-codecs/` 是完整矩阵，`windows-codecs-hevc/` 为修正期望值后的 HEVC 复测；`av1-exercise-final.json`、`audio-final/` 和 `ctest.log` 为最终交互、音频及测试套件结果；`apk-inspection-final.json` 和 `android-build-final.log` 为最终 APK 审计及签名结果。测试片和二进制不提交 Git。

| 最终产物 | SHA256 |
|---|---|
| Windows EXE | `16D87FDFBB21BA0BD6751844FA7AA9B74C5F0A711F7AD55CAC6370AE4A0AA9A8` |
| Android APK | `E0DFC8921F450E0D413A71931B9CFC6FE54167A2B69D1703B03A22603220C4E3` |
