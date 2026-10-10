cmake_minimum_required(VERSION 3.21)
project(MediaFlow VERSION 1.0.0 LANGUAGES CXX C) # C needed for vendored third_party/miniz

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Widgets Qml Quick QuickControls2 Sql Multimedia Concurrent Network)

set(MFQ_SOURCES
    src/main.cpp
    src/BroadcastController.cpp
    src/BroadcastController.h
    src/MediaExtractor.cpp
    src/MediaExtractor.h
    src/MediaLibraryModel.cpp
    src/MediaLibraryModel.h
    src/MeetingScheduleModel.cpp
    src/MeetingScheduleModel.h
    src/CameraDeviceModel.cpp
    src/CameraDeviceModel.h
    src/MediaAsset.h
    src/BroadcastEngine.cpp
    src/BroadcastEngine.h
    src/MediaThumbnailManager.cpp
    src/StagedMediaProxyModel.cpp
    src/StagedMediaProxyModel.h
    src/PinnedFolderModel.cpp
    src/PinnedFolderModel.h
    src/SongSearchUtils.cpp
    src/SongSearchUtils.h
    src/TimerController.cpp
    src/TimerController.h
    src/VirtualCameraManager.cpp
    src/VirtualCameraManager.h
    src/ObsVirtualCamWriter.cpp
    src/ObsVirtualCamWriter.h
    src/WorkbookManager.cpp
    src/WorkbookManager.h
    src/JwLibraryPaths.cpp
    src/JwLibraryPaths.h
    third_party/miniz/miniz.c
    third_party/miniz/miniz_tdef.c
    third_party/miniz/miniz_tinfl.c
    third_party/miniz/miniz_zip.c
)

if(WIN32)
    list(APPEND MFQ_SOURCES src/zoomhotkey_win.cpp src/zoomhotkey_win.h)
    # Embeds qml/assets/icon.ico into MediaFlow.exe (taskbar/Explorer/Alt-Tab).
    list(APPEND MFQ_SOURCES resources/app.rc)
endif()

# WIN32_EXECUTABLE explicitly, not left to qt_add_executable's own default --
# confirmed live that without it the linked exe still came out as a console
# (CUI) subsystem app, which is what put a raw terminal window (showing
# ffmpeg's own stderr spam, since child processes share the parent's
# console) on screen for the whole app runtime in production. All logging
# already goes to app_log.txt (see myMessageHandler in main.cpp), not
# stdout/stderr, so nothing depends on having a console.
qt_add_executable(mediaflow ${MFQ_SOURCES})
set_target_properties(mediaflow PROPERTIES OUTPUT_NAME "MediaFlow" WIN32_EXECUTABLE TRUE)

# Must be set BEFORE qt_add_qml_module() -- Qt's qmldir generator reads this
# property when the module is declared, not retroactively. Setting it after
# (as this file previously did) silently produces a qmldir entry missing the
# "singleton" keyword, which makes every Theme.<property> access in QML
# resolve to undefined instead of erroring -- the actual root cause behind
# this codebase's old defensive `typeof Theme !== "undefined"` fallback.
set_source_files_properties(qml/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(mediaflow
    URI MediaFlow
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/SplashScreen.qml
        qml/Theme.qml
        qml/OperatorDashboard.qml
        qml/AudienceWindow.qml
        qml/VirtualCameraWindow.qml
        qml/SettingsWindow.qml
        qml/TimerWindow.qml
        qml/components/PillButton.qml
        qml/components/MonitorView.qml
        qml/components/SongSelectorPopup.qml
        qml/components/MediaSourcePanel.qml
        qml/components/BroadcastIcon.qml
        qml/components/DockPanel.qml
        qml/components/ThemedDialog.qml
        qml/components/TimerPanel.qml
        qml/components/BackgroundMusicPanel.qml
        qml/assets/cam_placeholder.png
        qml/assets/meeting_placeholder.png
        qml/assets/video_placeholder.png
        qml/assets/logo.png
)

target_include_directories(mediaflow PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/miniz
)

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
    Qt6::OpenGL
    Qt6::Network
)

if(WIN32)
    # winmm: timeBeginPeriod/timeEndPeriod (see main.cpp -- 1ms timer
    # resolution for the app's lifetime, to avoid audio stutter when the
    # process loses foreground focus).
    target_link_libraries(mediaflow PRIVATE user32 Shell32 Ole32 winmm)
endif()

include(GNUInstallDirs)
install(TARGETS mediaflow RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
