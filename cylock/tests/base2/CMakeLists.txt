# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(spi_flash)

FILE(GLOB app_sources src/*.c)
target_sources(app PRIVATE ${app_sources})


# add gnss_module (the unit under test)
target_sources(app PRIVATE ../../src/FlashSPI.c)
target_sources(app PRIVATE ../../src/AES_CTR.c)
target_sources(app PRIVATE ../../src/PIN_CTRL.c)
target_sources(app PRIVATE ../../src/MQTT_Queue.c)
target_sources(app PRIVATE ../../src/Com_State_Machine.c)
target_sources(app PRIVATE ../../src/bg95.c)
target_sources(app PRIVATE ../../src/Comm_Mgr.c)
target_sources(app PRIVATE ../../src/thuraya.c)
target_sources(app PRIVATE ../../src/GPS.c)
target_sources(app PRIVATE ../../src/lock.c)
target_sources(app PRIVATE ../../src/NRF_SM.c)
target_sources(app PRIVATE ../../src/BLE_mgr.c)




target_include_directories(app PRIVATE .)
target_include_directories(app PRIVATE ../../src/)
