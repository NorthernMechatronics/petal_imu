# Petal IMU Demo

## Introduction

This application demonstrates the usage of the Bosch BMI270 IMU and the BMM350 magnetometer on
the IMU Petal plugin board.

## Requirements

### Hardware

- A Petal Development Board
- A Petal Core Board
- A Petal IMU Board
- A USB C cable
- A Linux or Windows machine

### Software

- ARM GNU Toolchain Compiler
- pyOCD
- make
- cmake
- Python
- Visual Studio Code

## Quick Start

1. Select the compiler version for your system as shown in the screenshot. If
   none appears in the list, try the `Scan for Kits` option or follow the
   <a href="https://github.com/NorthernMechatronics/nmapp2/blob/master/doc/getting_started.md">
   Getting Started</a> guide in nmapp2 to ensure that
   the ARM compiler is installed properly.
   ![compiler_selection](./doc/compiler_selection.png)

2. Select PETAL_IMU Debug or Release as the build variant.
   ![variant_selection](./doc/variant_selection.png)

3. Click the CMAKE extension icon in the Activity Bar on the left and
   in the primary sidebar, move your mouse cursor to Project Outline and click More
   Actions (denoted by the three dots ...) to expand the menu. Click on
   `Clean Reconfigure All Projects`.
   ![cmake_configure](./doc/cmake_configure_completed.png)

4. Once the configuration process is completed, click on the Build All Projects
   icon in Project Outline. For a clean re-build, you can also select
   `Clean Rebuild All Projects`
   ![cmake_configure](./doc/cmake_build_completed.png)

5. Once the build is completed, click on Run and Debug in the Activity Bar on the left and
   select Petal IMU Debug or Release as the run variant.
   ![cmake_configure](./doc/run_debug_variant_selection.png)

6. Click the play button to load and run the program. Once the board is booted, you should
   see the following in a serial terminal.
   ![cmake_configure](./doc/terminal_output.png)

## Code Description

This application provides a framework that the user can customized for specific
scenarios.  The code is organized into three layers.  The driver layer contains
driver code for both the IMU and the magnetometer and is located in `bsp/drivers`.
Above the driver is the hardware abstraction layer which provides high-level
sensor configurations.  These code are located under `motion`.  Finally, the application
layer which is implemented by the set of files prefixed with `application_` in the root
directory of the project.

A number of features are provided in the application layer to allow the user to
quickly implement their solution:

* In `application_task.c`, a message loop framework is provided to
demonstrate how to sample, read, and leverage the sensor data.  The
sampling rate is adjustable by the API `application_setup_sensors`
in `application_sensors.c` allowing the user to accommmodate different
algorithm requirements.

* Hard iron and isotropic soft iron calibration.

* Persistent storage solution for the magnetometer calibration data.
This is implemented with the little filesystem (lfs).

* An example power savings implementation that make use of the no motion
detection feature on the BMI270.  When no motion is detected for more than
2s, sampling is disabled to conserve power.  Sampling is resumed as soon
as motion occurs.  The user can configure the definition of no motion in 
the function `imu_feature_config_no_motion`.  User can also disable
this feature by setting `sampling_always_on` to 1 in `application_task.c`.

* In the current implementation, only the interrupt1 pin is used on the
BMI270.  This can be modified in `imu_interrupt_config` located in
`motion/imu.c`.

Regarding wireless communication, only the LoRaWAN communication stack is
enabled by default (`RAT_LORAWAN_ENABLE=ON`). To enable other radio access
technology (RAT) such as BLE, set `RAT_BLE_ENABLE` to `ON` in
`settings.json` in the `.vscode` directory for more details.
