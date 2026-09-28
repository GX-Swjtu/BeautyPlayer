# v0.2.1 SIMD 优化验证

2026-09-28，Windows 11 x64，Intel Core i9-13900HX（24 核 / 32 线程）、RTX 4070 Laptop GPU、Qt 6.11.2 / MSVC 2022。libyuv 单独使用本机 Android NDK 25.1.8937393 中的 clang-cl 14.0.6 编译为 Windows x64 静态库。没有增加应用运行时依赖。

## 实现与测量范围

默认使用 libyuv 的 CPU/OS 检测，最多开启 AVX2，支持 SSSE3/SSE2 和 C 内核回退。没有给整个播放器设置 `/arch:AVX2`。固定源码、归档 SHA256、BSD 许可和 PATENTS 已纳入交付；下载镜像内容与同提交的官方 Git 仓库比对一致。

优化发生在解码后的 YUV → RGBA 阶段，保留硬件解码、原分辨率和双线性色度插值。`LIBYUV_UNLIMITED_DATA` 启用更准确的蓝色系数。未知色彩元数据、非 SDR transfer、特殊 surface 变换等继续走 Qt。美颜仍由 GPUPixel/OpenGL 处理，硬件帧映射、上传和回读成本仍存在。

## 4K 实际播放

同版本通过 `--converter qt` 与 `--converter auto` 切换，分别运行 60 秒。素材为同一 2160×3840、23.976 fps 人脸视频。统计来自 `artifacts/simd/4k-*.json`；FPS 使用第 5 秒以后处于播放状态的采样均值。转换耗时包含帧映射与输出图像准备。

| 美颜配置 | 转换方式 | 转换 ms/帧 | GPU 阶段 ms/帧 | 两阶段合计 ms/帧 | 呈现 FPS |
|---|---|---:|---:|---:|---:|
| 默认美颜 | Qt 原路径 | 19.99 | 15.97 | 35.96 | 23.80 |
| 默认美颜 | AVX2 | 8.00 | 16.15 | 24.15 | 23.82 |
| 全部特效 | Qt 原路径 | 22.81 | 30.90 | 53.70 | 23.80 |
| 全部特效 | AVX2 | 7.98 | 29.48 | 37.46 | 23.76 |

默认美颜转换耗时降低约 **60%**，全部特效约 **65%**；两阶段合计分别降低约 **33% / 30%**。两阶段可以重叠，因此合计不是流水线的帧间隔。源视频只有约 24 fps，不能把耗时下降解读成实际播放 FPS 成倍增加。

GPU 阶段计时还包含人脸检测（若启用）、上传和同步回读，不是单独的 GPU 着色器执行时间。单机样本受设备功耗、温度和桌面负载影响，不代表其他机器的固定提速比例。

AVX2 两轮分别命中 1421 / 1417 次 libyuv 转换，Qt 回退均为 0。Qt 对照均无 libyuv 转换；不是仅依据 CPU 型号或状态文字判定加速成功。4K 原始像素尺寸保留。

1080×1920 / 30 fps 人脸视频各运行 60 秒：Qt → AVX2 的转换耗时从 **9.37 → 3.97 ms**，两阶段合计 **17.98 → 14.63 ms**；采样 FPS 为 **29.89 → 29.91**。AVX2 命中 1783 次、Qt 回退 0。

## 转换内核单独测试

固定 CPU 内存中的 NV12 帧，每种模式预热 5 次、测量 60 次；包括 UV 拆分、双线性色度插值、RGBA 输出，不包括视频解码、硬件帧映射、GPUPixel 和显示。结果不能直接当成整机播放器 FPS。

| 尺寸 | C 内核 ms/帧 | SSSE3 ms/帧 | AVX2 ms/帧 |
|---|---:|---:|---:|
| 1920×1080 | 6.05 | 0.77 | 0.54 |
| 2160×3840 | 24.07 | 3.92 | 2.97 |

AVX2 比 SSSE3 在这组测试中再减少约 24–29% 转换耗时。静态库符号检查确认包含 I444ToARGBRow 和 ScaleRowUp2_Bilinear 的 SSSE3、AVX2 内核。

## 画质与功能检查

- 3 组 CTest 全部通过：GPUPixel 效果、帧转换、视频库/历史。
- 帧转换测试共 591 项检查：BT.601/709、全/限范围、NV12/NV21/YUV420P/YV12、非连续行跨度、奇数尺寸、旋转镜像、回退和缓冲区持有。C / SSE / AVX2 输出最大通道差 **0**。
- 独立浮点色彩矩阵参照检查通过。对 Qt 路径不要求逐字节相同：其浮点着色器与 libyuv 定点计算存在舍入差异。
- 同一解码帧（PTS 0）比较：4K 对 Qt 的 RGB 平均绝对误差 **0.163/255**、最大 **2/255**、PSNR **55.98 dB**；1080p 为 **0.186/255**、最大 **2/255**、PSNR **55.40 dB**。已查看同帧 PNG，未见明显偏色或细节损失。
- 带旋转信息的视频保留 240×320 方向；其格式走 Qt 回退，前后像素一致。YUV 快路径的 frame rotation/mirror 组合由测试覆盖。
- 打包版可见窗口 32 秒交互检查通过，包含暂停调参、全零、原图、跳转、换片、全屏、续播、长按和滑动手势。

同帧 JSON/PNG 位于本地 `artifacts/simd/same-frame-*`，测试视频和个人目录不进入 Git。

打包版 180 秒全部特效测试包含暂停、参数修改、原图切换、反复跳转和换片，8 项后台检查通过，共呈现 **5170 帧**。60 秒后播放段平均 **29.77 fps**，Private Bytes 在 **886.8–903.0 MiB** 之间，末值 **889.7 MiB**，未见持续增长或崩溃。可见窗口的 15 项手势/界面检查另由 32 秒用例完成。此次没有重跑上一版的完整 10 分钟用例，不能把 3 分钟结果称为长时间无泄漏保证。

音画同步各测试 60 秒：原合成片缺少色彩标记，1783 次转换全部由 Qt 完成，归一化音视频 PTS 差为 **16–56 ms**；另用补充了 BT.709 / limited-range 标记的副本，**1783 次全部命中 AVX2、Qt 回退 0**，平均 **30.00 fps**，PTS 差 **16–65.33 ms**，未见累计漂移。视频与音频无需重编码。该指标比较 QAudioBufferOutput 与呈现帧时间戳，不包含显示器和声卡实际输出延迟。详细 Qt 日志确认使用 **D3D11VA 硬件解码**。

便携目录：`dist/windows-x64`，直接启动并完成以上交互/播放验证，无需 Qt Creator。`BeautyPlayer.exe` SHA256：`FAB32D40374351D33FFF733CB008E39856070DB6669A1993AF14F0058677DCC8`。libyuv 静态链接，许可文件随便携目录分发；本次 Git 推送只包含源码、脚本、许可和文档。

## 复测

```powershell
.\scripts\build.cmd
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/package.ps1
ctest --test-dir build/windows-release --output-on-failure
# 下一条需要 Qt bin 在 PATH 中
.\build\windows-release\FrameConversionTests.exe --benchmark
.\build\windows-release\ConversionMediaProbe.exe test-media/face-1080.mp4 artifacts/simd/same-frame-face-1080
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/benchmark-simd.ps1
```

`--converter sse` / `scalar` 通过收窄真实 CPU 特性测试回退；未使用真实旧 CPU。Android ARM、其他显卡和 HDR 未验证，不据此推断其效果或性能。
