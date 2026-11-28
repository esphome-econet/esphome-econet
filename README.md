# esphome-econet [![Made for ESPHome](https://img.shields.io/badge/Made_for-ESPHome-black?logo=esphome)](https://esphome.io) [![Nightly Build](https://github.com/esphome-econet/esphome-econet/actions/workflows/build-nightly.yml/badge.svg)](https://github.com/esphome-econet/esphome-econet/actions/workflows/build-nightly.yml) [![Discord](https://img.shields.io/discord/1148015790038188073?logo=discord&logoColor=%23FFFFFF&label=Discord&labelColor=%235865F2&color=%2399AAB5)](https://discord.gg/cRpxtjkQQ3)

This [ESPHome](https://esphome.io) package enables local control of a Rheem appliance, like a Rheem Heat Pump Water Heater (HPWH), and creates entities in Home Assistant to control and monitor these devices. This package provides more detailed and reliable sensors than Rheem's cloud-based [EcoNet integration](https://www.home-assistant.io/integrations/econet/) available in Home Assistant, and does so without any requirement for internet access by accessing the device via its RJ11 diagnostics port.

This package and the Rheem EcoNet integration can coexist if desired.

<!-- markdownlint-capture -->
<!-- markdownlint-disable -->
<p float="left">
    <img src="https://github.com/esphome-econet/econet-docs/blob/main/photos-and-screenshots/Controls_and_Sensors.png?raw=true" alt="Example Home Assistant device controls and sensors for an ESPHome-econet device." width=40%>
    <img src="https://github.com/esphome-econet/econet-docs/blob/main/photos-and-screenshots/Thermostat.png?raw=true" alt="Setting Water Heater thermostat mode through Home Assistant with ESPHome-econet." width=50%>
</p>
<p float="left">
    <img src="https://github.com/esphome-econet/econet-docs/blob/main/photos-and-screenshots/Diagnostics.png?raw=true" alt="Diagnostic sensors exposed to Home Assistant by ESPHome-econet." width=40%>
    <img src="https://github.com/esphome-econet/econet-docs/blob/main/photos-and-screenshots/Configuration.png?raw=true" alt="Configuration via Home Assistant with ESPHome-econet." width=40%>
</p>
<!-- markdownlint-restore -->

## Supported Rheem Hardware

Most modern Rheem Heat Pump Water Heaters, Tankless Water Heaters, Electric Tank Water Heaters, and HVAC Systems (with preliminary support for multi-zone systems) are supported.

## Required ESPHome Hardware

ESPHome-econet is a great first ESPHome project due to the easy hardware setup. All that's needed to run ESPHome-econet is an ESP32 or ESP8266 microcontroller, an RS485 interface, a USB-C charger to power it, and a connection to your Rheem appliance's RJ11/RJ12 port (either a cut 6P4C/6P6C phone cord or a solderless RJ12 male-to-screw-terminal breakout adapter with short jumper wires).

For simplicity, we recommend ordering **both** the m5Stack AtomS3 Lite **and** the m5Stack ATOMIC RS485 Base—they are sold separately, stack together into a single package, and use preconfigured pins (`tx_pin: GPIO6`, `rx_pin: GPIO5`) in the web installer and `build-yaml/*-esp32s3.yaml` manifests (note that `econet_base.yaml` defaults to `GPIO19`/`GPIO22` if writing custom YAML directly). The m5Stack K045 Kit bundled the older ATOM Lite + RS485 Base combination, but m5Stack has marked it end-of-life.

For full details on what hardware to buy, exact RJ11/RJ12 pinouts, optional 120-ohm bus termination, and how to wire everything up, head over to the [Recommended Hardware Purchase and Setup page on our wiki](https://github.com/esphome-econet/esphome-econet/wiki/Recommended-Hardware-Purchase-and-Setup-Instructions).

**Important wiring note:** Do **not** wire by color alone. Standard telephone cords reverse wire colors from one end to the other, and the M5Stack RS485 terminal label colors (`B` = blue, `A` = green, `DC 12V` = red, `G` = grey) do not match phone-wire colors. Always match by pin number on the plug going into the appliance (`Pin 3 -> B`, `Pin 4 -> A`, `Pin 5 -> G`) using the wiki pinout diagram.

<img src="https://github.com/esphome-econet/econet-docs/blob/main/photos-and-screenshots/Correctly-Wired-K045.jpeg?raw=true" alt="A correctly wired K045 unit." width=25%>

## Software Installation

To install the ESPHome-econet software on the recommended M5Stack hardware (or an ESP8266 Wemos D1 mini wired to `GPIO4`/`GPIO5`), connect your ESP device to your computer via USB and [visit the Installation page on the ESPHome-econet website](https://esphome-econet.github.io/install/). Select your Rheem hardware type (note: Hybrid Electric Heat Pump Water Heaters use **Heat Pump / Hybrid Electric Water Heater**, not Electric Tank) and the site will flash the device, configure Wi-Fi, and let you add it to Home Assistant.

For other boards, custom UART pins, or RS485 modules with a `DE`/`RE`/`EN` flow control pin, follow [the detailed Software Configuration and Installation Guide on our wiki](https://github.com/esphome-econet/esphome-econet/wiki/Initial-ESPHome%E2%80%90econet-Software-Configuration-and-Installation).

If your device connects to Home Assistant (showing `WiFi Signal Strength`, which is the ESP's Wi-Fi) but every appliance entity stays `unknown`, check the [Troubleshooting guide on our wiki](https://github.com/esphome-econet/esphome-econet/wiki/Troubleshooting).

## Installation Overview

For a video overview of how to setup the recommended hardware and deploy ESPHome-econet to it, please check out this video helpfully created by community member [Ylianst](https://github.com/Ylianst).

[![Water Heater - Home Assistant - ESP-Home EcoNET](https://img.youtube.com/vi/K75njuvRIks/0.jpg)](https://www.youtube.com/shorts/K75njuvRIks)

## Contributing to ESPHome EcoNet

Contributions to the ESPHome-econet are welcome and encouraged! If you're looking to help out and need inspiration, please check out [the list of open issues](https://github.com/esphome-econet/esphome-econet/issues).

Contributors will want to install the development dependencies (`esphome` and `pre-commit`), then install pre-commit hooks for your forked repo:

```bash
cd esphome-econet  # CD into the directory where you cloned your forked esphome-econet repo
pip install -U -r requirements.txt  # Install required python packages (esphome, pre-commit)
pre-commit install  # Enable pre-commit hooks to run prior to any commit
```

### Testing Local Changes

The esphome CLI can be used to compile and install changes to YAML and/or code via the `esphome config|compile|run` commands. The provided `example-local.yaml` file provides a simple example of how to build with all local changes like this; just add a `secrets.yaml` file to the root of your checked-out repo and run `esphome compile example-local.yaml` to test compilation of your configuration and code changes. You can use the `esphome config example-local.yaml` command to see the results of any config updates, or the `esphome run example-local.yaml` command to deploy your changes to an ESPHome-capable device over Wi-Fi or USB.

## Need More Help?

If you have further questions or ideas for how to improve ESPHome-econet, please [come visit us on our Discord](https://discord.gg/cRpxtjkQQ3).
