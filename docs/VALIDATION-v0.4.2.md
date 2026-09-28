# v0.4.2 字体与文件夹快捷筛选修复

日期：2026-09-29。基于 v0.4.1（`801ea4a`）；Android 测试机为 24129PN74C / 骁龙 8 Elite、Android 16。

## 字体

从美颜面板实际的 `font` 属性创建 `QTextLayout`，读取每个字形的 `QRawFont`：修复前“恢复推荐屏蔽”回退到 `MiSans Japanese VF`。在 12 px 字号下，“复”的字形包围盒宽 8 px，其余汉字为 12 px，与截图中“复”偏窄一致；不是按钮宽度挤压。

Android 启动时为 Han 脚本设置应用内字体回退顺序，仅选择手机已安装的 `Noto Sans CJK SC`、`Noto Sans SC`、`MiSans VF` 或 `Droid Sans Fallback`。该机修复后使用 `Noto Sans CJK SC`，六个字形宽度均为 12 px。“恢复推荐”截图已检查；Windows 字体不变，没有增加字体下载或 APK 内字体文件。

## 屏蔽目录

普通规范路径在 v0.4.1 已能移除快捷标签；本次未在该路径上复现残留。新增的目录别名测试复现了旧版缺陷：传入含原生分隔符、`/./` 或末尾斜杠的路径时，屏蔽、选中状态和恢复未使用统一目录标识。

修复后读取旧设置、屏蔽、恢复及判断时统一清理路径；Windows 比较忽略大小写。文件列表和快捷栏计数在同一次过滤中构建，屏蔽目录及子目录不再进入快捷栏。若正选中被屏蔽的目录，自动回到“全部视频”。屏蔽管理仍保留“恢复”入口。

## 检查

- 新增 `blockedFolderPathsUseTheSameIdentity`：旧实现失败；新实现覆盖父子目录、相近前缀不误伤、路径别名、旧设置加载、刷新、重启和大小写不同的恢复。
- `--library-probe` 检查实际 QML 快捷栏模型和按钮：原先存在 → 屏蔽后消失 → 恢复后重新出现，同时检查列表数量及选中状态。诊断使用内存设置，不修改用户屏蔽列表。
- 字体诊断记录实际字体、字形宽度及 Han 回退配置；手机截图确认“复”宽度正常。
- Windows CTest 三个套件通过；Android 播放与交互 43 项通过，包括美颜面板、暂停调整、对比和回到列表暂停。
- 最终便携包的库界面检查 8/8 通过，最终 APK 真机库界面检查 9/9 通过。另通过手机实际菜单屏蔽测试目录，确认快捷按钮和视频一起消失，重启仍隐藏；恢复后按钮及 2 个视频重新出现。测试结束恢复该目录原先未屏蔽的状态。

原始报告位于本机 `artifacts/v042` 及 `artifacts/android/device-*`，不提交用户媒体或原始目录截图。字体修复仅在上述手机实测；未重新执行 10 分钟或 4K 性能测试。不同文档提供器的独立目录别名仍不保证合并。

Windows 交付完整 `dist/windows-x64` 便携目录；Android 为 `dist/android-arm64/BeautyPlayer-arm64-v8a.apk`，版本 0.4.2 / versionCode 402，使用既有本机测试签名。

具体证据：

- 字体修复前：`artifacts/android/device-20260929-002155/report.json`。
- 最终 Windows：`artifacts/v042/windows-final.json`、`ctest-final.log`；旧实现失败证据为 `regression-before.txt`。
- 最终 Android 库界面：`artifacts/android/device-20260929-003113/report.json`。
- Android 播放和修复后面板截图：`artifacts/android/device-20260929-002802`（字体改动与最终包相同；最终包随后将文件夹计数改为缓存的聚合表，并重新通过库界面与菜单测试）。
- 真实菜单、重启、恢复的 UI 树：`artifacts/v042/android-manual-*.xml`。

| 产物 | SHA256 |
|---|---|
| Windows EXE | `0A8583929AF55C02C3A1699D63F40CE0B318BAFC0BEAD82C74AF669AF2F2349A` |
| Android APK | `31A891EFC0BE219C132CBA27829129967C6024DF4C7E60EA40D065CF6E8F50B3` |
