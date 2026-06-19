cmake_minimum_required(VERSION 3.16)

project(bms_dcdc_monitor VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Charts SerialPort)

set(MONITOR_SOURCES
    src/main.cpp
    src/DataModel.h
    src/MockDataGenerator.cpp
    src/MockDataGenerator.h
    src/mainwindow.cpp
    src/mainwindow.h

    src/communication/CanFrame.h
    src/communication/CanFrameInputParser.cpp
    src/communication/CanFrameInputParser.h
    src/communication/CanSessionController.cpp
    src/communication/CanSessionController.h
    src/communication/MockCanSource.cpp
    src/communication/MockCanSource.h
    src/communication/ModbusTransport.cpp
    src/communication/ModbusTransport.h
    src/communication/MockModbusTransport.cpp
    src/communication/MockModbusTransport.h
    src/communication/ModbusRtuClient.cpp
    src/communication/ModbusRtuClient.h
    src/communication/QSerialPortModbusTransport.cpp
    src/communication/QSerialPortModbusTransport.h
    src/models/CanFrameFilterProxyModel.cpp
    src/models/CanFrameFilterProxyModel.h
    src/models/CanFrameTableModel.cpp
    src/models/CanFrameTableModel.h
    src/pages/CanMonitorPage.cpp
    src/pages/CanMonitorPage.h
    src/protocol/BmsCanParser.cpp
    src/protocol/BmsCanParser.h
    src/protocol/ModbusRtuFrame.cpp
    src/protocol/ModbusRtuFrame.h
    src/protocol/DcdcModbusRegisterMap.cpp
    src/protocol/DcdcModbusRegisterMap.h
)

add_executable(${PROJECT_NAME} ${MONITOR_SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE
    src
    src/communication
    src/pages
    src/protocol
    src/models
)

target_link_libraries(${PROJECT_NAME} PRIVATE Qt6::Widgets Qt6::Charts Qt6::SerialPort)

include(CTest)

if(BUILD_TESTING)
    find_package(Qt6 REQUIRED COMPONENTS Test)

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

    add_executable(can_frame_input_parser_tests
        tests/CanFrameInputParserTest.cpp
        src/communication/CanFrame.h
        src/communication/CanFrameInputParser.cpp
        src/communication/CanFrameInputParser.h
    )

    target_include_directories(can_frame_input_parser_tests PRIVATE
        src
        src/communication
    )

    target_link_libraries(can_frame_input_parser_tests PRIVATE
        Qt6::Test
    )

    add_test(NAME can_frame_input_parser_tests COMMAND can_frame_input_parser_tests)

    add_executable(can_session_controller_tests
        tests/CanSessionControllerTest.cpp
        src/communication/CanFrame.h
        src/communication/CanSessionController.cpp
        src/communication/CanSessionController.h
        src/communication/MockCanSource.cpp
        src/communication/MockCanSource.h
    )

    target_include_directories(can_session_controller_tests PRIVATE
        src
        src/communication
    )

    target_link_libraries(can_session_controller_tests PRIVATE
        Qt6::Test
    )

    add_test(NAME can_session_controller_tests COMMAND can_session_controller_tests)

    add_executable(modbus_rtu_frame_tests
        tests/ModbusRtuFrameTest.cpp
        src/protocol/ModbusRtuFrame.cpp
        src/protocol/ModbusRtuFrame.h
    )

    target_include_directories(modbus_rtu_frame_tests PRIVATE
        src
        src/protocol
    )

    target_link_libraries(modbus_rtu_frame_tests PRIVATE
        Qt6::Test
    )

    add_test(NAME modbus_rtu_frame_tests COMMAND modbus_rtu_frame_tests)

    add_executable(modbus_rtu_client_tests
        tests/ModbusRtuClientTest.cpp
        src/communication/ModbusTransport.cpp
        src/communication/ModbusTransport.h
        src/communication/MockModbusTransport.cpp
        src/communication/MockModbusTransport.h
        src/communication/ModbusRtuClient.cpp
        src/communication/ModbusRtuClient.h
        src/protocol/ModbusRtuFrame.cpp
        src/protocol/ModbusRtuFrame.h
    )

    target_include_directories(modbus_rtu_client_tests PRIVATE
        src
        src/communication
        src/protocol
    )

    target_link_libraries(modbus_rtu_client_tests PRIVATE
        Qt6::Test
        Qt6::Core
    )

    add_test(NAME modbus_rtu_client_tests COMMAND modbus_rtu_client_tests)

    add_executable(dcdc_modbus_register_map_tests
        tests/DcdcModbusRegisterMapTest.cpp
        src/protocol/DcdcModbusRegisterMap.cpp
        src/protocol/DcdcModbusRegisterMap.h
    )

    target_include_directories(dcdc_modbus_register_map_tests PRIVATE
        src
        src/protocol
    )

    target_link_libraries(dcdc_modbus_register_map_tests PRIVATE
        Qt6::Test
    )

    add_test(NAME dcdc_modbus_register_map_tests COMMAND dcdc_modbus_register_map_tests)

    add_executable(qserialport_modbus_transport_tests
        tests/QSerialPortModbusTransportTest.cpp
        src/communication/ModbusTransport.cpp
        src/communication/ModbusTransport.h
        src/communication/QSerialPortModbusTransport.cpp
        src/communication/QSerialPortModbusTransport.h
    )

    target_include_directories(qserialport_modbus_transport_tests PRIVATE
        src
        src/communication
    )

    target_link_libraries(qserialport_modbus_transport_tests PRIVATE
        Qt6::Test
        Qt6::SerialPort
    )

    add_test(NAME qserialport_modbus_transport_tests COMMAND qserialport_modbus_transport_tests)
endif()
