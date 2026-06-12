cmake_minimum_required(VERSION 3.16)

project(bms_dcdc_monitor VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

option(ENABLE_CONTROLCAN "Enable the Windows ControlCAN.dll backend" ${WIN32})

find_package(Qt6 REQUIRED COMPONENTS Widgets Charts SerialPort Test)

set(MONITOR_SOURCES
    src/main.cpp
    src/DataModel.h
    src/MockDataGenerator.cpp
    src/MockDataGenerator.h
    src/mainwindow.cpp
    src/mainwindow.h

    src/communication/CanFrame.h
    src/communication/MockCanSource.cpp
    src/communication/MockCanSource.h
    src/communication/ModbusRtuClient.cpp
    src/communication/ModbusRtuClient.h
    src/models/CanFrameFilterProxyModel.cpp
    src/models/CanFrameFilterProxyModel.h
    src/models/CanFrameTableModel.cpp
    src/models/CanFrameTableModel.h
    src/pages/CanMonitorPage.cpp
    src/pages/CanMonitorPage.h
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

include(CTest)

if(BUILD_TESTING)
    add_executable(bms_can_parser_tests
        tests/BmsCanParserTest.cpp
        src/DataModel.h
        src/communication/CanFrame.h
        src/protocol/BmsCanParser.cpp
        src/protocol/BmsCanParser.h
    )

    target_include_directories(bms_can_parser_tests PRIVATE
        src
        src/communication
        src/protocol
    )

    target_link_libraries(bms_can_parser_tests PRIVATE
        Qt6::Test
    )

    add_test(NAME bms_can_parser_tests COMMAND bms_can_parser_tests)

    add_executable(modbus_rtu_client_tests
        tests/ModbusRtuClientTest.cpp
        src/communication/ModbusRtuClient.cpp
        src/communication/ModbusRtuClient.h
    )

    target_include_directories(modbus_rtu_client_tests PRIVATE
        src
        src/communication
    )

    target_link_libraries(modbus_rtu_client_tests PRIVATE
        Qt6::SerialPort
        Qt6::Test
    )

    add_test(NAME modbus_rtu_client_tests COMMAND modbus_rtu_client_tests)

    add_executable(can_monitor_tests
        tests/CanFrameTableModelTest.cpp
        src/communication/CanFrame.h
        src/models/CanFrameFilterProxyModel.cpp
        src/models/CanFrameFilterProxyModel.h
        src/models/CanFrameTableModel.cpp
        src/models/CanFrameTableModel.h
    )

    target_include_directories(can_monitor_tests PRIVATE
        src
        src/communication
        src/models
    )

    target_link_libraries(can_monitor_tests PRIVATE
        Qt6::Widgets
        Qt6::Test
    )

    add_test(NAME can_monitor_tests COMMAND can_monitor_tests)
endif()
