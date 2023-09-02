cmake_policy(SET CMP0048 NEW)
cmake_minimum_required(VERSION 3.13.0)

# if (NOT "$ENV{APPLICATION}" STREQUAL "")
#     set(APPLICATION $ENV{APPLICATION})
# else()
#     set(APPLICATION default_app_name)
# endif()

get_filename_component(APPLICATION ${CMAKE_CURRENT_SOURCE_DIR} NAME_WE)

if (NOT "$ENV{APPLICATION_NAME}" STREQUAL "")
    set(APPLICATION_NAME $ENV{APPLICATION_NAME})
else()
    set(APPLICATION_NAME ${APPLICATION})
endif()

set(TARGET "nm180100" CACHE STRING "" FORCE)

option(BSP_NM180100EVB "" OFF)
option(BSP_NM180310 "" OFF)

option(RAT_BLE_ENABLE "" ON)
option(RAT_LORAWAN_ENABLE "" ON)
option(TF_ENABLE "" OFF)

if (BSP_NM180100EVB)
add_definitions(-DBSP_NM180100EVB)
set(BSP_TARGET_DIR nm180100evb CACHE STRING "" FORCE)
endif()

if (BSP_NM180310)
add_definitions(-DBSP_NM180310)
set(BSP_TARGET_DIR nm180310 CACHE STRING "" FORCE)
endif()

add_subdirectory(nmsdk2)

project(${APPLICATION})

add_executable(${APPLICATION})
set_target_properties(
    ${APPLICATION}
    PROPERTIES
        OUTPUT_NAME ${APPLICATION_NAME}
        SUFFIX ".axf"
)

add_subdirectory(bsp)

if (RAT_BLE_ENABLE)
    message("BLE enabled")
    set(BLE_DEFINES
        -DRAT_BLE_ENABLE
    )

    set(BLE_INCLUDES
        ${BLE_INCLUDES}
        ${PROJECT_SOURCE_DIR}/comms/ble
        ${PROJECT_SOURCE_DIR}/comms/ble/amota
        ${PROJECT_SOURCE_DIR}/comms/ble/amota/profile
        ${PROJECT_SOURCE_DIR}/comms/ble/amota/service
    )

    set(BLE_SOURCES
        comms/ble/ble_stack.c
        comms/ble/ble_task_cli.c
        comms/ble/ble_task.c
        comms/ble/amota/amota_main.c
        comms/ble/amota/profile/amotas_main.c
        comms/ble/amota/service/svc_amotas.c
    )

    set(BLE_LIBS
        ble
    )
endif()

if (RAT_LORAWAN_ENABLE)
    message("LoRaWAN enabled")
    set(LORAWAN_DEFINES
        -DRAT_LORAWAN_ENABLE
        -DSOFT_SE
        -DCONTEXT_MANAGEMENT_ENABLED
    )

    set(LORAWAN_INCLUDES
        ${LORAWAN_INCLUDES}
        ${PROJECT_SOURCE_DIR}/comms/lorawan
        ${PROJECT_SOURCE_DIR}/comms/lorawan/soft-se
    )

    set(LORAWAN_SOURCES
        comms/lorawan/lmh_callbacks.c
        comms/lorawan/lmhp_fragmentation.c
        comms/lorawan/lorawan_se.c
        comms/lorawan/lorawan_task_cli.c
        comms/lorawan/lorawan_task.c
        comms/lorawan/soft-se/aes.c
        comms/lorawan/soft-se/cmac.c
        comms/lorawan/soft-se/soft-se.c
    )

    set(LORAWAN_LIBS
        lorawan
    )
endif()

if (TF_ENABLE)
    message("Tensorflow enabled")
    set(TF_DEFINES
        -DTF_ENALBE
        -DGEMMLOWP_ALLOW_SLOW_SCALAR_FALLBACK
        -DTF_LITE_STATIC_MEMORY
        -DTF_LITE_MCU_DEBUG_LOG
        -DARM_MATH_CM4
    )

    set(TF_INCLUDES
        ${TFLM_INCLUDES}
    )

    set(TF_LIBS
        tflm
    )
endif()

target_link_libraries(
    ${APPLICATION}
    PUBLIC
        -T${PROJECT_SOURCE_DIR}/AMA3B1KK.ld
        -static
        -Wl,--start-group
        -L${CMSIS_LIB_DIR}
        ${BLE_LIBS}
        ${LORAWAN_LIBS}
        ${TF_LIBS}
        rtos
        bsp
        hal
        -lm
        -lc
        -lgcc
        -lCMSISDSP
        -lcmsis-nn
        -Wl,--end-group
        -Wl,--gc-sections
)

target_compile_definitions(
    ${APPLICATION}
    PRIVATE
    ${BLE_DEFINES}
    ${LORAWAN_DEFINES}
    ${TF_DEFINES}
)

target_include_directories(
    ${APPLICATION}
    PRIVATE
    ${HAL_INCLUDES}
    ${RTOS_INCLUDES}
    ${BLE_INCLUDES}
    ${LORAWAN_INCLUDES}
    ${TF_INCLUDES}
    ${PROJECT_SOURCE_DIR}/bsp
    ${PROJECT_SOURCE_DIR}/bsp/${BSP_TARGET_DIR}
    ${PROJECT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/config
    ${PROJECT_SOURCE_DIR}/utils/bootloader
    ${PROJECT_SOURCE_DIR}/utils/RTT/Config
    ${PROJECT_SOURCE_DIR}/utils/RTT/RTT
)

target_sources(
    ${APPLICATION}
    PRIVATE
    startup_gcc.c
    main.c
    stub.c
    application_task_cli.c
    application_task.c
    console_task.c

    utils/bootloader/am_bootloader.c
    utils/bootloader/am_multi_boot.c

    utils/RTT/RTT/SEGGER_RTT.c
    utils/RTT/RTT/SEGGER_RTT_printf.c

    ${BLE_SOURCES}
    ${LORAWAN_SOURCES}
)

add_custom_command(
    TARGET ${APPLICATION}
    POST_BUILD
    # COMMAND echo "Creating list file"
    # COMMAND ${CMAKE_OBJDUMP} -S $<TARGET_FILE_NAME:${APPLICATION}> > $<TARGET_FILE_BASE_NAME:${APPLICATION}>.lst
    COMMAND echo "Creating bin file"
    COMMAND ${CMAKE_OBJCOPY} -Obinary $<TARGET_FILE_NAME:${APPLICATION}> $<TARGET_FILE_BASE_NAME:${APPLICATION}>.bin
)

add_dependencies(${APPLICATION} hal bsp rtos ${BLE_LIBS} ${LORAWAN_LIBS} ${TF_LIBS})