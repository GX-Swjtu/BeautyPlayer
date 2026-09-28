set_target_properties(BeautyPlayer PROPERTIES
    QT_ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_SOURCE_DIR}/android"
    QT_QML_ROOT_PATH "${CMAKE_SOURCE_DIR}/qml"
    QT_ANDROID_MIN_SDK_VERSION 28 QT_ANDROID_TARGET_SDK_VERSION 36
    QT_ANDROID_SDK_BUILD_TOOLS_REVISION "36.0.0"
    QT_ANDROID_VERSION_NAME "${PROJECT_VERSION}" QT_ANDROID_VERSION_CODE 402
    QT_ANDROID_APP_NAME "映颜播放器" QT_ANDROID_PACKAGE_NAME "org.beautyplayer.app"
    QT_ANDROID_EXTRA_LIBS "${MARS_LIBRARY}")
target_link_options(BeautyPlayer PRIVATE -Wl,-z,max-page-size=16384 -Wl,--gc-sections)
find_package(Qt6 REQUIRED COMPONENTS GuiPrivate)
target_link_libraries(BeautyPlayer PRIVATE Qt6::GuiPrivate)
target_sources(BeautyPlayer PRIVATE src/android/AndroidSelfTest.cpp tests/FrameConversionTests.cpp)
target_compile_definitions(BeautyPlayer PRIVATE BEAUTY_EMBEDDED_TESTS)
qt_import_plugins(BeautyPlayer EXCLUDE_BY_TYPE qmltooling platforminputcontexts vectorimageformats)
file(GLOB BEAUTY_LICENSE_FILES "${CMAKE_SOURCE_DIR}/licenses/*.txt")
qt_add_resources(BeautyPlayer "third_party_licenses" PREFIX "/licenses" BASE "${CMAKE_SOURCE_DIR}/licenses" FILES ${BEAUTY_LICENSE_FILES})
qt_add_resources(BeautyPlayer "beauty_models" PREFIX "/beauty/models"
    BASE "${GP_ANDROID_ROOT}/third_party/mars-face-kit/models"
    FILES "${GP_ANDROID_ROOT}/third_party/mars-face-kit/models/face_align.mars_model"
          "${GP_ANDROID_ROOT}/third_party/mars-face-kit/models/face_det.mars_model")
file(GLOB GP_ANDROID_IMAGES "${GP_ANDROID_ROOT}/src/res/*.png")
qt_add_resources(BeautyPlayer "beauty_makeup" PREFIX "/beauty/res" BASE "${GP_ANDROID_ROOT}/src/res" FILES ${GP_ANDROID_IMAGES})
