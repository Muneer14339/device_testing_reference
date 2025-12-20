# ============================================================================
# CMakeLists.txt - COMPLETE REPLACEMENT
# ============================================================================

cmake_minimum_required(VERSION 3.15)
project(recoil_tracker_ble CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SOURCES
    recoil_tracker.cpp
    imu_device_session.cpp
    imu_qa_manager.cpp
)

if(WIN32)
    list(APPEND SOURCES 
        imu_windows_ble_scanner.cpp
        imu_windows_connection.cpp
    )
    add_executable(recoil_tracker ${SOURCES})
    target_compile_definitions(recoil_tracker PRIVATE WINVER=0x0A00 _WIN32_WINNT=0x0A00)
    target_link_libraries(recoil_tracker PRIVATE windowsapp)
else()
    find_package(simpleble CONFIG REQUIRED)
    add_executable(recoil_tracker ${SOURCES})
    target_link_libraries(recoil_tracker PRIVATE simpleble::simpleble simpleble::simpleble-c)
    
    if(UNIX AND NOT APPLE)
        find_package(Threads REQUIRED)
        target_link_libraries(recoil_tracker PRIVATE dbus-1 Threads::Threads)
    endif()
endif()