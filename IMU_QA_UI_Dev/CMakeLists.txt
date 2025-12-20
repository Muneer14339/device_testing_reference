cmake_minimum_required(VERSION 3.15)
project(recoil_tracker_ble CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(simpleble REQUIRED)

add_executable(recoil_tracker
    recoil_tracker.cpp
    imu_device_session.cpp
    imu_qa_manager.cpp
)

target_link_libraries(recoil_tracker PRIVATE simpleble::simpleble simpleble::simpleble-c)

# Extra deps only on real Linux (BlueZ / dbus / pthread)
if(UNIX AND NOT APPLE)
    find_package(Threads REQUIRED)
    target_link_libraries(recoil_tracker PRIVATE simpleble::simpleble simpleble::simpleble-c)
endif()

