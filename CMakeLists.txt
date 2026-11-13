cmake_minimum_required(VERSION 3.21)
project(NanoGo VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.4 REQUIRED COMPONENTS Widgets)

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
)

qt_add_executable(NanoGo ${SOURCES})
target_link_libraries(NanoGo PRIVATE Qt6::Widgets)
set_target_properties(NanoGo PROPERTIES WIN32_EXECUTABLE ON)

# Windows: 编译后自动把 Qt 的 dll 复制到 exe 旁边，方便直接双击运行
if(WIN32)
    get_target_property(_qmake Qt6::qmake IMPORTED_LOCATION)
    get_filename_component(_qtbin "${_qmake}" DIRECTORY)
    find_program(WINDEPLOYQT windeployqt HINTS "${_qtbin}")
    if(WINDEPLOYQT)
        add_custom_command(TARGET NanoGo POST_BUILD
            COMMAND "${WINDEPLOYQT}" --no-translations --no-compiler-runtime "$<TARGET_FILE:NanoGo>"
            COMMENT "复制 Qt 运行库")
    endif()
endif()
