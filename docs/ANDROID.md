# Android 移植边界

v0.2.1 仍只有 Windows 可构建和已验证；CMake 在其他平台明确报错，避免把未完成适配的工程当作可用 APK。手机界面（390×844 窄屏）、长按、滑动、续播使用共用 QML/C++，已在 Windows 预览及事件注入检查；没有 Android 真机结论。此次 libyuv 的 AVX2/SSE 分发针对 Windows x64；未来 Android ARM 应使用其 NEON 等内核并重新验证，不能直接套用这些性能数据。

可复用部分：QML 播放器、QMediaPlayer/QVideoSink 帧桥接、有界队列、参数设置，以及 GPUPixelPipeline 的 `QImage RGBA → QImage RGBA` 接口。GPUPixel 不接收 Qt 的 Direct3D 纹理，界面层不依赖 Win32。原始分辨率策略同样可复用。

Android 后续需要单独完成：

1. 用 Android arm64-v8a SDK 替换 Windows DLL/导入库；匹配 Qt 6.11 的 Android SDK、NDK、JDK 版本。
2. 补齐 GPUPixel Java/JNI 初始化和模型/美妆资源复制。beta3 的 Android `Util::getResourcePath` 通过 Java 查询目录，与 Windows `setResourceRoot` 不同。
3. 处理 EGL 线程初始化、后台/前台恢复和文件选择器 content:// 的持久读取权限。窄屏美颜面板已经改为底部抽屉，但安全区域、系统返回键、设备旋转与真机触摸仍需验证。
4. 检查原生库的 16 KB 页兼容性。Windows 为人脸检测锁定 beta3，不意味着 Android 也必须沿用该二进制；新 Android SDK API 差异应隔离在 GPUPixelPipeline 内。
5. 真机验证发热、分辨率/帧率、音画同步和生命周期，再交付 APK。

不应把 Windows 跑通解释为 Android 已可用，也不应混用不同发行版的 GPUPixel 头文件和二进制。

## 手机快速扫描

Windows 已提供异步目录视频库；手机端不直接复用 Windows 的目录递归扫描。Android 后续应查询系统 **MediaStore.Video** 索引，读取 `_ID`、`DISPLAY_NAME`、`BUCKET_ID`/目录名称、时长、大小，使用 `content://` URI 播放；按 bucket 分组，避免遍历整个手机和逐文件解析视频。这是 Android 官方提供的媒体索引，不需要额外第三方扫描 SDK。

需要一并完成 Android 13 的 READ_MEDIA_VIDEO、Android 14+ 部分媒体授权、拒绝权限/重新授权后的刷新、外部卷和 URI 失效处理，再测首扫/缓存/万条索引的实际耗时。本次按允许省略复杂文件管理功能的范围，**未把这一手机适配声称为已实现**，也不承诺达到 MX Player 的速度。

参考：[Android 官方共享媒体访问](https://developer.android.com/training/data-storage/shared/media)。
