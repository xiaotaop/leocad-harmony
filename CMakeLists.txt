cmake_minimum_required(VERSION 3.28)

project(LeoCADHarmony VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

set(_qt_components
    Core Gui Widgets OpenGL OpenGLWidgets Xml Concurrent PrintSupport
    LinguistTools
)
if(NOT OHOS)
    list(APPEND _qt_components Network)
else()
    list(APPEND _qt_components Svg)
endif()

find_package(Qt6 6.12 REQUIRED COMPONENTS ${_qt_components})
find_package(ZLIB REQUIRED)

include(cmake/LeoCADSources.cmake)

option(LEOCAD_SAFE_RENDER "Disable optional render effects for device diagnosis" OFF)

# The upstream resource file names generated .qm files. Translations are handled
# by qt_add_translations(), so create a build-tree copy without those entries.
set(_generated_qrc "${CMAKE_CURRENT_BINARY_DIR}/generated/leocad_resources.qrc")
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/leocad.qrc" _qrc_lines)
set(_qrc_content "")
foreach(_line IN LISTS _qrc_lines)
    if(_line MATCHES "<file( alias=\"([^\"]+)\")?>([^<]+)</file>")
        set(_alias "${CMAKE_MATCH_2}")
        set(_path "${CMAKE_MATCH_3}")
        if(_path MATCHES "\\.qm$")
            continue()
        endif()
        if(NOT _alias)
            set(_alias "${_path}")
        endif()
        file(TO_CMAKE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/${_path}" _absolute_path)
        string(APPEND _qrc_content "        <file alias=\"${_alias}\">${_absolute_path}</file>\n")
    else()
        string(APPEND _qrc_content "${_line}\n")
    endif()
endforeach()
file(WRITE "${_generated_qrc}" "${_qrc_content}")

qt_add_executable(leocad
    ${LEOCAD_SOURCES}
    ${LEOCAD_HEADERS}
    ${LEOCAD_FORMS}
    "${_generated_qrc}"
    resources/stylesheet/stylesheet.qrc
)

target_include_directories(leocad PRIVATE common qt)
target_compile_definitions(leocad PRIVATE
    $<$<BOOL:${LEOCAD_SAFE_RENDER}>:LC_SAFE_RENDER=1>
    LC_DISABLE_UPDATE_CHECK=1
    LC_DISABLE_RENDER_DIALOG=1
    LC_DISABLE_BLENDER=1
)
if(OHOS)
    target_compile_definitions(leocad PRIVATE LC_DISABLE_NETWORK=1 LC_DISABLE_NON_CORE_FORMATS=1)
    target_link_libraries(leocad PRIVATE Qt6::Svg)
endif()
target_compile_options(leocad PRIVATE
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wno-unused-parameter;-Werror=unused-result>
)
target_link_libraries(leocad PRIVATE
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::OpenGL Qt6::OpenGLWidgets
    Qt6::Xml Qt6::Concurrent Qt6::PrintSupport
    ZLIB::ZLIB
)
if(NOT OHOS)
    target_link_libraries(leocad PRIVATE Qt6::Network)
endif()
if(WIN32)
    target_link_libraries(leocad PRIVATE wininet)
endif()
target_precompile_headers(leocad PRIVATE common/lc_global.h)

qt_add_translations(leocad
    TS_FILES ${LEOCAD_TRANSLATIONS}
    RESOURCE_PREFIX "/resources"
    LUPDATE_OPTIONS -no-obsolete
)

if(OHOS)
    set_target_properties(leocad PROPERTIES
        QT_HARMONYOS_APP_BUNDLE_NAME "io.github.xiaotaop.leocad"
        QT_HARMONYOS_APP_NAME "LeoCAD Harmony"
        QT_HARMONYOS_APP_LABEL "LeoCAD Harmony"
        QT_HARMONYOS_APP_VENDOR "xiaotaop"
        QT_HARMONYOS_APP_VERSION_CODE 1
        QT_HARMONYOS_APP_VERSION_NAME "0.1.0"
        QT_HARMONYOS_MODULE_DESCRIPTION "LDraw model editor for HarmonyOS PC"
        QT_HARMONYOS_MODULE_DEVICE_TYPES "2in1;tablet"
        QT_HARMONYOS_TARGET_ARCHS "arm64-v8a"
        QT_HARMONYOS_TARGET_SDK_VERSION "26.0.0"
        QT_HARMONYOS_COMPILE_SDK_VERSION "26.0.0"
    )
endif()

option(LEOCAD_BUILD_MODEL_TEST_HAP "Build the separate HarmonyOS model regression HAP" OFF)
if(OHOS AND LEOCAD_BUILD_MODEL_TEST_HAP)
    find_package(Qt6 6.12 REQUIRED COMPONENTS Test)
    set(_model_sources ${LEOCAD_SOURCES})
    list(REMOVE_ITEM _model_sources qt/qtmain.cpp)
    qt_add_executable(leocad-model-tests ${_model_sources} ${LEOCAD_HEADERS}
        ${LEOCAD_FORMS} tests/model/tst_model_io.cpp
        "${_generated_qrc}" resources/stylesheet/stylesheet.qrc)
    target_include_directories(leocad-model-tests PRIVATE common qt)
    target_compile_definitions(leocad-model-tests PRIVATE
        $<$<BOOL:${LEOCAD_SAFE_RENDER}>:LC_SAFE_RENDER=1>
        LC_DISABLE_UPDATE_CHECK=1 LC_DISABLE_NETWORK=1
        LC_DISABLE_RENDER_DIALOG=1 LC_DISABLE_BLENDER=1 LC_DISABLE_NON_CORE_FORMATS=1)
    target_link_libraries(leocad-model-tests PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets
        Qt6::OpenGL Qt6::OpenGLWidgets Qt6::Xml Qt6::Concurrent Qt6::PrintSupport
        Qt6::Svg Qt6::Test ZLIB::ZLIB)
    target_precompile_headers(leocad-model-tests PRIVATE common/lc_global.h)
    qt_add_resources(leocad-model-tests model_fixtures PREFIX "/fixtures"
        BASE tests/fixtures FILES tests/fixtures/two-pieces.ldr
        tests/fixtures/submodel.mpd tests/fixtures/empty-model.lxf)
    set_target_properties(leocad-model-tests PROPERTIES
        QT_HARMONYOS_APP_BUNDLE_NAME "io.github.xiaotaop.leocad.modeltests"
        QT_HARMONYOS_APP_NAME "LeoCAD Model Tests"
        QT_HARMONYOS_APP_LABEL "LeoCAD Model Tests"
        QT_HARMONYOS_APP_VENDOR "xiaotaop"
        QT_HARMONYOS_APP_VERSION_CODE 1 QT_HARMONYOS_APP_VERSION_NAME "0.1.0"
        QT_HARMONYOS_MODULE_DEVICE_TYPES "2in1;tablet"
        QT_HARMONYOS_TARGET_ARCHS "arm64-v8a"
        QT_HARMONYOS_TARGET_SDK_VERSION "26.0.0"
        QT_HARMONYOS_COMPILE_SDK_VERSION "26.0.0")
endif()

install(TARGETS leocad
    BUNDLE DESTINATION .
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)
