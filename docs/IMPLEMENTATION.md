# 接入说明

播放器使用 Qt 6.11.2 / MSVC x64 / C++17。GPUPixel 官方 v1.3.0-beta3 的 DLL、模型和资源保持原样，版本与 SHA256 见根目录 `dependencies.lock.json`。

## 帧与线程

QMediaPlayer 输出到独立输入 QVideoSink。v0.2 将帧转换移至 VideoConversion 线程，BeautyGL 线程持有完整的 GPUPixel 生命周期，完成检测、GPU 滤镜与当前帧回读，再通过 queued invocation 将 QImage 交给输出 QVideoSink。音频仍由 Qt 播放。转换下一帧时，GPU 可以处理上一帧。

队列容量固定：一个转换中帧、一个已转换槽、一个 GPU 处理中帧、一个最新待处理输入；另保留最后源帧用于暂停重绘及当前显示图像。没有按输入帧无限投递任务。跳转/换源清空未启动槽位，正在执行的工作完成后按版本丢弃。

跳转和更换文件增加 epoch，参数修改增加 revision；旧作业结果不能覆盖新状态。暂停后保留最后一个源帧以便重绘。处理过慢时替换待处理帧，不增加队列长度，也不自动降低分辨率。硬件解码帧到 RGBA、OpenGL 上传和读回都有成本，首版不是零拷贝管线。

Qt 的 `QVideoFrame::toImage()` 已处理 surface transformation，但未处理 frame presentation rotation/mirroring。FrameConversion 依次补充 frame rotation、horizontal mirror，然后以无额外变换的 QVideoFrame 输出。

v0.2.1 在 FrameConversion 增加 libyuv 分支：映射已解码的 8 位 NV12/NV21/YUV420P/YV12，校验每个平面的长度和 stride，按 BT.601/709 及 full/video range 转 RGBA。NV12/NV21 先 SplitUVPlane，再 I420ToARGBMatrixFilter，保留双线性色度插值。使用 Yvu 矩阵并交换 UV，适配 little-endian 的 RGBA 字节顺序。`LIBYUV_UNLIMITED_DATA` 保留更精确的蓝色系数。元数据未知、其他 transfer、crop、surface rotation/mirror 或上下颠倒时回退 Qt，然后统一补充 frame presentation 变换。

libyuv 用 clang-cl 单独编译，保留上游手写 SSE/AVX2 内核；Qt 应用仍用 MSVC。启动时 MaskCpuFlags 与真实 CPU/OS 能力取交集，默认最多允许 AVX2，不对整个应用使用 /arch:AVX2。该全局配置在工作线程启动前固定，不能在播放中修改。转换线程有独立 UV 临时区和三个 RGBA 槽，只有 isDetached 的槽可复用；全部仍被引用时分配独立图像。桥接层记录实际 libyuv/Qt 转换次数，避免把 CPU 支持 AVX2 误当成每一帧都命中加速。

## 旧版库兼容点

1. `SourceRawDataInput::uploadBytes` 的 stride 参数实际是像素数；使用连续 RGBA 并传 width。
2. 默认 TargetRawDataOutput 交替映射两个 PBO，返回的是前一帧，时间戳却为当前帧。CurrentFrameOutput 同步读取当前 FBO，保证暂停、逐帧和时间戳一致。
3. GPUPixel 的 WhiteBalanceFilter 着色器含桌面 GLSL 不兼容的精度限定符。在本机 Intel 上能编译，NVIDIA 上失败；旧版 GLProgram 又会把失败的 `return -1` 当作 true，且 Windows 日志不输出。DesktopWhiteBalanceFilter 复用原有算法和参数函数，只移除 shader 的 lowp/mediump/highp，并显式检查 shader link status。修改源码和原始版权说明保存在 WhiteBalanceShader.h。
4. SourceRawDataInput 自己的 shader 成员遮蔽了基类成员，基类 getProgram() 返回空；该路径通过奇数尺寸、颜色、方向和当前帧一致性回归测试验证。
5. GPUPixel 采用 Windows 延迟加载；先通过 QLibrary 检查 DLL 及其依赖。DLL/资源缺失时给出错误并保留原图播放，避免加载器在应用启动前直接终止。
6. 模型加载器使用 Windows 窄字符文件 API。资源路径使用 QFile::encodeName 转成本地文件名编码，并在进入模型加载器前验证可读性，修复中文安装目录下的崩溃。无法由当前系统编码表示的路径会明确报错并回退原图。

所有滤镜归零、关闭美颜或原图对比时，直接把原始 QVideoFrame 交给显示端，保留旋转/镜像/时间戳，不做 CPU 转换与 GPU 往返。诊断工具主动调用 lastImage 时才生成原图 CPU 副本。非零节点才加入滤镜链；首次启用五官/美妆时才创建检测器。需要检测时，检测最长边 640 像素的副本，归一化关键点用于原分辨率画面；未发现人脸则当帧清除关键点。

CurrentFrameOutput 有三个可复用图像槽，只复用已经没有 Qt/调用方引用的内存。使用 QImage::isDetached 检查共享状态，仍被显示线程持有的图像不会被写入。保留同步读当前 FBO，避免把旧 SDK 的上一帧回读结果错误配上新时间戳。尚未实现解码 D3D11 纹理与 GPUPixel OpenGL 的跨 API 共享。

Qt FFmpeg 保留默认自动硬件解码选择，不硬编码私有环境变量。在本机日志中已确认 d3d11va；无硬件解码的设备仍可由 Qt 回退软件解码。开启日志的方法来自 Qt 官方 [FFmpeg 配置说明](https://doc.qt.io/qt-6/advanced-ffmpeg-configuration.html)。QVideoFrame 的 NoHandle 不足以判断实际是否软件解码，因此报告中的 textureBackedInputFrames 只统计句柄类型，不作为硬件解码结论。

## 续播、手势与媒体库

ResumeStore 用原子文件写入保存最近 200 个位置，使用 URL 的 SHA256 作为键，不保存 URL 原文。本地文件同时校验大小和修改时间，避免替换视频后套用旧位置。记忆在媒体时长/seekable 就绪后恢复，加载过程不能覆盖旧书签；退出停止播放器之前保存。接近结尾（5 秒与总时长 5% 的较小值）清除续播点。

PlaybackSession 保存正常倍速与临时长按状态。松手、取消触摸、暂停、换源或应用失去活动状态均恢复正常倍速。水平滑动超过 18 像素且大于垂直位移 1.2 倍后开始预览；一屏映射最多 120 秒，松手才提交一次 seek。视频层手势不覆盖上层按钮或抽屉。

VideoLibrary 使用 QtConcurrent 工作线程枚举本地目录，不跟随目录符号链接、不解码视频、不请求缩略图。按扩展名过滤和 URL 去重，最多 10 万条；按文件修改时间排序。用户过滤作用于内存模型，列表复用行组件。启动先读缓存再刷新；重复刷新取消旧扫描并重新扫描。缓存、目录配置和播放历史只在本机存储，诊断/--fresh 模式不读写用户状态。未实现文件系统实时监听或 Android MediaStore 适配。

## 参数映射

界面常规强度 0–100 对应内部 0–1。磨皮/美白直接传值；瘦脸映射至 0–0.08，大眼 0–0.15，口红/腮红 0–0.5。冷暖/色调为 -100–100；冷暖使用 GPUPixel 白平衡的分段 Kelvin 映射，色调映射为 -100–100。单独设置对比原图不更改用户保存的参数。

## 发布

`scripts/build.cmd` 初始化指定 VS 开发环境并校验 SDK；`scripts/package.ps1` 部署 Qt/FFmpeg、GPU DLL、模型、图像资源、MSVC CRT DLL 和许可文本。程序不需要设置全局 PATH。测试素材单独下载并保留来源，不分发到便携包。
