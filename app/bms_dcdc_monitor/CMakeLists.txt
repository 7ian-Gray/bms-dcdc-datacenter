cmake_minimum_required(VERSION 3.16)

project(bms_dcdc_monitor VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Charts)

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
    src/models/CanFrameFilterProxyModel.cpp
    src/models/CanFrameFilterProxyModel.h
    src/models/CanFrameTableModel.cpp
    src/models/CanFrameTableModel.h
    src/protocol/BmsCanParser.cpp
    src/protocol/BmsCanParser.h
)

add_executable(${PROJECT_NAME} ${MONITOR_SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE
    src
    src/communication
    src/protocol
    src/models
)

target_link_libraries(${PROJECT_NAME} PRIVATE Qt6::Widgets Qt6::Charts)

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
endif()
