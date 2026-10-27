cmake_minimum_required(VERSION 3.21)
project(LeoCADRenderProbe LANGUAGES CXX)

find_package(Qt6 6.5 REQUIRED COMPONENTS Widgets OpenGL OpenGLWidgets)
qt_add_executable(leocad-render-probe main.cpp)
target_compile_features(leocad-render-probe PRIVATE cxx_std_17)
target_link_libraries(leocad-render-probe PRIVATE Qt6::Widgets Qt6::OpenGL Qt6::OpenGLWidgets)

# Keep the diagnostic independent of the application build and use its real shaders.
file(GLOB ShaderFiles CONFIGURE_DEPENDS "../../resources/shaders/*.glsl")
qt_add_resources(leocad-render-probe shaders
    PREFIX "/probe" BASE "../../resources" FILES ${ShaderFiles})
set_target_properties(leocad-render-probe PROPERTIES MACOSX_BUNDLE FALSE)

if(OHOS)
    set_target_properties(leocad-render-probe PROPERTIES
        QT_HARMONYOS_APP_BUNDLE_NAME "io.github.xiaotaop.leocad.glprobe"
        QT_HARMONYOS_APP_NAME "LeoCAD GL Probe"
        QT_HARMONYOS_APP_LABEL "LeoCAD GL Probe"
        QT_HARMONYOS_APP_VENDOR "xiaotaop"
        QT_HARMONYOS_APP_VERSION_CODE 1
        QT_HARMONYOS_APP_VERSION_NAME "0.2.0"
        QT_HARMONYOS_MODULE_DESCRIPTION "Qt Widgets and OpenGL capability probe"
        QT_HARMONYOS_MODULE_DEVICE_TYPES "2in1;tablet"
        QT_HARMONYOS_TARGET_ARCHS "arm64-v8a"
        QT_HARMONYOS_TARGET_SDK_VERSION "26.0.0"
        QT_HARMONYOS_COMPILE_SDK_VERSION "26.0.0"
    )
endif()
