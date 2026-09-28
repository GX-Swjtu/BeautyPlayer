# Android ARM64 版（v0.3.0）

已提供 APK 构建、安装和真机诊断脚本。最低 Android 9 / API 28，目标 API 36，CPU 为 **arm64-v8a**，GPU 需 OpenGL ES 3。适用于骁龙 8 Elite（8E）及满足条件的其他 ARM64 手机。没有限定高通机型或强制新指令；libyuv 按 CPU 实际能力分发。

**已在 24129PN74C / SM8750（骁龙 8 Elite）、Android 16 真机安装运行。** 日志确认高通硬解与 Adreno 830 GPU 美颜；591 项转换检查和播放交互检查通过。设备使用 4 KB 内存页；16 KB 目前只完成静态 ELF/APK 对齐检查。未测其他手机，也没有功耗仪实测。见 [本轮测试记录](ANDROID-VALIDATION.md)。

## 安装与操作

本地安装包：dist/android-arm64/BeautyPlayer-arm64-v8a.apk。复制到手机安装，或开启 USB 调试、允许电脑调试后运行：

~~~powershell
.\scripts\test-android.ps1
~~~

这是 Release C++ 编译、使用本机 debug 密钥签名的测试包；不是商店发布版本。密钥在项目忽略目录，不提交 Git。换电脑生成的签名可能不同，不能保证覆盖安装。

- 首次启动请求视频访问，查询 MediaStore.Video 索引，按文件夹分组。Android 14+ 部分授权时只显示获授权的视频。
- 拒绝扫描权限仍可点“文件”，通过系统选择器打开。支持时保留 URI 读取授权；文件已删除、授权撤销或临时 URI 可能无法续播。
- MediaStore 和系统选择器的 `content://` 地址由 ContentResolver 检查权限并打开只读文件描述符，再交给 QMediaPlayer；播放器保留原 URI 用于标题与续播，换片时关闭旧描述符。
- 默认磨皮 70、美白 32、冷暖 −4、色调 +2.5；瘦脸、大眼、口红、腮红默认关闭。冷暖和色调作用于全画面。
- 长按默认 3 倍速，松开恢复；左右滑动定位、松开跳转；暂停调参、原图比较和播放记忆共用桌面逻辑。
- 播放时屏幕常亮，后台暂停，返回前台由用户点击继续。Qt ApplicationWindow 管理安全区域，系统返回键优先关闭面板或返回视频库。

## 加速实现与边界

1. **解码：Qt FFmpeg + Android MediaCodec。** 默认优先 MediaCodec，不硬编码高通 codec 名称，不强行接受不支持的配置。Qt/FFmpeg 保留软件回退；MediaCodec 也可能选软件组件，必须从真机日志确认实际解码器。
2. **转换：GPU 纹理路径和 NEON 路径分别处理。** MediaCodec Surface 输出通过转换线程自己的 Qt QRhi/OpenGL ES 上下文取帧。有明确色彩元数据的 NV12/NV21/I420/YV12 CPU 帧用 libyuv NEON 转 RGBA；人脸副本缩图也用 NEON。不能把“已编译 NEON”解释为每个硬解纹理帧都执行 NEON。
3. **美颜：GPUPixel v1.3.1 + OpenGL ES 3。** 头文件、源码、Mars 库和模型来自同一提交；Windows 保持 beta3。GL 在唯一的 BeautyGL 线程执行。输入纹理复用，glTexSubImage2D 上传，省掉上游 RawDataSource 一次中间绘制。
4. **流水线：** 转换、美颜、显示分工，队列有界，只保留最新候选帧。跳转、换片、调参的旧结果丢弃；输出缓冲池保留独立寿命。人脸检测在主动开启五官/美妆时才创建，副本最长边 640；单帧检测模式避免跨跳转继承跟踪状态。
5. **保留原始尺寸。** 首版范围是普通 8 位 SDR。4K 不主动降尺寸，帧率由设备、编码、温控和功能开关决定。

当前仍有“硬解纹理 → RGBA 回读 → GPUPixel 上传 → 美颜结果回读 → Qt 显示”的拷贝，**不是全程 GPU 零拷贝**。这是后续真机性能分析的重点。没有使用高通 NPU、私有 DSP SDK 或付费美颜服务。

Qt 6.11 需要为硬解帧显式注册转换线程的 QRhi。原图比较也保持同一取帧线程，只绕过美颜，避免 SurfaceTexture 被显示线程争用。少量 QRhi 接口来自版本匹配的 GuiPrivate；升级 Qt 需回归这部分。

Android 启用 Qt 的 `QT_ANDROID_NO_EXIT_CALL`，保留对象、线程和 Activity 的正常清理，将进程回收交给系统，避免进程级 `exit()` 与 Android HWUI 线程竞争。升级 Qt 时同样需要复测正常返回、系统返回键和重新打开。

## Windows 主机重建

安装 Qt **6.11.2 Android arm64-v8a**、同版本 Windows host kit、CMake 和 Ninja。已有 SDK 接受过的许可证可复用：

~~~powershell
.\scripts\setup-android.ps1 -ExistingSdk D:\Android\Sdk
.\scripts\build-android.ps1 -QtRoot D:\Qt\6.11.2\android_arm64_v8a -QtHost D:\Qt\6.11.2\msvc2022_64
~~~

脚本校验固定的 JDK 21、Android platform 36、build-tools 36、NDK r27c、GPUPixel、libyuv，版本和 SHA256 在 android-dependencies.lock.json。SDK、JDK、Gradle 缓存与临时目录都在项目内，不修改机器级环境。Java 代理读取当前 HTTPS_PROXY，保留 TLS 验证。

每次打包将旧目录保存在 artifacts/android/package-backups，防止遗留 Qt 插件进入新包。原生库按 16 KB 链接，APK 使用未压缩、对齐的库。Qt 调整过的 libavformat.so 被 r27 二次 strip 后出现段偏移错误，因此 Gradle 对该库保留原文件，并检查 APK 内的实际 ELF。

## 真机验收

~~~powershell
# 颜色、行跨度、矩阵、缓存寿命及 NEON/GL/模型冒烟检查
.\scripts\test-android.ps1 -SelfTest
# 播放、暂停调参和共用手势检查
.\scripts\test-android.ps1 -Video .\test-media\face-1080.mp4 -Profile all -Exercise
# 默认美颜播放 10 分钟
.\scripts\test-android.ps1 -Video .\test-media\face-1080.mp4 -Seconds 600
# 软件解码对照
.\scripts\test-android.ps1 -Video .\test-media\face-1080.mp4 -SoftwareDecode
~~~

报告在 artifacts/android/device-日期时间。测试素材和报告使用应用私有目录，通过 debug 包的 `run-as` 读取，不要求扫描用户媒体权限；不会清空系统 logcat。检查实际 codec、renderer 是否 Adreno、converter、yuvConversions/qtConversions、textureBackedInputFrames、处理耗时、丢帧及音画 PTS 差。纹理句柄计数不能独立证明硬解。自动手势检查使用事件注入，真实手指操作仍需验证。`-SelfTest` 还包含标量/NEON 微基准，完成后可用普通启动命令恢复播放器。

还需覆盖：横竖屏、旋转、锁屏、来电、前后台、部分授权及撤销、不同编码、1080p30/4K、温控和内存趋势。自测成功不等于这些项目全部通过。

## 许可与参考

自有适配代码 MIT；依赖保留原许可，见 THIRD_PARTY_NOTICES.md。APK 动态链接 Qt，可以重建、替换兼容 Qt 库并重新签名。Mars 是上游预编译组件；仓库仅提交源码、许可和下载脚本，不上传其二进制、模型或 APK。

- [Qt Android 工具链](https://doc.qt.io/qt-6/android-configure-dev-environment.html)
- [Qt FFmpeg 硬件加速配置](https://doc.qt.io/qt-6/advanced-ffmpeg-configuration.html)
- [GPUPixel v1.3.1](https://github.com/pixpark/gpupixel/releases/tag/v1.3.1)
- [Android 共享媒体访问](https://developer.android.com/training/data-storage/shared/media)
- [Android 16 KB 内存页](https://developer.android.com/guide/practices/page-sizes)
