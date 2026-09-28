if(ANDROID)
    set(LIBYUV_DISABLE_JPEG ON CACHE BOOL "" FORCE)
    set(UNIT_TEST OFF CACHE BOOL "" FORCE)
    # r27c's Clang accepts the upstream probe but lacks the full SME ZA ABI.
    # ARM64 NEON / dot-product / i8mm remain enabled with runtime dispatch.
    set(CAN_COMPILE_SME OFF CACHE INTERNAL "NDK r27c has no complete SME ABI" FORCE)
    add_subdirectory(third_party/libyuv EXCLUDE_FROM_ALL)
    foreach(part yuv_common_objects yuv_neon64 yuv_sve yuv_sme)
        if(TARGET ${part})
            target_compile_definitions(${part} PRIVATE LIBYUV_UNLIMITED_DATA)
        endif()
    endforeach()
    target_include_directories(yuv INTERFACE "${CMAKE_SOURCE_DIR}/third_party/libyuv/include")
    add_library(beauty_yuv ALIAS yuv)
    return()
endif()
# clang-cl exposes libyuv's complete SSSE3/AVX2 bilinear conversion kernels.
# The application and Qt continue to use MSVC. No global /arch:AVX2 is used.
include(ExternalProject)
set(BEAUTY_CLANG_CL "" CACHE FILEPATH "clang-cl used only to build libyuv")
if(NOT BEAUTY_CLANG_CL)
    find_program(BEAUTY_CLANG_CL_FOUND NAMES clang-cl.exe
        HINTS "$ENV{LLVM_ROOT}/bin" "$ENV{VCINSTALLDIR}/Tools/Llvm/x64/bin")
    set(BEAUTY_CLANG_CL "${BEAUTY_CLANG_CL_FOUND}")
endif()
if(NOT EXISTS "${BEAUTY_CLANG_CL}")
    message(FATAL_ERROR "Set BEAUTY_CLANG_CL to clang-cl.exe, or use scripts/build.cmd (LLVM/VS/Android NDK discovery).")
endif()
set(YUV_SOURCE "${CMAKE_SOURCE_DIR}/third_party/libyuv")
set(YUV_BUILD "${CMAKE_BINARY_DIR}/libyuv")
ExternalProject_Add(libyuv_build
    BUILD_ALWAYS TRUE
    SOURCE_DIR "${YUV_SOURCE}" BINARY_DIR "${YUV_BUILD}"
    DOWNLOAD_COMMAND "" UPDATE_COMMAND "" INSTALL_COMMAND ""
    CMAKE_ARGS -G Ninja "-DCMAKE_MAKE_PROGRAM=${CMAKE_MAKE_PROGRAM}"
        "-DCMAKE_C_COMPILER=${BEAUTY_CLANG_CL}" "-DCMAKE_CXX_COMPILER=${BEAUTY_CLANG_CL}"
        -DCMAKE_BUILD_TYPE=Release -DCMAKE_POLICY_DEFAULT_CMP0091=NEW
        # Upstream's default blue coefficient is clipped for some matrices.
        # Use its accurate-coefficient option in both scalar and SIMD paths.
        "-DCMAKE_CXX_FLAGS_RELEASE=/O2 /Ob2 /DNDEBUG /DLIBYUV_UNLIMITED_DATA"
        -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDLL -DLIBYUV_DISABLE_JPEG=ON -DUNIT_TEST=OFF
    BUILD_COMMAND "${CMAKE_COMMAND}" --build <BINARY_DIR> --target yuv --parallel 8
    BUILD_BYPRODUCTS "${YUV_BUILD}/yuv.lib")
add_library(beauty_yuv STATIC IMPORTED GLOBAL)
set_target_properties(beauty_yuv PROPERTIES IMPORTED_LOCATION "${YUV_BUILD}/yuv.lib"
    INTERFACE_INCLUDE_DIRECTORIES "${YUV_SOURCE}/include")
add_dependencies(beauty_yuv libyuv_build)
