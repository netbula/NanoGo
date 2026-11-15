cmake_minimum_required(VERSION 3.21)
project(NanoGo VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.4 REQUIRED COMPONENTS Widgets Network)

if(MSVC)
    # 源码里有中文字符串，必须按 UTF-8 编译
    add_compile_options(/utf-8 /W3)
endif()

set(SOURCES
    src/main.cpp
    src/Board.h src/Board.cpp
    src/GameTree.h src/GameTree.cpp
    src/Sgf.h src/Sgf.cpp
    src/AppSettings.h src/AppSettings.cpp
    src/KataGoEngine.h src/KataGoEngine.cpp
    src/BoardWidget.h src/BoardWidget.cpp
    src/SettingsDialog.h src/SettingsDialog.cpp
    src/MainWindow.h src/MainWindow.cpp
    src/ModelUpdater.h src/ModelUpdater.cpp
    src/EngineInstaller.h src/EngineInstaller.cpp
    src/ModelChooser.h src/ModelChooser.cpp
    src/Icons.h src/Icons.cpp
    src/Review.h src/Review.cpp
    src/WinrateGraph.h src/WinrateGraph.cpp
    src/Tsumego.h src/Tsumego.cpp src/MainWindowTsumego.cpp
    src/I18n.h src/I18n.cpp
    src/HardwareInfo.h src/HardwareInfo.cpp
)

qt_add_executable(NanoGo ${SOURCES})
target_link_libraries(NanoGo PRIVATE Qt6::Widgets Qt6::Network)
# 按钮图示：Lucide 图标（ISC 许可证）预先渲染成 PNG，编进程序
file(GLOB NANOGO_ICON_PNGS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} res/icons/*.png)
qt_add_resources(NanoGo "icons" PREFIX "/" BASE res FILES ${NANOGO_ICON_PNGS})
qt_add_resources(NanoGo "tsumego" PREFIX "/" BASE res FILES res/tsumego/basic.sgf)
set_target_properties(NanoGo PROPERTIES WIN32_EXECUTABLE ON)
if(WIN32)
    target_link_libraries(NanoGo PRIVATE dxgi)
    # exe 文件图标（资源管理器、桌面快捷方式里显示）。rc 文件里写绝对路径，避免找不到 ico
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/nanogo_icon.rc" "IDI_ICON1 ICON \"${CMAKE_CURRENT_SOURCE_DIR}/res/nanogo.ico\"\n")
    target_sources(NanoGo PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/nanogo_icon.rc")
endif()

# Windows: 编译后自动把 Qt 的 dll 复制到 exe 旁边，方便直接双击运行
if(WIN32)
    get_target_property(_qmake Qt6::qmake IMPORTED_LOCATION)
    get_filename_component(_qtbin "${_qmake}" DIRECTORY)
    find_program(WINDEPLOYQT windeployqt HINTS "${_qtbin}")
    if(WINDEPLOYQT)
        add_custom_command(TARGET NanoGo POST_BUILD
            COMMAND "${WINDEPLOYQT}" --no-compiler-runtime "$<TARGET_FILE:NanoGo>"
            COMMENT "复制 Qt 运行库")
    endif()
endif()
