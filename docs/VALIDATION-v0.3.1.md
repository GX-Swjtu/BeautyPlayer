# v0.3.1 紧凑美颜面板验证

日期：2026-09-28。范围：美颜界面及其参数绑定；不修改解码器、帧转换、美颜流水线或参数默认值。

## 交互与布局

- 沿用播放控制栏的“美颜”入口，默认不开面板。
- 一排横向效果选项、一根共用调整条；保留全部八项效果。
- 常规面板高 172 个逻辑像素，窗口高度小于 480 时压缩至 144。原桌面面板在 760 高窗口内约为 631。
- 面板打开后隐藏播放控制栏与临时提示；视频按原比例完整显示在其上方，无模态遮罩或整体变暗。
- 保留美颜总开关、恢复推荐、按住对比、完成。瘦脸、大眼、口红、腮红保留独立开关；点选项目不启动效果，调高强度或打开开关后才启动。
- 关闭面板后恢复完整播放区域；窗口变窄后将当前选项滚动到可见范围。

## 验证结果

| 检查 | 结果 | 证据与边界 |
| --- | --- | --- |
| Windows Release 构建与独立便携目录 | 通过 | 从 PATH 移除 Qt / 项目路径，在 `dist/windows-x64` 直接启动 |
| Windows 播放与界面诊断 | 通过，48/48 | `artifacts/ui-compact/windows-final.json` |
| 单调整条切换参数、负值调色、恢复推荐 | 通过 | 实际发送鼠标输入，核对 `BeautySettings` 改变的是选中参数 |
| 美妆只选择不启用、独立开关、总开关 | 通过 | 同一界面诊断；默认重置后的美妆为 0 |
| 原图按住/松开、暂停刷新、完成关闭 | 通过 | UI 对比按钮状态及图像差异检查 |
| 视频区域不被面板覆盖 | 通过 | 场景坐标检查和截图核对；不是将原图回退作为美颜成功 |
| 1120×760、390×844、844×390、360×480 | 通过 | Windows 逻辑窗口尺寸；DPI 为 150%，截图物理尺寸相应放大 |
| Android ARM64 构建、签名、库对齐 | 通过 | 构建脚本验证 86 个原生库及 APK v2 签名 |
| Android 竖屏真机播放与界面诊断 | 通过，39/39 | 骁龙 8 Elite / 24129PN74C / Android 16；`artifacts/android/device-20260928-212318/report.json` |
| Android 真实触控横滑、选择口红、从 0 拖到 79 | 通过 | ADB 触控输入；`android-touch-swiped.png`、`android-touch-makeup-off.png` |
| Android 中途暂停后同帧口红预览 | 通过 | `android-touch-paused.png` 与 `android-touch-paused-adjusted.png`；嘴唇变化，视频姿态不变 |
| 原有回归测试 | 通过，3/3 | PipelineTests、FrameConversionTests、LibraryHistoryTests；`artifacts/ui-compact/ctest.log` |
| Android 片尾停止后调参 | 存在限制 | 参数和开关会更新，最后一帧可能不刷新；重新播放后中途暂停可以立即调参。此次未修改相关核心代码 |
| Android 真机横屏、其他机型、长时间性能复测 | 本次未执行 | Windows 已检查横屏布局；既有性能记录不代表此次重新测量 |

设备诊断使用已获准的公开人脸视频 `BeautyPlayer-demo.mp4`；临时诊断不改写用户的美颜设置、视频库或播放历史。截图和原始日志留在本机 `artifacts`，不进入源码仓库。系统“刷新率显示”悬浮文字可能出现在 ADB 截屏中，不是播放器 UI；应用自身截图不含该悬浮层。

## 产物

- Windows：`dist/windows-x64/BeautyPlayer.exe`，598528 字节。运行或复制时保留整个目录。
- Android：`dist/android-arm64/BeautyPlayer-arm64-v8a.apk`，92034847 字节，版本 0.3.1 / versionCode 301，已覆盖安装至连接的测试手机。
- Windows EXE SHA256：`5df455376bc5b596a8b92c35f1c4dd490214b89a56ccb44c2d33d09711bd426c`。
- Android APK SHA256：`eb40b06021b545f3b6d18e1938234167450a3cdd80816f4c2d2daa267fcdad1a`。

## 复测

```powershell
.\scripts\build.ps1
.\scripts\package.ps1
.\dist\windows-x64\BeautyPlayer.exe --probe artifacts/ui-check --seconds 35 --exercise --profile default --library-folder E:/code/player/test-media E:/code/player/test-media/face-1080.mp4
ctest --test-dir build/windows-release --output-on-failure
.\scripts\build-android.ps1 -SkipSetup
.\scripts\test-android.ps1 -Video test-media/face-1080.mp4 -Seconds 35 -Exercise -Profile default
```

参考截图：`artifacts/ui-compact/windows-final-beauty-panel.png`、`windows-final-beauty-phone.png`、`windows-final-beauty-landscape.png`、`windows-final-beauty-small.png`；Android 应用截图在上述设备诊断目录，额外触控截图在 `artifacts/ui-compact/`。
