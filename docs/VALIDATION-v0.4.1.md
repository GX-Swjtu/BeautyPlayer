# v0.4.1 播放交互与美白修复验证

日期：2026-09-28 至 2026-09-29。基于 v0.4.0（`1d3c5ac`）。Windows x64 与 Android ARM64 均已构建、打包和运行；安卓测试机为 24129PN74C / SM8750（骁龙 8 Elite）、Android 16、Adreno 830。

## 完成项

1. “⋮ → 屏蔽文件夹”可屏蔽目录及其子目录，立即从全部视频、搜索结果和文件夹标签中排除；可恢复，刷新/重启后仍保存。长按文件夹标签也可屏蔽。仅过滤视频库，不删除文件，系统选择器仍可手动打开文件。
2. 从播放画面返回视频库时结束临时倍速、暂停并保存续播点。返回播放画面不会自动继续；视频库内禁用播放/快进快捷键，搜索时按空格不会恢复后台播放。
3. Windows 接收拖入的本地视频文件并播放。拖入多个项目时选第一个有效视频，拒绝目录和非视频，以复制操作接收，不移动源文件。
4. Windows 鼠标/触控板移到画面底部时显示控制栏，停止移动约 2.5 秒后隐藏；拖动进度条或使用设置/菜单时暂缓隐藏。安卓触摸交互保留原方式。
5. 默认冷暖、色调均为 0；美白推荐值由 32 改为 20，磨皮保持 70。升级时只迁移与旧推荐值相同的参数，其他自定义值保留。“恢复推荐”可随时应用新默认。
6. 修复 GPUPixel 在美白强度零附近的跳变，两平台效果从 0 连续增加，不增加滤镜绘制次数或 CPU/GPU 数据往返。

## 美白根因与像素证据

界面 0–100 到滤镜 0–1 的数值映射正确，界面上的 1 实际传入 0.01。问题在两个固定版本的 `beauty_face_unit_filter.cc`：当 `whiten > 0` 时，前三层 LUT 已完整应用，只有最后的自定义 LUT 受强度控制。

原末行 `mix(color, color_custom, whiten)` 改为 `mix(colorEPM, color_custom, clamp(whiten, 0, 1))`，其中 `colorEPM` 是磨皮后、未美白的颜色。这样保留原强度 1 的终点，同时使 0 对应不美白。Windows 使用带原作者署名的替换着色器，Android 编译构建目录内的源码副本，未修改下载依赖。

同一 GPUPixel 示例人像、磨皮 0.7、只改变美白的 Windows GPU 实测（RGB 0–255 平均绝对像素差，相对美白 0）：

| 内部美白值 | 修复前 | 修复后 |
|---|---:|---:|
| 0.001 | 6.1841 | 0.0116 |
| 0.01（界面 1） | 6.3333 | 0.1786 |

分别在磨皮关闭和 0.7 两种情况下验证 0.001、0.01、0.1、0.2、0.32、0.5、1 共 14 组 GPU 输出：Windows 和安卓均通过单调性、相对 0/1 端点的线性误差检查。线性误差阈值为 RGB 平均 0.8，允许 8 位帧缓冲量化。Android 使用合成颜色图，不能把其像素差与 Windows 人像的差值直接横向比较。视觉检查确认低强度不会突然套用旧版完整调色；审美仍取决于片源和用户选择。

## 验证结果

| 项目 | 结果 | 范围 |
|---|---|---|
| Windows CTest | 通过 | 3/3 套件；含 GPU 连续性、帧转换、缓存/视频库、目录屏蔽及预设迁移 |
| Windows 播放与界面 | 通过 | 打包 EXE 的 57/57 项检查，包括返回列表暂停、位置保持、列表空格不恢复播放、底部鼠标唤出、静止隐藏、Qt 原生拖入事件接收/拒绝 |
| Android 自测 | 通过 | NEON/颜色回归、EGL/资源初始化、GPU 美白连续性和模型加载，9/9 组检查 |
| Android 播放与界面 | 通过 | 43/43 项，含授权目录视频播放、暂停调参、原图对比、触控、续播以及返回列表保持暂停/禁用播放快捷键 |
| Android 真实菜单屏蔽/恢复 | 通过 | 仅对本次测试目录操作：2 个测试视频 → 屏蔽后 0 个 → 重启仍为 0 → 恢复后 2 个 |
| 默认值迁移 | 通过 | 原推荐冷暖/色调转为 0，美白转为 20；非默认自定义值保留，恢复推荐为新默认 |
| Windows/Android 安装包 | 通过 | Windows 已从便携目录运行；安卓覆盖安装 v0.4.1（versionCode 401） |

Windows 拖拽和悬停自动检查向实际 QQuickWindow 发送鼠标、DragEnter、Drop 事件，走完整应用事件路由；未另行驱动资源管理器进行人工拖动。没有在本轮重新执行 10 分钟/4K 长测或其他安卓机型测试。屏蔽主要针对当前文件夹标识：外部存储的 MediaStore/SAF 路径已统一，其他文档提供器的不同别名不保证自动合并。

## 本地证据与产物

`artifacts/v041`（不随源码提交）保存：

- `ctest.log`、`windows-playback.json`、`windows-build.log`、`android-build.log`。
- `whitening-before/results.json`、`whitening-after/results.json` 及同帧强度对比 PNG。
- `android-block-restart.json`、`android-block-restored.json` 以及真实菜单 UI XML。
- Android 自测完整记录：`artifacts/android/device-20260928-235944/report.json`。
- Android 最终播放完整记录：`artifacts/android/device-20260929-000924/report.json`，退出后日志无崩溃/ANR。

| 产物 | 字节 | SHA256 |
|---|---:|---|
| `dist/windows-x64/BeautyPlayer.exe` | 1281536 | `D18959AEDCD52EE9E16DB794C8271194230B21813AB9D6CC8F82F24D24A3C46A` |
| `dist/android-arm64/BeautyPlayer-arm64-v8a.apk` | 92818773 | `82BE9AFB229F0B0DE0479CB5D7BCB2E65951073399C009103ADECABDED99F684` |

Windows 需保留完整便携目录；APK 使用本机既有测试签名。只推送源码、测试和文档，不上传 SDK/模型/安装包。

实现依据：[Qt HoverHandler](https://doc.qt.io/qt-6/qml-qtquick-hoverhandler.html)、[Qt DropArea](https://doc.qt.io/qt-6/qml-qtquick-droparea.html)，以及本地固定提交的 GPUPixel 着色器源码。
