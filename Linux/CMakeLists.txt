cmake_minimum_required(VERSION 3.10)
project(esp32_notifier LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt5 REQUIRED COMPONENTS Core Gui Widgets DBus Network)

add_executable(esp32-notifier
    main.cpp
    notification_listener.h
    notification_listener.cpp
    autostart_manager.h
    autostart_manager.cpp
    settings_dialog.h
    settings_dialog.cpp
)

target_link_libraries(esp32-notifier PRIVATE
    Qt5::Core
    Qt5::Gui
    Qt5::Widgets
    Qt5::DBus
    Qt5::Network
)
