cmake_minimum_required(VERSION 3.22)

# Track version.txt as a configure input so changing it triggers reconfigure on build
configure_file(
    "${CMAKE_SOURCE_DIR}/version.txt"
    "${CMAKE_BINARY_DIR}/version.txt.copy"
    COPYONLY
)

file(READ "${CMAKE_BINARY_DIR}/version.txt.copy" APP_VERSION_RAW)
string(STRIP "${APP_VERSION_RAW}" APP_VERSION)

set(PROJECT_VER "${APP_VERSION}")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(esp32s3-led-pwm-blink)

set(FIRMWARE_ARTIFACT_NAME "${CMAKE_PROJECT_NAME}_v${PROJECT_VER}.bin")
set(APP_BIN_PATH "${CMAKE_BINARY_DIR}/${CMAKE_PROJECT_NAME}.bin")

# OTA slot is 1 MB = 0x100000 bytes
set(OTA_SLOT_SIZE 1048576)

# Script that will run after build
set(CHECK_AND_COPY_SCRIPT "${CMAKE_BINARY_DIR}/check_and_copy_firmware.cmake")

file(WRITE "${CHECK_AND_COPY_SCRIPT}" [=[
set(APP_BIN_PATH "@APP_BIN_PATH@")
set(ARTIFACT_DIR "@CMAKE_SOURCE_DIR@/artifacts")
set(ARTIFACT_PATH "${ARTIFACT_DIR}/@FIRMWARE_ARTIFACT_NAME@")
set(OTA_SLOT_SIZE @OTA_SLOT_SIZE@)

if(NOT EXISTS "${APP_BIN_PATH}")
    message(FATAL_ERROR "Firmware binary not found: ${APP_BIN_PATH}")
endif()

file(SIZE "${APP_BIN_PATH}" APP_BIN_SIZE)

message(STATUS "Firmware binary: ${APP_BIN_PATH}")
message(STATUS "Firmware size: ${APP_BIN_SIZE} bytes")
message(STATUS "OTA slot size: ${OTA_SLOT_SIZE} bytes")

if(APP_BIN_SIZE GREATER OTA_SLOT_SIZE)
    message(FATAL_ERROR "Firmware is too large for OTA slot. Expected <= ${OTA_SLOT_SIZE} bytes, got ${APP_BIN_SIZE} bytes.")
else()
    file(MAKE_DIRECTORY "${ARTIFACT_DIR}")
    file(COPY_FILE "${APP_BIN_PATH}" "${ARTIFACT_PATH}" ONLY_IF_DIFFERENT)
    message(STATUS "Firmware copied to: ${ARTIFACT_PATH}")
endif()
]=])

configure_file(
    "${CHECK_AND_COPY_SCRIPT}"
    "${CHECK_AND_COPY_SCRIPT}.configured"
    @ONLY
)

add_custom_command(
    TARGET app
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -P "${CHECK_AND_COPY_SCRIPT}.configured"
    COMMENT "Checking firmware size and copying OTA-ready artifact if it fits"
)