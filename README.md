# Custom FPV Quadcopter
A custom quadcopter built from scratch: STM32F405 flight controller PCB, embedded firmware, ESP-NOW wireless controller, and 3D-printed frame.
![Full CAD assembly](cad-isometric.png)
## Overview

This project is a 5-inch FPV quadcopter featuring a fully custom frame, electronics package, and firmware built from individual parts using university equipment and resources. At its core is a custom 4-layer flight controller PCB built around an STM32F405 microcontroller, interfacing with an IMU and barometer over SPI, I²C, and UART. The drone communicates wirelessly through ESP-NOW with a custom handheld controller, and the goal is to achieve stable flight with live video streaming from an onboard camera.

The motivation behind the project is to gain hands-on experience in PCB design, embedded systems, and industry tools including KiCad, SolidWorks, and STM32CubeIDE. In addition, I find flight and aerial technology fascinating as they draw multiple engineering concepts to life. Rather than using an off-the-shelf flight controller, designing the project from the ground up meant working through the full engineering process, integrating mechanical design, embedded systems, and software development.

The part I'm most proud of is the flight controller PCB. Designing it from a blank schematic meant making every engineering decision myself, including component selection, power architecture, sensor integration, and layout with attention to multi-rail power distribution and noise isolation, which are all choices and considerations crucial to the performance of the drone. 

Future goals: autonomous flight capabilities and a potential payload claw mechanism.

## System Architecture

The system is split into three independent subsystems: a handheld 3D-printed controller, the drone's flight and communication electronics, and a standalone video feed. The controller and drone communicate wirelessly over ESP-NOW, while the onboard ESP32-CAM streams video directly to a browser over its own Wi-Fi connection.

```mermaid
graph TD
    subgraph Controller["Handheld Controller"]
        JS["Joysticks + Switches"]
        OLED["OLED Display"]
        S3["ESP32-S3"]
        JS --> S3
        S3 --> OLED
    end

    subgraph Drone["Drone"]
        WROOM["ESP32-WROOM<br/>(Radio Link)"]
        STM["STM32F405<br/>(Flight Controller)"]
        IMU["IMU"]
        BARO["Barometer"]
        CURR["Current Sensor"]
        ESC["4-in-1 ESC"]
        MOTORS["4 Motors"]

        WROOM -->|UART| STM
        IMU -->|SPI| STM
        BARO -->|I2C| STM
        CURR -->|I2C / ADC| STM
        STM -->|DSHOT| ESC
        ESC --> MOTORS
    end

    subgraph Video["Video Feed"]
        CAM["ESP32-CAM"]
        BROWSER["Browser"]
        CAM -->|Wi-Fi| BROWSER
    end

    S3 <-->|ESP-NOW| WROOM
