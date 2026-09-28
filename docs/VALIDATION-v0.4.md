# v0.4 视频库与图标验证

日期：2026-09-28。基于 v0.3.1（`e4f85b4`）增加共用图标、缓存预览图、`.nomedia` 设置、单列/双列及三种排序。Windows 与 Android ARM64 均完成 Release 构建；没有修改美颜滤镜或解码管线。

## 功能与实现

- 图标采用薄荷绿播放丝带和美颜星光。Windows EXE/窗口、Android 自适应图标和视频库标题使用同一母图。资源与完整生成提示词在 [图标说明](../assets/brand/README.md)，由内置 image_gen 生成。
- Windows 使用 Shell 缩略图，Android 优先使用系统媒体缩略图；系统无图或近乎纯黑/白时尝试约 10% 位置（最多 10 秒附近）。后台按需串行生成，不在扫描过程中逐个打开视频。
- 缓存最长边 512 像素；URL、大小、修改时间变化时更新；30 天未使用自动清理，目标上限 256 MiB / 4096 个条目。损坏/不支持文件的失败结果缓存 30 分钟。仅清理应用自己的缩略图文件。
- `.nomedia` 默认启用；Android 通过系统目录选择器授予读取权限后，可以在关闭该选项时补充隐藏目录视频。未授权的目录仍受系统访问限制。
- 文件名采用自然数字排序；创建时间缺失时回退拍摄/入库/修改等可用时间。布局、排序方向与过滤偏好跨启动保存。

## 验证结果

| 项目 | 结果 | 证据与范围 |
|---|---|---|
| Windows 构建与便携启动 | 通过 | 打包目录启动，PATH 去除 Qt/项目工具链，3 张预览图显示；无需 Qt Creator |
| Android 构建与安装 | 通过 | arm64-v8a，24129PN74C / SM8750，Android 16，Adreno 830；沿用本机测试签名覆盖安装 |
| Windows 冷/热缓存 | 通过 | 3 个测试视频，首次 generated=3；独立进程再次启动 diskHits=3、generated=0、failed=0 |
| Android 冷/热缓存 | 通过 | 最终 APK 在隔离缓存中首次 generated=2；再次启动 diskHits=2、generated=0、failed=0 |
| 缓存失效与自动清理 | 通过 | 单元测试覆盖跨实例复用、文件大小/修改时间失效、31 天未用项清理、近期访问保留、容量淘汰、非缓存文件保留及失败抑制；使用模拟文件时间，无需实际等待 30 天 |
| `.nomedia` Windows | 通过 | 父/子目录、重叠扫描根、开关切换、删除标记后刷新 |
| `.nomedia` Android | 通过 | 对 Movies 授权后，界面实际点按：开启时 2 个测试视频，关闭时 3 个，包含 Hidden/.nomedia 下的视频；测试后恢复开启 |
| 单列/双列 | 通过 | 两端截图检查预览图、文字布局；手机实际点按切换为两列，重启复核设置 |
| 三种排序 | 通过 | 单元测试验证名称、创建时间、大小的升降序；手机实际选择大小升序，8.5 MB 排在 18.9 MB 前，并复核重启保存 |
| 1 万文件扫描 | 通过 | 100 个目录、10000 个文件，本机 worker 236 ms，扫描和模型发布 295 ms；不包含预览图生成，不等同所有手机性能 |
| Windows 回归 | 通过 | CTest 3/3 测试套件通过；播放交互诊断 48/48 |
| Android 授权目录播放 | 通过 | 直接打开授权目录中的测试视频，39/39 检查通过，含暂停调参、对比、归零、五官/美妆开关、长按、滑动、续播与紧凑面板；无播放器/美颜错误 |

Android 最终媒体索引耗时为 54 ms / 83 ms（包含已授权目录补充，当前库 33 项；预览和自动检查仅匹配两个公开测试视频）。这只是本机当前目录规模下的结果，未对其他手机或任意大目录宣称相同速度。检查不上传媒体、目录索引或播放历史。

本轮没有重新执行 10 分钟温控测试、4K 性能测试、所有 Android 文档提供器或系统部分授权/撤销组合；历史结果仍见前版记录。预览图生成失败会显示占位图，视频仍可点击播放。Android 片尾停止后调参预览的既有局限未在本轮修改。

## 本地证据

以下为本地忽略目录，不随源码提交：

- `artifacts/library-v04/ctest-final.log`、`playback.json`。
- `artifacts/library-v04/windows-final-cold.json`、`windows-final-warm.json`、`portable-final.json` 及对应列表/网格截图。
- `artifacts/library-v04/android-nomedia-on.json`、`android-nomedia-off.json` 及 UI XML。
- `artifacts/library-v04/android-sort-restart.xml`、`android-layout-restart.xml`：真实控件点按与重启后的布局/排序。
- `artifacts/android/device-20260928-221733` / `221743`：最终 APK 预览图冷/热缓存报告、截图、日志。
- `artifacts/android/device-20260928-221753`：授权目录视频播放报告，39 项全部通过。
- `build/windows-release/artifacts/library-scale.json`：1 万文件索引耗时。

## 产物

| 产物 | 字节 | SHA256 |
|---|---:|---|
| `dist/windows-x64/BeautyPlayer.exe` | 1207808 | `EA1C7C0B791948C8261879EE103C5E09633379F1A7E722E6BF24AE5B6535474A` |
| `dist/android-arm64/BeautyPlayer-arm64-v8a.apk` | 92720469 | `A176AE228726B9243E0F2F2500F950F6780125539E65291690B70A11A9696F23` |

Windows 需复制整个便携目录；APK 为本地测试签名包。源码按仓库既有许可提交，未将预编译 SDK、模型、测试视频或安装包上传仓库。

## 官方接口依据

- [Android 媒体访问与 loadThumbnail](https://developer.android.com/training/data-storage/shared/media)
- [Android 目录授权与 Storage Access Framework](https://developer.android.com/training/data-storage/shared/documents-files)
- [Windows IShellItemImageFactory](https://learn.microsoft.com/en-us/windows/win32/api/shobjidl_core/nn-shobjidl_core-ishellitemimagefactory)
