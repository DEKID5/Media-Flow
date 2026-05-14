cmake_minimum_required(VERSION 3.21)
project(MediaFlow VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Widgets Qml Quick QuickControls2 Sql Multimedia Concurrent)

set(MFQ_SOURCES
    src/main.cpp
    src/BroadcastController.cpp
    src/BroadcastController.h
    src/JwLibraryWorker.cpp
    src/JwLibraryWorker.h
    src/MediaExtractor.cpp
    src/MediaExtractor.h
    src/MediaLibraryModel.cpp
    src/MediaLibraryModel.h
    src/MeetingScheduleModel.cpp
    src/MeetingScheduleModel.h
    src/CameraDeviceModel.cpp
    src/CameraDeviceModel.h
    src/FfmpegUdpBridge.cpp
    src/FfmpegUdpBridge.h
    src/MediaAsset.h
    src/BroadcastEngine.cpp
    src/BroadcastEngine.h
    src/MediaThumbnailManager.cpp
    src/StagedMediaProxyModel.cpp
    src/StagedMediaProxyModel.h
    src/SongManager.cpp
    src/SongManager.h
    src/SongSearchUtils.cpp
    src/SongSearchUtils.h
    src/TimerController.cpp
    src/TimerController.h
    src/MediaLibraryProxyModel.cpp
    src/MediaLibraryProxyModel.h
    src/MediaScanWorker.cpp
    src/MediaScanWorker.h
)

if(WIN32)
    list(APPEND MFQ_SOURCES src/zoomhotkey_win.cpp src/zoomhotkey_win.h)
endif()

qt_add_executable(mediaflow ${MFQ_SOURCES})
set_target_properties(mediaflow PROPERTIES OUTPUT_NAME "MediaFlow")

qt_add_qml_module(mediaflow
    URI MediaFlow
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/Theme.qml
        qml/OperatorDashboard.qml
        qml/AudienceWindow.qml
        qml/components/PillButton.qml
        qml/components/MonitorView.qml
        qml/components/SongSelectorPopup.qml
        qml/components/SongSelector.qml
        qml/components/MediaSourcePanel.qml
        qml/components/BroadcastIcon.qml
        qml/components/TimerPanel.qml
        qml/assets/cam_placeholder.png
        qml/assets/meeting_placeholder.png
        qml/assets/video_placeholder.png
        qml/assets/logo.png
)
set_source_files_properties(qml/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

target_include_directories(mediaflow PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_libraries(mediaflow PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Sql
    Qt6::Multimedia
    Qt6::Concurrent
)

if(WIN32)
    target_link_libraries(mediaflow PRIVATE user32)
endif()

include(GNUInstallDirs)
install(TARGETS mediaflow RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
