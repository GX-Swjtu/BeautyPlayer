# Rebuild the Qt 6.11.2 FFmpeg plugin with one codec-discovery fix. The Qt kit
# stays untouched; the replacement is a separate, dynamically loaded LGPL plugin.
if(NOT Qt6_VERSION VERSION_EQUAL "6.11.2" OR NOT ANDROID_ABI STREQUAL "arm64-v8a")
    message(FATAL_ERROR "The AV1 plugin patch is pinned to Qt 6.11.2 / Android ARM64; revalidate before upgrading.")
endif()
set(qt_media_source "${CMAKE_SOURCE_DIR}/third_party/qtmultimedia-6.11.2/src/plugins/multimedia/ffmpeg")
set(ffmpeg_headers "${CMAKE_SOURCE_DIR}/third_party/FFmpeg-n7.1.5")
if(NOT EXISTS "${qt_media_source}/qffmpegcodecstorage.cpp" OR NOT EXISTS "${ffmpeg_headers}/libavcodec/avcodec.h")
    message(FATAL_ERROR "Run scripts/setup-android.ps1 to fetch the pinned codec patch sources.")
endif()
get_target_property(qt_core_path Qt6::Core IMPORTED_LOCATION_RELEASE)
get_filename_component(qt_lib_dir "${qt_core_path}" DIRECTORY)
foreach(component AVCODEC AVFORMAT AVUTIL SWRESAMPLE SWSCALE)
    string(TOLOWER "${component}" library)
    set(${component}_INCLUDE_DIR "${ffmpeg_headers}" CACHE PATH "Pinned FFmpeg headers" FORCE)
    set(${component}_LIBRARY "${qt_lib_dir}/lib${library}.so" CACHE FILEPATH "Qt's FFmpeg runtime" FORCE)
endforeach()
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/third_party/qtmultimedia-6.11.2/cmake")
find_package(Qt6 6.11.2 EXACT REQUIRED COMPONENTS FFmpegMediaPluginImplPrivate)

set(patch_dir "${CMAKE_CURRENT_BINARY_DIR}/qt-media-patch")
file(MAKE_DIRECTORY "${patch_dir}/include/libavutil")
# Public FFmpeg avconfig.h for the kit's little-endian ARM64 target. These are
# its only public configuration macros; FFmpeg itself is not recompiled here.
file(WRITE "${patch_dir}/include/libavutil/avconfig.h"
    "#pragma once\n#define AV_HAVE_BIGENDIAN 0\n#define AV_HAVE_FAST_UNALIGNED 1\n")
file(READ "${qt_media_source}/qffmpegcodecstorage.cpp" codec_storage)
set(av1_anchor "        if (codecName == \"avc\"_L1)\n            return AV_CODEC_ID_H264;")
string(FIND "${codec_storage}" "${av1_anchor}" anchor_pos)
if(anchor_pos EQUAL -1 OR codec_storage MATCHES "return AV_CODEC_ID_AV1;")
    message(FATAL_ERROR "Qt codec source changed; review the AV1 mapping patch.")
endif()
string(REPLACE "${av1_anchor}" "${av1_anchor}\n        if (codecName == \"av01\"_L1)\n            return AV_CODEC_ID_AV1;" codec_storage "${codec_storage}")
file(CONFIGURE OUTPUT "${patch_dir}/qffmpegcodecstorage.cpp" CONTENT "${codec_storage}" @ONLY)

# Defining all codec-storage functions in this object prevents the old object
# from being extracted from Qt's static implementation archive. Other backend
# code remains the exact Qt kit implementation. Undefined/duplicate symbols fail
# the link, and the deployment script checks the packaged plugin's hash.
add_library(BeautyFFmpegPlugin MODULE
    "${qt_media_source}/qffmpegplugin.cpp" "${patch_dir}/qffmpegcodecstorage.cpp")
target_include_directories(BeautyFFmpegPlugin PRIVATE "${patch_dir}/include" "${ffmpeg_headers}" "${qt_media_source}")
target_link_libraries(BeautyFFmpegPlugin PRIVATE Qt6::FFmpegMediaPluginImplPrivate)
target_link_options(BeautyFFmpegPlugin PRIVATE -Wl,--no-undefined -Wl,-z,max-page-size=16384
    -Wl,--gc-sections -Wl,--strip-debug "-Wl,-Map,${patch_dir}/plugin-link.map")
set_target_properties(BeautyFFmpegPlugin PROPERTIES
    CXX_VISIBILITY_PRESET hidden VISIBILITY_INLINES_HIDDEN YES
    OUTPUT_NAME plugins_multimedia_ffmpegmediaplugin_arm64-v8a
    LIBRARY_OUTPUT_DIRECTORY "${patch_dir}")
add_dependencies(BeautyPlayer BeautyFFmpegPlugin)
