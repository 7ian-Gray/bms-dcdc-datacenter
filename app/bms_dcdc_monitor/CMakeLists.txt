cmake_minimum_required(VERSION 3.16)

project(bms_dcdc_monitor VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

option(ENABLE_CONTROLCAN "Enable the Windows ControlCAN.dll backend" ${WIN32})

find_package(Qt6 REQUIRED COMPONENTS Widgets Charts SerialPort)

set(MONITOR_SOURCES
    src/main.cpp
    src/DataModel.h
    src/MockDataGenerator.cpp
    src/MockDataGenerator.h
    src/mainwindow.cpp
    src/mainwindow.h

    src/communication/CanFrame.h
    src/communication/ModbusRtuClient.cpp
    src/communication/ModbusRtuClient.h
    src/protocol/BmsCanParser.cpp
    src/protocol/BmsCanParser.h
)

if(WIN32 AND ENABLE_CONTROLCAN)
    list(APPEND MONITOR_SOURCES
        src/communication/ControlCanWorker.cpp
        src/communication/ControlCanWorker.h
        vendor/windows/ControlCAN.h
    )
endif()

add_executable(${PROJECT_NAME} ${MONITOR_SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE
    src
    src/communication
    src/protocol
    vendor/windows
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Widgets
    Qt6::Charts
    Qt6::SerialPort
)

if(WIN32 AND ENABLE_CONTROLCAN)
    target_compile_definitions(${PROJECT_NAME} PRIVATE BMS_HAS_CONTROLCAN=1)

    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/vendor/windows/ControlCAN.dll"
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>/ControlCAN.dll"
        COMMENT "Copying ControlCAN.dll beside the application executable"
    )
endif()
