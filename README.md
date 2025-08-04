# The IoT Clock and Room Monitor
Author: Ivan Shtuka (is222zf)

# Table of Contents
- [The IoT Clock and Room Monitor](#the-iot-clock-and-room-monitor)
- [Table of Contents](#table-of-contents)
- [Introduction](#introduction)
- [Objective](#objective)
- [Material required](#material-required)
- [Setup](#setup)
  - [Install the IDE](#install-the-ide)
    - [VSCode](#vscode)
    - [Espressif-IDE](#espressif-ide)
  - [Install the prerequisites (VSCode only)](#install-the-prerequisites-vscode-only)
    - [WSL2 (Windows)](#wsl2-windows)
      - [Configure udev rules](#configure-udev-rules)
      - [Configure usbipd-win](#configure-usbipd-win)
    - [Docker Desktop](#docker-desktop)
    - [Docker Desktop \& VSCode integration](#docker-desktop--vscode-integration)
  - [Setup project using VSCode](#setup-project-using-vscode)
    - [Clone the project](#clone-the-project)
    - [Open the project in a container](#open-the-project-in-a-container)
    - [Configure the project](#configure-the-project)
    - [Build the project](#build-the-project)
    - [Flash the device](#flash-the-device)
- [Putting everything together](#putting-everything-together)
  - [Circuit Diagram](#circuit-diagram)
    - [Sensors](#sensors)
    - [Displays](#displays)
    - [Complete](#complete)
  - [Power Consumption](#power-consumption)
  - [Main system elements](#main-system-elements)
    - [The configuration settings used for tasks:](#the-configuration-settings-used-for-tasks)
    - [Periodic actions of each task:](#periodic-actions-of-each-task)
  - [System Initialization](#system-initialization)
  - [Sensor Tasks](#sensor-tasks)
    - [Air Quality Measurement task](#air-quality-measurement-task)
    - [Sound Sensor Sampling task](#sound-sensor-sampling-task)
    - [TSL2591 Sampling Task](#tsl2591-sampling-task)
    - [SNTP Sampling Task](#sntp-sampling-task)
  - [Data Aggregation Task](#data-aggregation-task)
  - [MQTT Transmission Task](#mqtt-transmission-task)
- [Platform](#platform)
- [Transmitting the data / connectivity](#transmitting-the-data--connectivity)
      - [For instance for a payload:](#for-instance-for-a-payload)
- [Presenting the data](#presenting-the-data)
  - [Full dashboard](#full-dashboard)
  - [Time panel](#time-panel)
  - [Air Quality panel](#air-quality-panel)
  - [Noise panel](#noise-panel)
  - [Luminosity panel](#luminosity-panel)
- [Finalizing the design](#finalizing-the-design)
  - [Pictures](#pictures)
    - [First version](#first-version)
    - [Final version](#final-version)
  - [Video Demonstrations](#video-demonstrations)
    - [First version](#first-version-1)
    - [Final version](#final-version-1)

# Introduction

This project is an IoT-based digital clock and room monitor, designed to assist in optimizing a room for better sleep. It collects data on air quality, luminosity, and noise levels. The device uses the MQTT protocol to transmit data to the cloud via a WiFi connection. The information is displayed both on a cloud-based dashboard, useful for historical analysis, and on four small OLED displays for immediate access. The goal is to support better sleep, as related issues are often subtle and may manifest in indirect ways.

Time required to complete this project: 15–20 hours, depending on experience.

# Objective

Quality sleep is one of the most important aspects of maintaining good health. Sleep quality depends on a wide range of factors. Numerous online sources suggest that for sleep to be restful and refreshing, the environment should be dark, quiet, cool, and maintain a certain level of air humidity. It’s also crucial to get these conditions just right, even subtle background noise can significantly degrade the quality of sleep.

Since humans aren’t naturally adept at perceiving subtle differences in environmental conditions, only the extremes, an IoT-based solution can be incredibly useful. By leveraging various sensors, it’s possible to measure and assess different characteristics of a sleeping environment with greater precision.

That’s the core idea behind this project. It gathers data on several key markers, including luminosity, noise level, air quality, temperature, and humidity. This data is made accessible both locally and in the cloud, enabling a better understanding of one’s sleeping conditions. The aim is to evaluate and compare my sleeping environment against commonly accepted norms found online, and make adjustments accordingly.

# Material required
A list of required materials and their costs are listed.
      
| Product | Quantity | Link  | Description | Price (SEK) |
| :---         |     ---:       | :--- | :--- | ---: |
| Waveshare ESP32-S3 Zero | 1 | [Electrokit](https://www.electrokit.com/utvecklingskort-esp32-s3) | This is the brain of the project, it will do all heavy lifting required to control the sensors, displays and the whole application flow. It boasts two Xtensa LX7 cores, running on up to 240MHz. It has plenty of SRAM, 512KB to be precise, with the addition of 2MB of PSRAM (can be used as RAM (almost)). The list of peripherals also includes a WiFi and a Bluetooth peripherals, making it perfect for low-cost prototyping. | 99 |
| Adafruit TSL2591 | 1 | [Electrokit](https://www.electrokit.com/tsl2591-digital-ljussensor-monterad-pa-kort) | A high dynamic range luminosity sensor, the measurements provided are in lux. Has adjustable measurement integration time and signal gain, together these characteristics position it as a good option for luminosity measurements in low-light environments. Since it includes an on-board regulator, it supports input voltages from 3.3V to 5V. | 109 |
| Bosch BME690 Shuttle Board 3.0 | 1 | [DigiKey](https://www.digikey.se/en/products/detail/bosch-sensortec/SHUTTLE-BOARD-3-0-BME690/25835546) | Successor to BME688, functions as a gas, temperature and humidity sensor. Doesn't have an on-board regulator, therefore requires one. The input voltage to the sensor is 3.3V. For some reason the packaging uses 1.27mm pin headers, thus an adapter is required. | 191 |
| Switchregulator step-down 0.8V - 17V 1.5A | 2 | [Electrokit](https://www.electrokit.com/switchregulator-step-down-0.8v-17v) | A step-down switching voltage regulator, supports a wide range of input values and adjustable output values. Maximum output current is 1.5A, rated efficiency is up-to 97.5%. | 58 |
| AZDelivery GY-MAX4466 | 1 | [Amazon](https://www.amazon.se/AZDelivery-GY-MAX4466-Mikrofonf%C3%B6rst%C3%A4rkare-kompatibel-Raspberry/dp/B08T1L9Y7F) | A simple breakout board that includes an electret microphone and an amplifier stage with adjustable gain.  | 64 |
| AZDelivery 1.3-inch OLED 128 x 64 pixels I2C-screen | 4 | [Amazon](https://www.amazon.se/AZDelivery-I2C-sk%C3%A4rm-kompatibel-Raspberry-inklusive/dp/B07V9SLQ6W) | Portable 1.3-inch OLED screens based on SH1106 display controller. Use I2C to communicate. In my personal experience, the displays are very sensitive to power supply quality and quality of the connection that they make. | 300 |
| AZDelivery PCA9548A I2C multiplexer | 1 | [Amazon](https://www.amazon.se/AZDelivery-PCA9548A-multiplexer-8-kanals-kompatibel/dp/B086W7SL63) | A highly customizeable, 8-channel I2C multiplexer. It is very easy to use and it plays a very important role in getting the displays to work, by allowing multiple displays that have the same I2C address, to be on the same I2C bus.   | 89 |
| Breadboard | 2 | [Electrokit](https://www.electrokit.com/kopplingsdack-840-anslutningar) |  | 138 |
| 1.27mm connectors | 1| [Amazon](https://www.amazon.se/-/en/world-trading-net-Miniature-Connectors-Connector-Varnished/dp/B0D5XXTCMS/) | | 130 |
| 1.27mm to 2.54mm breakout | 1| [Amazon](https://www.amazon.se/1-27Mm-2-54Mm-Breakout-Adapter-10pcs/dp/B0C8T5JD4F/) | | 70 |
| Total price | | | | 1248 |

The total cost of the project will vary depending on which components are already available and how closely one intends to replicate the original design. For example, the BME690 sensor can be substituted with a more accessible alternative, such as the BME688 or BME680. Doing so may also eliminate the need for additional components like a 1.27mm to 2.54mm adapter, reducing related costs.

Likewise, opting to use only a single screen for displaying the clock can help free up part of the budget as well.

# Setup

To connect your ESP32-S3 to your computer and upload code to it the following editors are recommended.

| Editor | Description |
| :--- | :---|
| [VS Code](https://code.visualstudio.com/) | Used in this project. It is very extensible but the setup process can be too volatile for a beginner. |
| [Espressif-IDE](https://github.com/espressif/idf-eclipse-plugin/blob/master/README.md) | You can use a specifically developed IDE based on Eclipse IDE if you are more familliar with it or if you don't like VSCode, also setting up the build pipeline may be more straightforward. |

## Install the IDE
### VSCode
To install VSCode follow the instructions provided here [Setting up Visual Studio Code](https://code.visualstudio.com/docs/setup/setup-overview).

### Espressif-IDE
To install Espressif-IDE follow the instructions provided by espressif [Espressif-IDE Installation](https://docs.espressif.com/projects/espressif-ide/en/latest/installation.html).

## Install the prerequisites (VSCode only)
There is some extra setup required, it will help to isolate the project and its dependencies into discrete and manageable containers.
### WSL2 (Windows)
The steps for installing WSL2 on Windows are described here by Microsoft [How to install Linux on Windows with WSL](https://learn.microsoft.com/en-us/windows/wsl/install).
 #### Configure udev rules
 In order to be able to flash the firmware onto the ESP32 device, you will need to configure `udev` rules.
 To acomplish this repeat the following steps:
 - Press `Win` key, search for `Ubuntu` (assuming its the distribution you've installed previously) and run it
 - A terminal will open inside of the WSL, run `sudo apt update` to update the list of packages
 - Make sure that `udev` is installed by running `sudo apt install udev`
 - Create a `udev` rule using `echo 'SUBSYSTEMS=="usb", ATTRS{idVendor}=="303a", ATTRS{idProduct}=="00??", GROUP="plugdev", MODE="0666"' | sudo tee /etc/udev/rules.d/40-custom.rules > /dev/null`
 - Make sure that your user is part of the `plugdev` group by running `groups`, if not run `sudo usermod -aG plugdev $(whoami)`
 - To reload the `udev` rules use `sudo udevadm control --reload` and `sudo udevadm trigger` to trigger them
 #### Configure usbipd-win
 Since USB devices are not directly accessible inside of the WSL from Windows, you will need to use `usbipd-win` to pass them through.
 This can be done in the following manner:
 - Download the driver here [usbipd-win](https://github.com/dorssel/usbipd-win/releases/tag/v5.1.0)
 - Run the installer, you may have to reboot your PC
 - Download a GUI convience tool from [WSL USB Manager](https://github.com/nickbeth/wsl-usb-manager) to make the usage more comfortable
 - After connecting the ESP32 it will be visible in the application, to flash software on the device: select the device, click bind and then attach. The device will be disconnect from Windows and attach itself to WSL


### Docker Desktop
Docker Desktop can be installed by following instructions outlined by Docker team [Install Docker Desktop on Windows](https://docs.docker.com/desktop/setup/install/windows-install/).
### Docker Desktop & VSCode integration
VSCode has support for Docker integration in form of so called [Dev Containers](https://containers.dev/). It allows for creating and configuring Docker-integrated development environments, with all dependencies being installed only in the corresponding containers. You can install it as a part of the [Remote Development Extension Pack](https://marketplace.visualstudio.com/items?itemName=ms-vscode-remote.vscode-remote-extensionpack) for VSCode.

 **To use the extension you have to:**
 - Create a folder named .devcontainer in the root directory of the project
 - Populate it with the configuration files - devcontainer.json and Dockerfile

The configuration files to use will be provided in the repository, to summarize them:
<details>
<summary>Dockerfile</summary>

```dockerfile
ARG DOCKER_TAG=latest
FROM espressif/idf:${DOCKER_TAG}
```
Will pull the latest version of a pre-configured espressif/idf Docker image.

```dockerfile
ENV LC_ALL=C.UTF-8
ENV LANG=C.UTF-8
```
Will set LC_ALL and LANG environment variables to C.UTF-8 which is used by the espidf build system.

```dockerfile
RUN apt-get update -y && apt-get install udev -y
```
Will update the system packages list and install udev to allow udev rules needed for flashing the microcontroller.

```dockerfile
RUN echo "source /opt/esp/idf/export.sh > /dev/null 2>&1" >> ~/.bashrc
ENTRYPOINT [ "/opt/esp/entrypoint.sh" ]
CMD ["/bin/bash", "-c"]
```
Will finish the setup by running the setup script and setting the entrypoint of the container.
</details>

<details>
<summary>devcontainer.json</summary>

```json
	"name": "ESP-IDF",
	"build": {
		"dockerfile": "Dockerfile",
		"args": {
			"DOCKER_TAG": "release-v5.4"
		}
	}
```
This will set a name for the container, configure the Docker image build process by using the Dockerfile that was created before and pass `release-v5.4` as a DOCKER_TAG argument.

```json
	"customizations": {
		"vscode": {
			"settings": {
				"idf.gitPath": "/usr/bin/git",
				"idf.showOnboardingOnInit": false,
				"idf.espIdfPath": "/opt/esp/idf",
				"idf.toolsPath": "/opt/esp",
				"terminal.integrated.defaultProfile.linux": "bash"
			},
			"extensions": [
				"espressif.esp-idf-extension",
				"espressif.esp-idf-web",
				"ms-vscode.cmake-tools",
				"ms-vscode.cpptools-extension-pack"
			]
		}
	}
```
This configuration will specify the VSCode extensions to pre-install and some configuration options required by the ESP-IDF VSCode extension.


```json
{
 "runArgs": [
		"--privileged",
		"-v",
		"/dev:/dev",
		"-v",
		"/etc/udev/rules.d:/etc/udev/rules.d"
 ],
 "mounts": [
		"source=vscode-${localEnv:PROJECT_ID}-extensions,target=/root/.vscode-server/extensions,type=volume",
		"source=vscode-${localEnv:PROJECT_ID}-userdata,target=/root/.vscode-server/userdata,type=volume",
		"source=${localEnv:HOME}/.ssh,target=/root/.ssh,type=bind,consistency=cached"
 ]
}
```
Will pass a number of arguments to the Docker container, as well as specify the mounts that are required for Git and for the ESP-IDF VSCode extension.
</details>

## Setup project using VSCode
### Clone the project
Clone the project using ```git clone --recursive git@github.com:xl9p/IoT-Clock-RoomMonitor.git```.
### Open the project in a container
Open the cloned folder, then navigate to `firmware/esp32s3`, after that open the folder using VSCode. 
If it is the first time opening the project, you will get a prompt to reopen the folder in a Docker container, alternatively you can press `F1` and search for `Dev Containers: Reopen in Container` option, after selecting it wait until the environment is initialized.
### Configure the project
Press `F1` and search for `ESP-IDF: Run idf.py reconfigure Task`, wait until all dependencies are validated. You should see a similar output in the terminal (press `CTRL+J` if its closed) if the operation succeeded.
```
-- Configuring done (155.9s)
-- Generating done (21.2s)
-- Build files have been written to: /workspaces/IoT-Clock-RoomMonitor/firmware/esp32s3/build
```

### Build the project
To build the project, again press `F1` and run `ESP-IDF: Build Your Project` option. Wait until it finishes. If it succeeds you should see a memory usage summary print-out in the terminal. 

Something similar to this.
```
                            Memory Type Usage Summary                             
┏━━━━━━━━━━━━━━━━━━━━━┳━━━━━━━━━━━━━━┳━━━━━━━━━━┳━━━━━━━━━━━━━━━━┳━━━━━━━━━━━━━━━┓
┃ Memory Type/Section ┃ Used [bytes] ┃ Used [%] ┃ Remain [bytes] ┃ Total [bytes] ┃
┡━━━━━━━━━━━━━━━━━━━━━╇━━━━━━━━━━━━━━╇━━━━━━━━━━╇━━━━━━━━━━━━━━━━╇━━━━━━━━━━━━━━━┩
│ Flash Code          │       797242 │          │                │               │
│    .text            │       797242 │          │                │               │
│ Flash Data          │       169920 │          │                │               │
│    .rodata          │       169664 │          │                │               │
│    .appdesc         │          256 │          │                │               │
│ DIRAM               │       143123 │    41.88 │         198637 │        341760 │
│    .text            │        88907 │    26.01 │                │               │
│    .bss             │        33704 │     9.86 │                │               │
│    .data            │        20512 │      6.0 │                │               │
│ IRAM                │        16383 │    99.99 │              1 │         16384 │
│    .text            │        15356 │    93.73 │                │               │
│    .vectors         │         1027 │     6.27 │                │               │
│ RTC SLOW            │           28 │     0.34 │           8164 │          8192 │
│    .force_slow      │           28 │     0.34 │                │               │
│ RTC FAST            │           24 │     0.29 │           8168 │          8192 │
│    .rtc_reserved    │           24 │     0.29 │                │               │
└─────────────────────┴──────────────┴──────────┴────────────────┴───────────────┘
Total image size: 1092992 bytes (.bin may be padded larger)
```

### Flash the device
After successfully building the project, the firmware needs to be uploaded onto the ESP32. This can be accomplished by following these instructions:
- Connect ESP32S3 to the PC using an appropriate cable
- Open `WSL USB Manager` that you have downloaded previously
- Locate the microcontroller in the list of all connected devices
- Select the device, click `bind` and then `attach`
- In VSCode press `F1` to open the Command Palette, from there select `ESP-IDF: Select Port to Use (COM, tty, usbserial)`. This should open a dropdown menu with all connected devices, find and select the microcontroller (often listed as `/dev/ttyACM0`).
- In the Command Palette, select `ESP-IDF: Select Flash Method`, from the dropdown choose `UART` (`JTAG` is also available, but in my experience, it has been quite unstable).
- Finally, in the Command Palette, find `ESP-IDF: Flash Your Project` option, it will initiate the flash process. If the process succeeded you will see the following print-out `[Flash]
Flash Done ⚡️
Flash has finished. You can monitor your device with 'ESP-IDF: Monitor command'`


# Putting everything together
Assembling the device is pretty straightforward, since its mostly just power supplies, I2C and analogue sensors.

>[!CAUTION]
> Care must be taken to avoid shorting the connections, especially VDD and GND.
> Always have a means to quickly shutdown the whole device.

>[!NOTE]
> Pay attention and avoid confusing the clock and the data lines. 
> 
> Keep in mind that the OLED screens are very sensitive to the quality of the power supply and quality of the connection that the pins make.

## Circuit Diagram
The circuit required for this project is fairly simple, consisting mostly of discrete components. As a result, no electrical calculations are necessary.

Schematics describing the device are provided below.

### Sensors
![Sensors Schematic](https://raw.githubusercontent.com/xl9p/IoT-Clock-RoomMonitor/refs/heads/main/images/schematics/sensors_scheme.png)
Most of the sensors include an onboard voltage regulator and are therefore connected directly to the 5V rail. The exceptions are the BME690 sensor and the MAX4466 microphone, which require a separate voltage regulator to step down the 5V supply to 3.3V.

>[!NOTE]
>The device requires a 5V power supply. This can come from any suitable source, in my case, I used a spare wall adapter that outputs 5V.

The TSL2591, BMP180, and BME690 sensors all share the same I2C bus for communication with the microcontroller. In contrast, the MAX4466 microphone outputs a DC-coupled AC signal, which is sampled via the microcontroller’s ADC.

### Displays
![Displays Schematic](https://raw.githubusercontent.com/xl9p/IoT-Clock-RoomMonitor/refs/heads/main/images/schematics/displays_scheme.png)
This schematic shows how the OLED displays are connected. The project uses four identical displays. Connecting them all directly to the same I2C bus would have caused conflicts, but this is resolved with the PCA9548A, an 8-channel I2C multiplexer. It allows control of each display by selectively passing the main I2C signal to one channel at a time, based on specific commands.

The A pins on the multiplexer determine its I2C address. In this setup, they are grounded, which sets the device address to 0x70.

As for power, the setup here is similar to the sensor section. All the connected devices tolerate 5V, so the displays are powered directly from the main 5V rail.

### Complete
![Complete Schematic](https://raw.githubusercontent.com/xl9p/IoT-Clock-RoomMonitor/refs/heads/main/images/schematics/complete_scheme.png)

## Power Consumption
Using the Nordic Power Profiler Kit II, the current consumption of the fully assembled device was measured. The results indicate a fairly significant current draw. The highest average current occurs during the initial WiFi initialization and MQTT connection phase, peaking at around 92 mA. During normal operation, the current drops slightly, averaging approximately 63.5 mA.

Although these values are somewhat high, there is room for optimization. For example, the ESP32’s light sleep mode and Dynamic Frequency Scaling (DFS) can be enabled to reduce power consumption. This can be pushed even further by utilizing the Ultra Low Power (ULP) co-processor and fine-tuning the data accumulation thresholds.

However, implementing these optimizations would require a significant time investment in the project.

![Current Cosumption](https://raw.githubusercontent.com/xl9p/IoT-Clock-RoomMonitor/refs/heads/main/images/plots/final-device-highlight-op-regions.png)

The project uses the ESP-IDF framework to simplify system management and ensure timely sampling of the sensors. It also incorporates Arduino as an ESP-IDF component, enabling the use of various driver libraries. As a result, all firmware is written in C/C++.

Although most of the sensors are controlled using publicly available drivers, a considerable amount of custom code was written, mainly as wrappers around system drivers, and mostly for self-learning purposes.

No specific code samples will be discussed here, as it wouldn't add much value in this context. Instead, diagrams will be used to illustrate the main responsibilities of each system component, without diving too deep into implementation details.

## Main system elements
![Main system elements](https://raw.githubusercontent.com/xl9p/IoT-Clock-RoomMonitor/refs/heads/main/images/diagrams/main_system.png)

This diagram outlines the main application logic. Sensor data is collected using specifically dedicated tasks. Each task checks a number of timing conditions to determine whether certain actions need to be taken. These actions include periodic calculations, Min/Max evaluations, telemetry display, and data transmission.

To aggregate the data, sensor tasks send it in packaged sensor payload structures. These contain all the required information to later encode the data properly using CBOR. Since CBOR-encoded data is binary, sending it directly through FreeRTOS queues can be problematic. To handle this, the aggregation task and the MQTT task share two separate queues. These queues are used to rotate pointers to statically allocated binary buffers. The current implementation uses three such buffers, ensuring the aggregation task almost always has one available to fill.

Display control is handled by a module called `grid_composer`. The idea behind this module is to abstract display control in a grid-like manner, where each screen acts as a grid cell capable of showing arbitrary content. To enable this, the module defines an interface that each display must implement. This includes functions like `draw_figure`, `draw_text`, and `clear`. Each display is wrapped into this interface and added to an instance of the `grid_composer`.

To perform rendering, the module uses a data type called a **draw descriptor**, which defines what should be displayed on a specific cell. A draw descriptor contains essential information such as the content type (text or figure), size, position, and more. Once a descriptor is created, it is sent to the module's internal queue, `draw_queue`. An internal task processes this queue and calls the appropriate draw methods, causing the corresponding cell to display the content (if valid).

### The configuration settings used for tasks:
| Task Name | Pinned to Core | Stack Size (bytes)  | Priority |
| :---      |     ---:       | :---                | :---     |
| SNTP Task         | 0 | 3144 | 6  |
| TSL2591 Task      | 0 | 3144 | 6  |
| Air Quality Task  | 0 | 3144 | 6  |
| Sound Sensor Task | 0 | 3144 | 6  |
| Aggregation Task  | 0 | 6144 | 8  |
| MQTT Task         | 1 | 8196 | 9  |
| Draw Task         | 0 | 6144 | 20 |

### Periodic actions of each task:
| Task Name | Base loop period (ms) | Calculation period (ms)  | Min/Max period (ms) | Report period (ms) |
| :---      |     ---:         | :---                | :---           | :---          |
| SNTP Task         | 15000 |  -   | -       | 30000 |
| TSL2591 Task      | 2800  |  -   | 3600000 | 2800  |
| Air Quality Task  | 1500  |  -   | -       | 1500  |
| Sound Sensor Task | 100   | 4000 | 3600000 | 4000  |

All sensor tasks share the same priority to allow the RTOS scheduler to use round-robin scheduling. The aggregation task has a higher priority, as it must process data promptly to prevent data loss or bottlenecks. All of these tasks run on the first core, while the second core is reserved for wireless transmission. Stack sizes were determined empirically.

The draw task is initialized separately from the main initialization logic and is assigned a priority of 20 to ensure that screen updates remain highly responsive.


## System Initialization
The system starts by initializing all essential components in a specific order. This includes hardware peripherals such as I2C buses, the ADC module, sensors, displays, and WiFi. It also includes the necessary software components, such as the SNTP and MQTT clients, the grid_composer module, queues, buffers, and most importantly, the tasks themselves.

Below are several UML flowcharts that describe the initialization procedures for each task.
## Sensor Tasks
### Air Quality Measurement task
![Air Quality Task](https://raw.githubusercontent.com/xl9p/IoT-Clock-RoomMonitor/refs/heads/main/images/diagrams/air_quality_task.png)

### Sound Sensor Sampling task
![Sound Sensor Task](https://raw.githubusercontent.com/xl9p/IoT-Clock-RoomMonitor/refs/heads/main/images/diagrams/sound_sensor_task.png)

### TSL2591 Sampling Task
![TSL2591 Task](https://raw.githubusercontent.com/xl9p/IoT-Clock-RoomMonitor/refs/heads/main/images/diagrams/tsl2591_task.png)

### SNTP Sampling Task
![SNTP Task](https://raw.githubusercontent.com/xl9p/IoT-Clock-RoomMonitor/refs/heads/main/images/diagrams/sntp_task.png)

## Data Aggregation Task
![Aggregation Task](https://raw.githubusercontent.com/xl9p/IoT-Clock-RoomMonitor/refs/heads/main/images/diagrams/aggregation_task.png)

## MQTT Transmission Task
![MQTT Task](https://raw.githubusercontent.com/xl9p/IoT-Clock-RoomMonitor/refs/heads/main/images/diagrams/mqtt_task.png)



# Platform
I chose to use a private Linux server that I assembled from spare PC parts as my platform. It handles everything cloud-related necessary for data collection and visualization in this project. This includes an MQTT broker, the TIG stack, and a proxy server. There are many reasons behind this decision, but the main ones are the desire for full control over the services and the aim to avoid the limitations commonly imposed by popular IoT cloud platforms.

The server hosts an MQTT broker, specifically, the EMQX Community version. It is configured to run as a cluster of three nodes. The setup also includes a Redis credential store and a NestJS backend to manage MQTT clients.

For data visualization, the solution relies on the TIG stack. The project uses a dedicated InfluxDB bucket and a Telegraf instance with a custom configuration to correctly parse incoming data. Grafana is used to visualize the data by connecting directly to the InfluxDB database.

An Nginx proxy sits in front of the EMQX cluster to enable SSL termination for mTLS support. Additionally, Nginx handles load balancing of incoming traffic between the nodes in the EMQX cluster.

All the files needed to spin up your own local instance of the platform are included in the repository. This includes OpenSSL certificates, so there is no need to generate them manually.

# Transmitting the data / connectivity
Since the device is designed for use inside buildings, it’s common to have some form of WiFi coverage. Therefore, the device uses WiFi to send collected data to the server on the local network. This greatly simplifies development and also allows flexibility, as it’s possible to switch to a public MQTT broker if needed.

Naturally, the chosen transport protocol is MQTT. MQTT is a machine-to-machine communication protocol with low overhead and extensive configuration options for controlling data transmission. It is especially popular in IoT applications. This project uses MQTT version 3.1.1 over port 8884. Although encryption isn’t strictly required for local deployment, the device still uses a set of pre-generated certificates to secure communication with the server.

Considering the amount of data being sent, it is accumulated and sent only when it reaches a certain size threshold, rather than transmitting periodically. This approach helps maintain longer sleep periods and reduces the overhead of each transmission, both in terms of bandwidth and power consumption. Power efficiency is especially important because the initial connection setup, handshakes, and toggling WiFi are the main causes of high current draw on the ESP32. Even though the device is intended to be powered from a wall outlet, efficient power use remains critical. This strategy also lays the groundwork for converting the device to battery power in the future.

The same concerns about efficiency apply to network bandwidth, so the size of transmitted packets is minimized. For this reason, JSON was avoided as a transmission format. Instead, a more size-efficient alternative is used, **Concise Binary Object Representation** (CBOR). Based on RFC 8949, CBOR shares a similar data model with JSON and supports a wide range of data types. It offers relatively low encoding overhead at the cost of human readability.

---

#### For instance for a payload:
<details>
<summary>data.json</summary>

```json
{
  "data": {
    "deviceId": "clock1",
    "sensor_data": [
      {
        "sensor": "sound_sens",
        "timestamp": 1754225188496271,
        "fields": {
          "rms_max_sound": 0,
          "rms_min_sound": 0,
          "rms_sound": 51.94227600097656,
          "win_s": 3600
        }
      },
      {
        "sensor": "bme690",
        "timestamp": 1754225189774227,
        "fields": {
          "iaq": 92.74124145507812,
          "press": 1010.6799926757812,
          "temp": 26.14429473876953,
          "humid": 41.80635452270508
        }
      },
      {
        "sensor": "tsl2591",
        "timestamp": 1754225190400281,
        "fields": {
          "max_lux": 0,
          "min_lux": 0,
          "lux": 33.384239196777344,
          "win_s": 3600
        }
      },
      {
        "sensor": "sntp",
        "timestamp": 1754225191885208,
        "fields": {
          "sntp_time": 1754225191
        }
      },
      {
        "sensor": "sound_sens",
        "timestamp": 1754225192500199,
        "fields": {
          "rms_max_sound": 0,
          "rms_min_sound": 0,
          "rms_sound": 56.665687561035156,
          "win_s": 3600
        }
      },
      {
        "sensor": "bme690",
        "timestamp": 1754225192789853,
        "fields": {
          "iaq": 91.45542907714844,
          "press": 1010.6549682617188,
          "temp": 26.128393173217773,
          "humid": 41.8249397277832
        }
      },
      {
        "sensor": "tsl2591",
        "timestamp": 1754225193684106,
        "fields": {
          "max_lux": 0,
          "min_lux": 0,
          "lux": 33.49155044555664,
          "win_s": 3600
        }
      },
      {
        "sensor": "bme690",
        "timestamp": 1754225195804870,
        "fields": {
          "iaq": 90.54932403564453,
          "press": 1010.6953125,
          "temp": 26.128355026245117,
          "humid": 41.837589263916016
        }
      },
      {
        "sensor": "sound_sens",
        "timestamp": 1754225196505258,
        "fields": {
          "rms_max_sound": 0,
          "rms_min_sound": 0,
          "rms_sound": 50.80354309082031,
          "win_s": 3600
        }
      },
      {
        "sensor": "tsl2591",
        "timestamp": 1754225196967119,
        "fields": {
          "max_lux": 0,
          "min_lux": 0,
          "lux": 33.54059982299805,
          "win_s": 3600
        }
      },
      {
        "sensor": "bme690",
        "timestamp": 1754225198821030,
        "fields": {
          "iaq": 91.85678100585938,
          "press": 1010.6994018554688,
          "temp": 26.13634490966797,
          "humid": 41.83697509765625
        }
      },
      {
        "sensor": "tsl2591",
        "timestamp": 1754225200250107,
        "fields": {
          "max_lux": 0,
          "min_lux": 0,
          "lux": 33.16069030761719,
          "win_s": 3600
        }
      },
      {
        "sensor": "sound_sens",
        "timestamp": 1754225200508273,
        "fields": {
          "rms_max_sound": 0,
          "rms_min_sound": 0,
          "rms_sound": 51.87485122680664,
          "win_s": 3600
        }
      },
      {
        "sensor": "bme690",
        "timestamp": 1754225201835845,
        "fields": {
          "iaq": 92.88265228271484,
          "press": 1010.6950073242188,
          "temp": 26.131006240844727,
          "humid": 41.89170837402344
        }
      },
      {
        "sensor": "tsl2591",
        "timestamp": 1754225203533117,
        "fields": {
          "max_lux": 0,
          "min_lux": 0,
          "lux": 32.30799865722656,
          "win_s": 3600
        }
      }
    ]
  }
}
```

</details>

After minifying the payload, and base64 encoding it, the final size is around 2118 bytes.

For reference, the same payload encoded using CBOR results in just 1301 bytes, a **38.6% decrease** in size. This, however, will vary depending on the exact data used.

---

In practice, data is accumulated until 15 sensor packets are assembled. Each packet is encoded in a stream-like manner, resulting in an average transmission size of about 1.45 to 1.5 KiB. The number of packets per transmission can be adjusted up or down to suit specific needs. The interval between transmissions typically varies between 15 and 17 seconds, due to round-robin scheduling among the sensors and their individual data reporting periods.

# Presenting the data
The data is stored in an InfluxDB time-series database. This choice came naturally since time-series databases are the preferred option for data with time-based characteristics. InfluxDB, in particular, is fast and, most importantly, accessible.

There is no automation involved beyond using Telegraf, which pulls data from the MQTT broker and parses it using a predefined configuration file. The parsed data is then sent to the InfluxDB database for storage.

The dashboard is built using Grafana. It is composed of specifically dedicated dashboard components configured to visualize the data effectively.
## Full dashboard
![Full dashboard](https://raw.githubusercontent.com/xl9p/IoT-Clock-RoomMonitor/refs/heads/main/images/dashboard/full_panel.png)

## Time panel
![Time panel](https://raw.githubusercontent.com/xl9p/IoT-Clock-RoomMonitor/refs/heads/main/images/dashboard/time.png)

## Air Quality panel
![Air Quality panel](https://raw.githubusercontent.com/xl9p/IoT-Clock-RoomMonitor/refs/heads/main/images/dashboard/air_quality.png)

## Noise panel
![Noise panel](https://raw.githubusercontent.com/xl9p/IoT-Clock-RoomMonitor/refs/heads/main/images/dashboard/noise.png)

## Luminosity panel
![Luminosity panel](https://raw.githubusercontent.com/xl9p/IoT-Clock-RoomMonitor/refs/heads/main/images/dashboard/luminosity.png)


# Finalizing the design
Overall, I’m satisfied with how the project turned out. There is still plenty of room for improvement, things like code optimization, additional error debugging, power consumption improvements, and so on. I would also like to design a custom PCB using prototyping boards to improve signal integrity, and potentially use a 3D printer to create an enclosure.

Although this is a beginner course, I gained not only technical knowledge but also a better understanding of myself throughout the process. Most importantly, I committed to finishing a project. I will probably make occasional improvements over time until I feel completely satisfied with it.

Below are some simple pictures and videos demonstrating how the project functions.
## Pictures
### First version
![First version picture](https://raw.githubusercontent.com/xl9p/IoT-Clock-RoomMonitor/refs/heads/main/images/pictures/older_version.JPG)

### Final version
![First version picture](https://raw.githubusercontent.com/xl9p/IoT-Clock-RoomMonitor/refs/heads/main/images/pictures/final_version_cropped.JPG)

## Video Demonstrations
### First version
<iframe width="800" height="500" src="https://www.youtube.com/embed/YibGUrIH6mc?si=sYMAHyqzEiHYGNi6" 
frameborder="0" allow="accelerometer; autoplay; clipboard-write; encrypted-media; 
gyroscope; picture-in-picture" allowfullscreen></iframe>
<!-- [First version demo](https://youtu.be/YibGUrIH6mc?si=5X3wSY4GCFQi1KCD) -->

### Final version
<iframe width="800" height="500" src="https://www.youtube.com/embed/7gsQKSHX8e0?si=A7egEwnlTy4JavXE" 
frameborder="0" allow="accelerometer; autoplay; clipboard-write; encrypted-media; 
gyroscope; picture-in-picture" allowfullscreen></iframe>
<!-- [Final version demo](https://youtu.be/7gsQKSHX8e0?si=A7egEwnlTy4JavXE) -->
