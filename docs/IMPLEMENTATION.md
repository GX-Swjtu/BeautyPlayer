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

`libraryOpen` 从播放切到列表时统一调用 `pauseForLibrary()`，结束临时倍速、暂停并保存续播；返回按钮和 Android 返回键都走此逻辑。视频库内禁用空格播放和左右快进快捷键，避免搜索时恢复后台播放。Windows DropArea 只接受存在的视频文件，目录/非视频不接收，拖入多个文件时播放第一个有效视频，以 CopyAction 接收而不移动源文件。非阻塞 HoverHandler 监听鼠标/触控板位置，底部移动显示控制栏；静止 2.5 秒后隐藏，拖动进度或打开设置/菜单时暂缓隐藏。

VideoLibrary 使用 QtConcurrent 工作线程读取目录元数据，不跟随目录符号链接，最多 10 万条。扫描与视频解码、缩略图生成分开；按文件名、创建时间或大小在内存模型中排序，支持自然数字排序及升降序。单列和双列共用 GridView，复用可见项。Windows 启动先读索引缓存再刷新；重复刷新取消旧扫描并重新扫描。未实现文件系统实时监听，文件变更后点击刷新。

Android 优先读取 MediaStore 索引，另通过 ACTION_OPEN_DOCUMENT_TREE 读取用户明确授权的目录。每个目录查询一次子项，先检查 `.nomedia` 再遍历子目录；与 MediaStore 按路径去重，授权目录内以当前扫描策略为准。扫描使用 CancellationSignal 取消。默认识别 `.nomedia`，关闭后可以扫描授权树中未被系统索引收录的视频；没有目录授权时无法绕过 Android 存储限制。Android 不保存额外媒体索引 JSON，避免撤销权限后继续显示旧文件名。

ThumbnailProvider 是独立的 QQuickAsyncImageProvider，串行后台队列只处理被列表请求的缩略图，取消的待处理项直接跳过。Windows 优先使用 IShellItemImageFactory，Android 使用 ContentResolver.loadThumbnail；系统无图或近乎纯黑/白时，分别通过 Qt 或 MediaMetadataRetriever 取约 10% 位置（最多 10 秒附近）的画面，避开直接抓首帧。系统代表帧不保证一定是视频中间帧，提取失败则显示图标占位，不影响点击播放。

ThumbnailCache 用 URL、文件大小、修改时间和格式版本生成 SHA256 键；以最长边 512 像素、JPEG 80 质量写入应用缓存，使用 QSaveFile 原子提交。重新启动复用磁盘结果；文件元数据变化自动换键。使用时更新缓存访问时间（每小时最多一次），启动、每小时及每生成 32 张后分批清理：30 天未访问、总量超过 256 MiB 或 4096 个条目时优先删除旧项。目标上限允许一个生成批次内的短暂超出。失败记录保留 30 分钟，避免反复解码坏文件。清理只删除本缓存目录内符合哈希命名规则的文件。

缓存、目录配置和播放历史只存本机。排序、布局及 `.nomedia` 偏好写入 QSettings。诊断/--fresh 模式不保存这些偏好；缩略图诊断可用 `--thumbnail-cache` 指向隔离目录。创建时间优先采用 Windows birthTime 或 Android 拍摄时间；不可获得时按平台回退入库时间或修改时间。

屏蔽目录单独保存到 `library/blockedFolders`，在搜索、排序和文件夹计数之前应用于内存索引，不删除媒体。匹配完整目录或带分隔符的子路径，避免 `private` 误伤 `private2`。Android 外部存储的 MediaStore 和 SAF 使用同一文件夹路径标识，授权方式切换不影响屏蔽。无法取得实际路径的其他文档提供器保留提供器/授权树内的逻辑路径；跨不同提供器别名不保证自动合并。

## 参数映射

界面常规强度 0–100 对应内部 0–1。磨皮/美白直接传值；瘦脸映射至 0–0.08，大眼 0–0.15，口红/腮红 0–0.5。冷暖/色调为 -100–100；冷暖使用 GPUPixel 白平衡的分段 Kelvin 映射，色调映射为 -100–100。单独设置对比原图不更改用户保存的参数。

v0.4.1 美白修复：两版上游 `beauty_face_unit_filter.cc` 在 `whiten > 0` 时完整运行灰阶、基础、肤色 LUT，最后才 `mix(color, color_custom, whiten)`，造成 0 与极小正数不连续。改为 `mix(colorEPM, color_custom, clamp(whiten, 0, 1))`，其中 colorEPM 是已磨皮、未美白的颜色；强度现在控制完整美白链，强度 1 保持原上限。不增加绘制或 CPU/GPU 拷贝。

Windows 的 ContinuousBeautyFilter 复用 SDK 滤镜组、LUT 纹理和参数接口，仅替换末端着色器并释放旧程序；着色器源和上游署名保存在 ContinuousBeautyShader.h。Android 在 CMake 中验证固定源码的两处匹配，生成构建目录副本并编译，下载的第三方源码不改动。两端以磨皮 0 / 0.7、七个美白强度进行 GPU 像素连续性检查，不能只用“滑块值变了”证明有效。

## 发布

`scripts/build.cmd` 初始化指定 VS 开发环境并校验 SDK；`scripts/package.ps1` 部署 Qt/FFmpeg、GPU DLL、模型、图像资源、MSVC CRT DLL 和许可文本。程序不需要设置全局 PATH。测试素材单独下载并保留来源，不分发到便携包。
