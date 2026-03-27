cmake_minimum_required(VERSION 3.21)

get_filename_component(PROJECT_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)
project(${PROJECT_NAME} VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

list(PREPEND CMAKE_PREFIX_PATH
    "D:/C++Library/spdlog-1.17.0/build/install/lib/cmake/spdlog"
    "D:/C++Library/json-3.12.0/build/install/share/cmake/nlohmann_json"
    "D:/C++Library/openvino_toolkit_windows_2026.0.0.20965.c6d6a13a886_x86_64/runtime/cmake"
    "D:/C++Library/opencv_puls-4.12.0/x64/vc17/lib"
)

if(MSVC)
    add_compile_options(/utf-8)
endif()

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Charts Sql Network SerialBus SerialPort)

if(QT_VERSION_MAJOR GREATER_EQUAL 6)
    find_package(Qt6 REQUIRED COMPONENTS Core)
endif()

find_package(spdlog CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(OpenCV REQUIRED COMPONENTS world)

# ── HIK MVS SDK ──────────────────────────────────────────────
set(MVS_SDK_DIR "D:/MVS/Development")
set(MVS_INCLUDE_DIR "${MVS_SDK_DIR}/Includes")
set(MVS_LIB_DIR    "${MVS_SDK_DIR}/Libraries/win64")

# ── HACLON SDK ──────────────────────────────────────────────
set(HACLON_SDK_DIR "D:/C++Library/haclon-25.11SDK")
set(HACLON_INCLUDE_DIR "${HACLON_SDK_DIR}/include")
set(HACLON_LIB_DIR    "${HACLON_SDK_DIR}/lib/x64-win64")

# ── Source files ─────────────────────────────────────────────
set(PROJECT_SOURCES
    # app
    src/app/main.cpp
    src/app/config_manager.h src/app/config_manager.cpp
    src/app/app_context.h    src/app/app_context.cpp
    src/app/common.h
    resource.qrc

    # ui
    src/ui/main_window.h  src/ui/main_window.cpp  src/ui/main_window.ui
    src/ui/ioparam_dialog.h   src/ui/ioparam_dialog.cpp   src/ui/ioparam_dialog.ui
    src/ui/camera_view_widget.h src/ui/camera_view_widget.cpp src/ui/camera_view_widget.ui
    src/ui/camera_param_dialog.h src/ui/camera_param_dialog.cpp src/ui/camera_param_dialog.ui
    src/ui/toolbox_widget.h       src/ui/toolbox_widget.cpp       src/ui/toolbox_widget.ui
    src/ui/workflow_view_widget.h src/ui/workflow_view_widget.cpp src/ui/workflow_view_widget.ui
    src/ui/operation_view_widget.h src/ui/operation_view_widget.cpp src/ui/operation_view_widget.ui

    # camera
    src/camera/camera_interface.h
    src/camera/hik_camera.h       src/camera/hik_camera.cpp
    src/camera/camera_manager.h   src/camera/camera_manager.cpp

    # communication
    src/communication/modbus_client_base.h  src/communication/modbus_client_base.cpp
    src/communication/modbus_tcp_client.h   src/communication/modbus_tcp_client.cpp
    src/communication/modbus_rtu_client.h   src/communication/modbus_rtu_client.cpp
    src/communication/comm_manager.h        src/communication/comm_manager.cpp
    src/communication/light_serial_controller.h src/communication/light_serial_controller.cpp

    # workflow
    src/workflow/workflow_manager.h      src/workflow/workflow_manager.cpp
    src/workflow/inspection_pipeline.h   src/workflow/inspection_pipeline.cpp
    src/workflow/nodes/node_base.h
    src/workflow/nodes/capture_node.h    src/workflow/nodes/capture_node.cpp
    src/workflow/nodes/algorithm_node.h  src/workflow/nodes/algorithm_node.cpp
    src/workflow/nodes/result_node.h     src/workflow/nodes/result_node.cpp

    # algorithm
    src/algorithm/algorithm_interface.h
    src/algorithm/algorithm_factory.h    src/algorithm/algorithm_factory.cpp

    # ai_infer
    # src/ai_infer/ai_utils.h  src/ai_infer/ai_common.h     src/ai_infer/infer_backend.h
    # src/ai_infer/preprocess.h src/ai_infer/preprocess.cpp
    # src/ai_infer/postprocess.h src/ai_infer/postprocess.cpp
    # src/ai_infer/yolo.h src/ai_infer/yolo.cpp
    # src/ai_infer/openvino_infer.h src/ai_infer/openvino_infer.cpp


    # storage
    # src/storage/database_manager.h  src/storage/database_manager.cpp
    # src/storage/image_storage.h     src/storage/image_storage.cpp
)

if(QT_VERSION_MAJOR GREATER_EQUAL 6)
    qt_add_executable(${PROJECT_NAME} WIN32
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
    )
else()
    target_link_libraries(${PROJECT_NAME} PRIVATE Qt6::Core)
    add_executable(${PROJECT_NAME} ${PROJECT_SOURCES})
endif()

if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE
        $<$<CONFIG:Debug>:/Zi /RTC1>
        $<$<CONFIG:Release>:/O2 /arch:AVX2 /fp:fast>
    )
endif()

target_compile_definitions(${PROJECT_NAME} PRIVATE
    NOMINMAX
    WIN32_LEAN_AND_MEAN
    _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING
    _CRT_SECURE_NO_WARNINGS
    OPENCV_DISABLE_DEBUG_BUILD_GUARD
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${MVS_INCLUDE_DIR}
    ${HACLON_INCLUDE_DIR}
    D:/C++Library/taskflow-4.0.0
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Charts
    Qt${QT_VERSION_MAJOR}::Sql
    Qt${QT_VERSION_MAJOR}::Network
    Qt${QT_VERSION_MAJOR}::SerialBus
    Qt${QT_VERSION_MAJOR}::SerialPort
    Qt6::Core
    spdlog::spdlog
    nlohmann_json::nlohmann_json
    ${OpenCV_LIBS}
    ${MVS_LIB_DIR}/MvCameraControl.lib
    ${HACLON_LIB_DIR}/halconcpp.lib
)

    # ── Copy data to exe dir ──
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/data"
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
    COMMENT "Copy config.json to exe dir"
)

if(WIN32)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_RUNTIME_DLLS:${PROJECT_NAME}> $<TARGET_FILE_DIR:${PROJECT_NAME}>
        COMMAND_EXPAND_LISTS
        COMMENT "Copy linked runtime DLLs to the exe dir"
    )

    # ── OpenCV DLLs ──
    set(_opencv_bin_dir "${OpenCV_DIR}/../bin")
    file(GLOB _opencv_dlls "${_opencv_bin_dir}/opencv_world*.dll")
    if(_opencv_dlls)
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    ${_opencv_dlls} $<TARGET_FILE_DIR:${PROJECT_NAME}>
            COMMENT "Copy OpenCV DLLs"
        )
    endif()

    # ── Halcon DLLs ──
    set(_halcon_bin_dir "${HACLON_SDK_DIR}/dll")
    file(GLOB _halcon_dlls "${_halcon_bin_dir}/halcon*.dll")
    if(_halcon_dlls)
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    ${_halcon_dlls} $<TARGET_FILE_DIR:${PROJECT_NAME}>
            COMMENT "Copy Halcon DLLs"
        )
    endif()

    # ── HIK MVS DLLs ──
    set(_mvs_bin_dir "C:/Program Files (x86)/Common Files/MVS/Runtime/Win64_x64")
    file(GLOB _mvs_dlls "${_mvs_bin_dir}/*.dll")
    if(_mvs_dlls)
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    ${_mvs_dlls} $<TARGET_FILE_DIR:${PROJECT_NAME}>
            COMMENT "Copy HIK MVS DLLs"
        )
    endif()

    if(TARGET Qt6::QWindowsIntegrationPlugin)
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${PROJECT_NAME}>/platforms"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    $<TARGET_FILE:Qt6::QWindowsIntegrationPlugin>
                    "$<TARGET_FILE_DIR:${PROJECT_NAME}>/platforms/"
            COMMENT "Copy Qt platform plugin: qwindows.dll"
        )
    endif()

    if(TARGET Qt6::QModernWindowsStylePlugin)
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${PROJECT_NAME}>/styles"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    $<TARGET_FILE:Qt6::QModernWindowsStylePlugin>
                    "$<TARGET_FILE_DIR:${PROJECT_NAME}>/styles/"
            COMMENT "Copy Qt style plugin"
        )
    endif()

    set(_qt_sql_plugins
        $<$<TARGET_EXISTS:Qt6::QSQLiteDriverPlugin>:$<TARGET_FILE:Qt6::QSQLiteDriverPlugin>>
    )
    if(_qt_sql_plugins)
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${PROJECT_NAME}>/sqldrivers"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    ${_qt_sql_plugins} "$<TARGET_FILE_DIR:${PROJECT_NAME}>/sqldrivers/"
            COMMAND_EXPAND_LISTS
            COMMENT "Copy SQL driver plugins"
        )
    endif()
endif()

if(QT_VERSION VERSION_LESS 6.1.0)
    set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.${PROJECT_NAME})
endif()

set_target_properties(${PROJECT_NAME} PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE FALSE
)

include(GNUInstallDirs)
install(TARGETS ${PROJECT_NAME}
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(${PROJECT_NAME})
endif()
