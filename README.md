# 映颜 · BeautyPlayer

基于 Qt 6 Multimedia 和 GPUPixel 的实时美颜播放器。自有代码采用 **MIT**，支持修改、分发和商业使用；第三方组件保留各自许可。

**Windows x64 和 Android ARM64 已运行验证。** Android 在骁龙 8 Elite / SM8750、Android 16 真机上确认使用高通 MediaCodec 硬解码、libyuv NEON 和 Adreno 830 GPU 美颜。默认美颜的 1080p30、原尺寸 4K、暂停调参和播放手势已通过测试；其他手机仍需验证。详见 [Android 构建与安装](docs/ANDROID.md) 和 [真机记录](docs/ANDROID-VALIDATION.md)。

## 功能

- 本地文件和 HTTP(S) 视频直链，音量、倍速、循环、全屏。
- 默认开启磨皮 70、美白 20，冷暖和色调均为 0。瘦脸、大眼、口红、腮红初始关闭，按需启用；默认不运行人脸检测。
- 暂停调参即时生效；“按住对比”查看原图；“恢复推荐”恢复默认预设。冷暖/色调影响全画面。
- 紧凑美颜面板：底部横滑选择效果，只显示当前效果的一根调整条。视频完整显示在面板上方，不加暗色遮罩；横屏自动压缩面板高度。
- 自动记住最近 200 个视频的位置；切换文件、暂停、退出和每 5 秒保存。接近片尾或播完后，下次从头开始。
- 返回视频库立即暂停并保存位置，再进入播放画面时仍保持暂停。
- Windows 可拖入视频文件播放；鼠标移到底部显示控制栏，静止约 2.5 秒后自动隐藏。
- **长按画面临时 3× 播放**，松开恢复原速；可在播放设置中改为 1.5–5×。
- **左右滑动快进/快退**，显示目标位置，松开才跳转；双击播放/暂停，单击显示/隐藏控制栏。
- Windows 视频库自动扫描“视频”和“下载”，支持添加/移除目录、子文件夹分组、搜索和刷新。启动先显示索引缓存，后台重新扫描；扫描过程只读取文件元数据。
- Android 视频库优先查询系统 MediaStore 索引，也支持添加并授权扫描文件夹；支持部分视频授权和系统文件选择器。
- 单列/双列视频预览，可按文件名、创建时间、文件大小升序或降序排列，设置自动保存。
- 视频缩略图按需在独立后台队列生成，优先使用系统代表帧；缺失或纯黑/白时尝试视频约 10% 位置。结果缓存到本机，重启可复用；文件大小或修改时间变化后更新，30 天未使用自动清理，缓存目标上限 256 MB。
- “视频库设置”可开关识别 `.nomedia`，默认开启并跳过对应目录及子目录。Android 关闭后需先授权对应扫描文件夹，才能获取系统索引未收录的视频。
- “⋮ → 屏蔽文件夹”可隐藏目录及其子目录的视频，并随时恢复；也可长按文件夹标签进行屏蔽。屏蔽不删除或修改文件，刷新和重启后仍有效。
- Android 自适应图标、Windows EXE/窗口图标使用同一套薄荷绿播放丝带图案。

v0.4.1 修复美白从 0 到 1 的突变：强度现在连续控制整套美白效果，而不是只控制最后一层 LUT。升级时，仍等于旧推荐值的美白/冷暖/色调迁移到新默认，其他自定义值保留；也可点击“恢复推荐”。关闭“美颜”总开关即可观看原图。全部五官/美妆关闭时跳过检测；开启时处理检测器返回的第一张脸。详见 [v0.4.1 验证记录](docs/VALIDATION-v0.4.1.md)。

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

视频库右上角“文件”打开视频，“⋮”中添加目录、播放网址或调整设置。播放界面左下角“美颜”打开底部面板：点选磨皮、美白等项目，上方同一根调整条切换为对应参数；选项可左右滑动。瘦脸、大眼、口红和腮红仅在调高强度或打开其开关后生效，点选项目本身不会启用效果。按住“按住对比”临时查看原图，“完成”或返回键收起面板。

视频库上方可选择排序字段、升降序和单列/双列；“⋮ → 视频库设置”控制 `.nomedia`。Android 的“添加扫描文件夹”只申请所选目录的读取权限，不申请“所有文件访问”。系统限制授权的目录仍不可扫描。创建时间优先使用 Windows 文件创建时间或 Android 拍摄时间，缺失时使用入库/修改时间。详见 [v0.4 验证记录](docs/VALIDATION-v0.4.md)。

v0.3.1 的窗口、手机触控与打包检查见 [紧凑面板验证记录](docs/VALIDATION-v0.3.1.md)。Android 片尾停止后的调参预览仍有限制，播放中或中途暂停时正常。

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
