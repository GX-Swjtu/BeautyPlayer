# 映颜 · BeautyPlayer

基于 Qt 6 Multimedia 和 GPUPixel 的实时美颜播放器。自有代码采用 **MIT**，支持修改、分发和商业使用；第三方组件保留各自许可。

**Windows x64 和 Android ARM64 已运行验证。** Android 在骁龙 8 Elite / SM8750、Android 16 真机上确认使用高通 MediaCodec 硬解码、libyuv NEON 和 Adreno 830 GPU 美颜。默认美颜的 1080p30、原尺寸 4K、暂停调参和播放手势已通过测试；其他手机仍需验证。详见 [Android 构建与安装](docs/ANDROID.md) 和 [真机记录](docs/ANDROID-VALIDATION.md)。

## 功能

- 本地文件和 HTTP(S) 视频直链，音量、倍速、循环、全屏。
- 默认开启磨皮 70、美白 32、冷暖 −4、色调 +2.5。瘦脸、大眼、口红、腮红初始关闭，按需启用；默认不运行人脸检测。
- 暂停调参即时生效；按住“看原图”比较；“恢复推荐”恢复默认预设。冷暖/色调影响全画面。
- 自动记住最近 200 个视频的位置；切换文件、暂停、退出和每 5 秒保存。接近片尾或播完后，下次从头开始。
- **长按画面临时 3× 播放**，松开恢复原速；可在播放设置中改为 1.5–5×。
- **左右滑动快进/快退**，显示目标位置，松开才跳转；双击播放/暂停，单击显示/隐藏控制栏。
- Windows 视频库自动扫描“视频”和“下载”，支持添加/移除目录、子文件夹分组、搜索和刷新。启动先显示缓存，后台重新扫描；不读取视频内容或生成缩略图。
- Android 视频库查询系统 MediaStore 索引，支持按目录分组、搜索和部分视频授权；也可用系统文件选择器打开。

默认美颜第一次升级到 v0.2 时应用一次，以后保留用户修改。关闭“美颜”总开关即可观看原图。全部五官/美妆关闭时跳过检测；开启时处理检测器返回的第一张脸。

## 构建

需要 Windows x64、Visual Studio 2022 的“使用 C++ 的桌面开发”、Qt **6.11.2 MSVC 2022 x64**（Multimedia、Quick、Quick Controls）、CMake ≥3.24 和 Ninja。

v0.2.1 额外用 **clang-cl** 编译 libyuv 的完整 SIMD 内核，播放器仍用 MSVC。脚本支持 PATH 中的 LLVM、VS C++ Clang 工具和 Android NDK 自带的 clang-cl；也可设置 `BEAUTY_CLANG_CL` 为其完整路径。缺少时可通过 `winget install LLVM.LLVM` 安装。运行便携包不需要这些编译工具。

```powershell
git clone https://github.com/GX-Swjtu/BeautyPlayer.git
cd BeautyPlayer
$env:QT_ROOT = 'D:\Qt\6.11.2\msvc2022_64'  # 换成自己的 Qt kit
.\scripts\build.cmd
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\package.ps1
.\scripts\run.ps1
```

脚本自动通过 `vswhere` 找到 MSVC，也支持 `BEAUTY_VS_ROOT` 指定 VS 安装目录。Qt 工具或 CMake/Ninja 可来自 PATH；没有时尝试 Qt 的 Tools 目录。依赖按 `dependencies.lock.json` 下载并校验 SHA256，所有依赖和产物放在项目内。MinGW 与此 GPUPixel MSVC SDK 不兼容。

便携目录是 `dist/windows-x64`，双击其中的 `BeautyPlayer.exe` 即可使用；移动时复制整个目录。仓库不包含 DLL、模型、测试视频和编译产物。

## 操作

Android ARM64 构建：`.\scripts\build-android.ps1`；输出 `dist/android-arm64/BeautyPlayer-arm64-v8a.apk`。手机连接 USB 调试后运行 `.\scripts\test-android.ps1` 安装启动，`-SelfTest` 可执行手机端颜色、NEON、GL 和模型检查。详见 [Android 文档](docs/ANDROID.md)。

视频库右上角“文件”打开视频，“⋮”中添加目录、播放网址或调整设置。播放界面左下角“美颜”打开可收起的底部面板，五官与美妆各有独立开关。

键盘：空格播放/暂停，左右键 ±10 秒，Ctrl+O 打开文件，F11 全屏，Esc 关闭面板或退出全屏。扫描和播放记录仅存本机，不上传媒体或历史。移除扫描目录不会删除文件；缓存不会自动监听文件变动，点“刷新”更新。

## 性能与范围

Qt FFmpeg 自动选择可用硬件解码器：Windows 已确认 D3D11VA，Android 真机已确认高通 `c2.qti.avc.decoder`；美颜由 OpenGL/OpenGL ES GPU 执行。帧转换和 GPU 处理分别在两个工作线程运行，队列固定上限，过载时丢弃尚未处理的旧输入。Windows 原图模式直接传递 QVideoFrame；Android 原图仍在同一转换线程取帧，避免 SurfaceTexture 上下文争用，再跳过 GPUPixel。输出图像缓冲区可安全复用。

libyuv 加速常见 NV12/NV21/YUV420P/YV12 到 RGBA 的转换，Windows 运行时选择 **AVX2 → SSSE3/SSE2 → 标量**，Android ARM64 选择 **NEON**，保留双线性色度插值、原分辨率和 BT.601/709 全/限范围。色彩信息不明确、HDR 或复杂 surface 变换继续交给 Qt。未对整个程序强制开启特定高端指令；硬解纹理由 Qt GPU 转换，人脸检测副本使用 libyuv 缩图。

保留原始像素尺寸。美颜路径仍有 CPU/GPU 数据传输，并非全程零拷贝；实际帧率取决于视频、GPU 和开启的特效。支持范围为 8 位 SDR、单人脸实时播放，不包含导出、HDR 保真或摄像头。实测数据与限制见 [v0.2 验证记录](docs/VALIDATION-v0.2.md)。

## 测试

```powershell
# 测试图像、旋转、视频库、播放记忆
ctest --test-dir build/windows-release --output-on-failure
# FFmpeg 用于生成测试素材，不是应用运行依赖
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/prepare-test-media.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/verify.ps1
# 10 分钟连续播放与跳转测试
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/verify.ps1 -LongRun
# 依次测试默认/全部特效/原图 4K 与音画同步，避免多个播放器争用 GPU
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/benchmark.ps1
```

单独诊断：

```powershell
.\dist\windows-x64\BeautyPlayer.exe --probe artifacts/check --seconds 60 --profile default test-media/face-1080.mp4
.\dist\windows-x64\BeautyPlayer.exe --fresh --phone --library-folder E:/Videos
```

`--profile` 可选 `default`（默认美颜）、`all`（固定全部特效，诊断默认值）、`original`；`--exercise` 自动操作暂停/对比/跳转/续播，并在可见窗口发送鼠标手势检查。`--hidden` 用于长测。诊断不修改用户的设置、历史或扫描目录。`--fresh` 同样临时使用独立的内存设置。

`--converter auto` 是默认 SIMD 模式；`sse` 禁用 AVX、`scalar` 禁用 libyuv 特定指令集内核、`qt` 使用原 Qt 转换，方便在同一可执行文件中对照。诊断 JSON 记录实际转换后端和 libyuv/Qt 命中帧数。运行 `scripts/benchmark-simd.ps1` 可依次复测，细节见 [SIMD 验证记录](docs/VALIDATION-v0.2.1.md)。

## 结构与许可

`qml/Main.qml` 是响应式界面；`BeautyVideoBridge` 管理解码、帧转换/GPU 流水线及帧版本；`GPUPixelPipeline` 封装滤镜和 OpenGL；`PlaybackSession` / `ResumeStore` 管理倍速与续播；`VideoLibrary` 提供异步目录索引。

[MIT](LICENSE) 适用于本项目原创代码，Qt 示例部分保留 BSD-3-Clause。Qt 动态库按 LGPL 使用，GPUPixel 为 MIT；Mars-Face 和配套模型为上游预编译依赖，**不因本仓库采用 MIT 而变成 MIT 开源代码**。这次发布仅上传源码与下载脚本，未上传 SDK 或便携包。详见 [第三方声明](THIRD_PARTY_NOTICES.md) 和 [实现说明](docs/IMPLEMENTATION.md)。
