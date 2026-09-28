# Third-party components

BeautyPlayer's original code is licensed under MIT (`LICENSE`, copyright 2026 GX-Swjtu).
This permission does not relicense any dependency. The Qt-derived player setup/UI retains its BSD-3-Clause notices; the adapted GPUPixel shader retains its MIT notice.
Git contains source, notices and reproducible dependency download instructions only. SDK binaries, face models, makeup resources, Qt DLLs, build outputs, personal media and playback history are excluded from Git.

## Qt

Qt 6.11.2 MSVC x64, using Core, Gui, Qml, Quick, Quick Controls 2, Network and Multimedia.
Qt is dynamically linked. Open-source license texts are included in `licenses/LGPL-3.0.txt` and `licenses/GPL-3.0.txt`.
Users may replace the dynamically linked Qt libraries with compatible modified versions, debug such modifications and rebuild the application from this repository. No additional EULA restricts those rights.
Packaging excludes unused Qt Virtual Keyboard, Lottie vector-image plugins and QML/Quick3D profiling plugins. These optional modules are not required by this player and must not be accidentally introduced by windeployqt's default plugin expansion. Each package starts from a clean directory, preserving the previous package under ignored local artifacts.
Sources and applicable module licenses: https://code.qt.io/ ; https://download.qt.io/archive/qt/6.11/6.11.2/ .
Qt-provided FFmpeg binaries and plugins are deployed by Qt's windeployqt; source: https://ffmpeg.org/ and the matching Qt distribution.
The deployed avcodec-61.dll reports "LGPL version 2.1 or later" via avcodec_license(); text included in `licenses/FFmpeg-LGPL-2.1.txt`.

Player window and QMediaPlayer/VideoOutput setup adapted from the Qt Multimedia example:
https://code.qt.io/cgit/qt/qtmultimedia.git/tree/examples/multimedia/video/mediaplayer?h=v6.11.2
Copyright (C) 2022-2024 The Qt Company Ltd. BSD-3-Clause text included in `licenses/Qt-example-BSD-3-Clause.txt`.

## GPUPixel and Mars-Face

GPUPixel v1.3.0-beta3, commit 9069057a529d3130f518e54d68fbe86af62239c1.
https://github.com/pixpark/gpupixel/releases/tag/v1.3.0-beta3
Copyright (c) 2023 PixPark. MIT license in `licenses/GPUPixel-MIT.txt`.
The unmodified official Windows release includes `mars-face-kit.dll`, face detection/alignment models and makeup resources. Mars-Face is a precompiled upstream component; this project does not claim to provide its implementation source or assign it an independent license.
The repository's MIT license covers neither an independent redistribution grant for that closed component nor its models. `scripts/setup.ps1` retrieves the original upstream SDK for local building. No SDK or portable binaries are uploaded by this source publication; a public binary release requires checking the upstream component's redistribution terms separately.
Exact download hashes are recorded in `dependencies.lock.json`.

Android v0.3.0 uses GPUPixel v1.3.1, commit `5a1b34ea74cf131088ee4f29946a16c667a83e26`, with the source, models, resources and ARM64 Mars library from that same commit. The Android EGL context and Qt pipeline adapter are provided in this repository. Windows continues to use beta3; its headers and libraries are not mixed with the Android SDK. Android download hashes are in `android-dependencies.lock.json`. The same closed-component redistribution boundary applies to Android.

v0.4.1 adapts the MIT-licensed beauty shader to interpolate the entire whitening result continuously from zero. The Windows adaptation is in `src/ContinuousBeautyShader.h`; `cmake/AndroidGPUPixel.cmake` applies the equivalent change to a build-directory copy of the pinned Android source. Upstream copyright and license notices are retained.

## libyuv

Pinned source commit `7320155784bdfefb69db8edddca37ecacf7e5198` from https://chromium.googlesource.com/libyuv/libyuv .
The archive is fetched from the same commit in the https://github.com/lemenkov/libyuv mirror, with SHA256 in `dependencies.lock.json`.
Statically linked, BSD-3-Clause; notices in `licenses/libyuv-LICENSE.txt`, `licenses/libyuv-PATENTS.txt` and `licenses/libyuv-AUTHORS.txt`.
Built with clang-cl, JPEG disabled, and `LIBYUV_UNLIMITED_DATA` for more accurate YUV coefficients. Upstream source is unmodified. CPU/OS feature detection selects the applicable scalar, SSE or AVX2 kernels; no application-wide AVX2 requirement is introduced.

Android builds libyuv with NDK r27c Clang and ARM64 NEON kernels, using HWCAP feature detection for NEON, dot-product and i8mm. SME is disabled because this NDK lacks the complete SME ZA ABI. GPUPixel includes stb_image (public domain/MIT choice; license in the pinned source header). Android dynamically links Qt 6.11.2 and its FFmpeg libraries. License texts are embedded as Qt resources and also shipped beside the locally built APK. Qt Android application/Gradle template adaptations retain Qt's BSD-3-Clause notice.

## Android Java support

AndroidX Core 1.17.0 and its AndroidX dependencies are used by the Qt Android template; AndroidX is licensed under Apache-2.0. The license copied from the official Core AAR is in `licenses/AndroidX-Apache-2.0.txt`. The runtime also includes Kotlin standard library 2.2.10 and JetBrains annotations 23.0.0, licensed under Apache-2.0. Sources: https://android.googlesource.com/platform/frameworks/support/ ; https://github.com/JetBrains/kotlin ; https://github.com/JetBrains/java-annotations . Build-time Gradle, AGP, JDK and Android SDK tools are downloaded separately and are not bundled as application runtime code.

## OpenGL support

GLAD 0.1.36 generated loader and Khronos platform header, taken from the pinned GPUPixel source tree. Licenses: `licenses/GLAD.txt`, `licenses/Khronos.txt`.
GLFW declarations taken from that same tree; GLFW is linked inside upstream GPUPixel. License: `licenses/GLFW.txt`.

## Test media (not bundled with the app)

Angela Roma, “Close-up Face of a Woman”, Pexels video 7477538:
https://www.pexels.com/video/close-up-face-of-a-woman-7477538/
Pexels license: https://www.pexels.com/license/ . Downloaded for local playback/effect verification. The 1080p30 derivative is generated from the original 2160x3840 23.976 fps clip.

`demo.png` comes from the official GPUPixel demonstration archive, used only for regression comparisons. Synthetic direction/color and flash/beep clips are generated locally with FFmpeg.
