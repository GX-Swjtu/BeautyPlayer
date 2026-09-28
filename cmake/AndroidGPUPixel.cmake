# Android uses the version-matched v1.3.1 source, models and 16 KB Mars library.
# Windows keeps its verified beta3 DLL. No cross-version C++ ABI mixing.
if(NOT ANDROID_ABI STREQUAL "arm64-v8a")
    message(FATAL_ERROR "This Android distribution requires arm64-v8a")
endif()
set(GP_ANDROID_ROOT "${CMAKE_SOURCE_DIR}/third_party/android/gpupixel-5a1b34ea74cf131088ee4f29946a16c667a83e26")
set(MARS_LIBRARY "${GP_ANDROID_ROOT}/third_party/mars-face-kit/libs/android/arm64-v8a/libmars-face-kit.so")
add_library(mars_face SHARED IMPORTED)
set_target_properties(mars_face PROPERTIES IMPORTED_LOCATION "${MARS_LIBRARY}")
file(GLOB GP_ANDROID_SOURCES CONFIGURE_DEPENDS
    "${GP_ANDROID_ROOT}/src/core/*.cc" "${GP_ANDROID_ROOT}/src/filter/*.cc"
    "${GP_ANDROID_ROOT}/src/source/*.cc" "${GP_ANDROID_ROOT}/src/sink/*.cc"
    "${GP_ANDROID_ROOT}/src/utils/*.cc")
list(REMOVE_ITEM GP_ANDROID_SOURCES "${GP_ANDROID_ROOT}/src/core/gpupixel_context.cc" "${GP_ANDROID_ROOT}/src/sink/sink_view.cc")
add_library(gpupixel STATIC ${GP_ANDROID_SOURCES} src/android/GPUPixelEglContext.cpp)
set_target_properties(gpupixel PROPERTIES POSITION_INDEPENDENT_CODE ON AUTOMOC OFF)
target_include_directories(gpupixel PUBLIC "${GP_ANDROID_ROOT}/include" "${GP_ANDROID_ROOT}/src"
    "${GP_ANDROID_ROOT}/third_party/mars-face-kit/include"
    PRIVATE "${GP_ANDROID_ROOT}/third_party/stb/include" "${GP_ANDROID_ROOT}/third_party/ghc/include")
target_link_libraries(gpupixel PUBLIC mars_face EGL GLESv3 PRIVATE beauty_yuv android log jnigraphics)
target_compile_options(gpupixel PRIVATE -ffunction-sections -fdata-sections)
