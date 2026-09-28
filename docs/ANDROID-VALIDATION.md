# Android v0.3.0 真机验证

日期：2026-09-28。设备：24129PN74C / SM8750（骁龙 8 Elite），Android 16，Adreno 830，arm64-v8a，4 KB 页。USB 调试安装本地 debug 签名、Release C++ 编译的测试 APK。结果仅代表这台设备和以下素材。

## 实际加速链路

- 日志实际出现 `c2.qti.avc.decoder`；GPUPixel 报告 `Adreno (TM) 830`。硬解 H.264 输入为 `SamplerExternalOES`，并非仅根据设置推断使用硬件。
- 硬解纹理经 Qt 转换线程自己的 QRhi/GLES 取帧，GPU 美颜在独立线程执行。此路径 `qtConversions > 0`、`yuvConversions = 0`，没有把 NEON 的存在误记为每个硬解帧都经过 NEON。
- ARM64 libyuv 内核与运行时检测已执行；五官美妆的检测副本使用 libyuv 缩图。没有调用高通 NPU 或 DSP。
- 保留原分辨率；仍有 CPU/GPU 上传回读，未实现全程零拷贝。转换和美颜阶段可以重叠，所以两阶段耗时之和不等于流水线的帧间隔。

## 已通过

| 项目 | 结果与证据 |
|---|---|
| APK 安装和启动 | 真机安装成功，原生库加载成功；签名、依赖完整性和 86 个 ARM64 ELF 的 16 KB LOAD 对齐检查通过 |
| NEON、颜色与帧寿命 | 591 项转换检查通过；NEON 与标量最大像素通道差 0；覆盖 BT.601/709、全/限范围、奇数尺寸、行跨度、旋转、镜像、缓冲持有 |
| GLES 模型及输出 | 8 项手机自测通过：资源、shader、RGBA 方向/尺寸、当前帧同步输出、旧图像不被覆盖、默认美颜确实改变像素、无脸时清除关键点 |
| 全部美颜与交互 | 14/14 检查通过；全特效测试中 339 帧检测到人脸。暂停调参、原图比较、归零、全屏、QML 滑块、长按 3×、松开恢复、滑动快退、续播均通过 |
| 原尺寸竖屏 1080p30 | 默认美颜测试末段 30 fps，1080×1920；20 秒短测平均转换 13.87 ms、美颜 12.69 ms |
| 原尺寸竖屏 4K | 2160×3840、23.976 fps 素材，稳定片段约 24 fps；25 秒短测平均转换 35.24 ms、美颜 33.07 ms，未降分辨率 |
| 横屏音画同步片 | 1920×1080、30 fps；25 秒测试约 30 fps，平均处理 24.93 ms；稳定采样音视频 PTS 差约 24–63 ms，没有持续扩大 |
| 旋转标记与色块 | 320×240 带旋转信息的文件输出为 240×320；人工核对红绿蓝黄色块位置及方向正确 |
| Windows 回归 | 三个 CTest 套件通过；Windows 使用自己的 beta3 GPUPixel，未混用 Android 库 |
| 10 分钟默认美颜 | 播放阶段 600 秒、14/14 检查通过；循环并每 45 秒跳转，稳定阶段平均 29.74 fps，无持续内存增长。结束后的退出问题见下文 |
| 软件解码回退 | 禁用硬解后，YUV420P 输入实际命中 432 次 NEON 转换、0 个纹理帧；约 30 fps，未出现美颜回退 |
| 全特效持续播放 | 独立 20 秒测试，稳定阶段约 16.60 fps，323 帧有人脸；默认关闭五官美妆可以明显降低负担 |
| 视频库与直接点播 | 系统授权后列出 31 个视频，索引查询 1–5 ms；从目录点播公开测试片，完成播放。此小样本不代表海量媒体扫描性能 |
| MediaStore URI 回归 | 从 `content://` 播放的 35 秒交互测试 14/14，通过跳转、换源、同帧对比和续播；退出阶段无 native/Java crash |
| 前后台与系统返回 | 播放后回到桌面，返回应用保持暂停；间隔 3 秒的两次界面采样位置均为 00:01。系统返回键结束 Activity，日志正常返回 0，重新打开成功 |

音画 PTS 是软件时间戳指标，不是外部仪器测量的扬声器与屏幕延迟。早期短测在第 3 秒保存诊断窗口 PNG 会阻塞界面，统计包含这次停顿；当前诊断改为后台编码截图。4K 结论不外推到 4K60 或所有编解码器。

## NEON 微基准

同一手机、同一个可执行文件，NV12 → RGBA，BT.709 限范围、双线性色度插值，预热 5 次后平均 60 次：

| 尺寸 | 标量 | NEON | 转换阶段加速 |
|---|---:|---:|---:|
| 1920×1080 | 19.1269 ms | 1.0749 ms | 约 17.8× |
| 2160×3840 | 52.5971 ms | 4.3197 ms | 约 12.2× |

这是 CPU 转换微基准，不是整个播放器的加速比。Qt 的 GLES RGBA 回读对个别通道有 1/255 舍入差异，逐像素旋转/镜像检查使用该容差；NEON 与标量比较没有这类误差。

## 长测与待验证项

10 分钟测试实际展示 17,656 帧，接收 17,865 帧；待处理帧丢弃 152，跳转等版本切换丢弃旧结果 63。平均转换 14.01 ms、美颜 12.63 ms，合计 26.64 ms。第 60–600 秒平均 29.74 fps，541 个稳定期采样均处于播放状态。

进程 RSS 稳定期范围 432.68–471.96 MiB；第 60–120 秒平均 445.33 MiB、末分钟平均 447.86 MiB，末帧 447.32 MiB，观察到波动但没有持续增长。RSS 不等于完整的 GPU 显存或系统内存占用。播放阶段未见崩溃；旧帧淘汰计数不等于自动证明完全不存在闪帧。

长测完成后检查完整 logcat，发现 `main() returned 0` 之后 HWUI 工作线程触发已销毁 mutex 的 SIGABRT。此前 JSON 功能检查不能覆盖这一退出阶段错误。Qt 6.11.2 的 Android 入口在 Activity 清理后调用全进程 `exit()`，会析构 Android 绘制线程还在使用的全局对象。

Android 入口改用 Qt 的 `QT_ANDROID_NO_EXIT_CALL=1`：播放器、工作线程和 EGL 正常析构，Activity 正常结束，进程交由 Android 回收，不从 Qt 线程调用全进程退出。测试脚本现在等待退出阶段并检查 native/Java crash，不能仅凭 `report.json` 的 passed 判定成功。依据：[Qt 6.11.2 Android 入口源码](https://github.com/qt/qtbase/blob/v6.11.2/src/plugins/platforms/android/androidjnimain.cpp)。退出修复的复测结果见本地记录。

最终 APK：91,969,311 字节，SHA256 `e04def49ce7f6430eef80d307e0efc8395f4285793aab7100c558e534b42af8c`。长测后修改了退出方式、MediaStore 输入和异步加载的续播时机，GPU/转换流水线未变；上述修改通过最终包的 35 秒真机交互及退出检查，不将长测描述为最终 APK 的完整重跑。

尚未验收：16 KB 页真机、其他 ARM64 手机、Android 9–15、HDR、4K60、HEVC/AV1 全编码组合、来电和锁屏恢复、权限撤销/部分授权全流程、真实触指体验、功耗仪测试。扫描只在上述 31 个视频规模验证；测试日志与截图不上传仓库。

## 本地原始记录

仓库忽略测试素材、手机日志和图像，只提交本摘要。可用 `scripts/test-android.ps1` 重跑。

- 自测和微基准：`artifacts/android/device-20260928-203609/report.json`
- 全部美颜及 14 项交互：`artifacts/android/device-20260928-203433/report.json`，同目录有同帧暂停调参/原图 PNG。
- 1080p 默认短测：`artifacts/android/device-20260928-203246/report.json`
- 4K：`artifacts/android/device-20260928-203643/report.json`
- 横屏音画：`artifacts/android/device-20260928-203719/report.json`
- 旋转：`artifacts/android/device-20260928-203754/report.json`
- Windows：`artifacts/android/windows-tests-final.log`
- Windows 最终播放回归：`artifacts/android/windows-final.json`，14/14。
- 10 分钟：`artifacts/android/device-20260928-203959/report.json`，汇总 `artifacts/android/long-summary.json`。
- 加入 AndroidX 许可后的自测：`artifacts/android/device-20260928-205011/report.json`。
- 软件解码：`artifacts/android/device-20260928-205025/report.json`。
- 全特效持续播放：`artifacts/android/device-20260928-205048/report.json`。
- 退出修复：`artifacts/android/device-20260928-205353/report.json`，14/14；脚本额外等待 Activity 清理并确认日志没有 native/Java crash。
- 最终 MediaStore/续播/退出：`artifacts/android/device-20260928-210416/report.json`，14/14。
- 最终 APK 自测：`artifacts/android/device-20260928-210516/report.json`，8/8，内含 591 项转换检查及微基准。
- 前后台、系统返回及重新打开：`artifacts/android/resume-background-1.xml`、`resume-background-2.xml`、`lifecycle-final-logcat.txt`。UI 文件只留在本机。

首轮真机发现并修复了 GPUPixel 美白 LUT 资源遗漏；初始化现在验证所有配套资源。测试脚本改用应用私有目录解决 Android 存储权限问题。未把资源缺失时的原图回退计为美颜通过。

实际视频库点播还发现：Qt 6.11 的 DocumentFile 仅检查显式 URI 授权，不能代表 MediaStore 运行时视频权限。当前使用 Android ContentResolver 打开授权的只读描述符，并通过 `QMediaPlayer::setSourceDevice` 播放；保留原 URI 作为标题和续播标识，不绕过系统权限检查，不复制整段视频到缓存。

首次 URI 探针还检出重新加载文件时续播丢失：旧的 duration/seekable 属性在新媒体加载期间仍可见。续播现在等待 LoadedMedia/BufferedMedia 才跳转，复测从约 3.17 秒正确恢复；Windows 14 项播放回归也通过。
