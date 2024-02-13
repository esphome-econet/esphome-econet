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

ESPHome-econet is a great first ESPHome project due to the easy hardware setup. All that's needed to run ESPHome-econet is an ESP32 or ESP8266 microcontroller and an RS485 Interface, plus a phone cord to hook it up to your Rheem appliance and a USB-C charger to power it. For simplicity, we recommend the m5Stack K045 Kit, which includes both the ESP32 & RS485 components in a simple package.

For full details on what hardware to buy and how to set it up, head over to the [Recommended Hardware Purchase and Setup page on our wiki](https://github.com/esphome-econet/esphome-econet/wiki/Recommended-Hardware-Purchase-and-Setup-Instructions).

<img src="https://github.com/esphome-econet/econet-docs/blob/main/photos-and-screenshots/Correctly-Wired-K045.jpeg?raw=true" alt="A correctly wired K045 unit." width=25%>

## Software Installation

To install the ESPHome-econet software, connect your ESP device to your computer via USB and [visit the Installation page on the ESPHome-econet website](https://esphome-econet.github.io/install/). Simply select your Rheem hardware type and the site will allow you to image the device, connect it to Wi-Fi, and add it to Home Assistant.

For alternative software installation methods and details on how to customize your configuration, check out [the detailed Software Configuration and Installation Guide on our wiki](https://github.com/esphome-econet/esphome-econet/wiki/Initial-ESPHome%E2%80%90econet-Software-Configuration-and-Installation).

## Installation Overview

For a video overview of how to setup the recommended hardware and deploy ESPHome-econet to it, please check out this video helpfully created by community member [Ylianst](https://github.com/Ylianst).

[![Water Heater - Home Assistant - ESP-Home EcoNET](https://img.youtube.com/vi/5u0_TsBOTEI/0.jpg)](https://www.youtube.com/shorts/K75njuvRIks)

## Contributing to ESPHome EcoNet

Contributions to the ESPHome-econet are welcome and encouraged! If you're looking to help out and need inspiration, please check out [the list of open issues](https://github.com/esphome-econet/esphome-econet/issues).

Contributors will want to install esphome and pre-commit, then install pre-commit hooks for your forked repo:

```bash
pip install -U esphome pre-commit  # Install required python packages
cd esphome-econet  # CD into the directory where you cloned your forked esphome-econet repo
pre-commit install  # Enable pre-commit hooks to run prior to any commit
```

### Testing Local Changes

The esphome CLI can be used to compile and install changes to YAML and/or code via the `esphome config|compile|run` commands. The provided `example-local.yaml` file provides a simple example of how to build with all local changes like this; just add a `secrets.yaml` file to the root of your checked-out repo and run `esphome compile example-local.yaml` to test compilation of your configuration and code changes. You can use the `esphome config example-local.yaml` command to see the results of any config updates, or the `esphome run example-local.yaml` command to deploy your changes to an ESPHome-capable device over Wi-Fi or USB.

## Need More Help?

If you have further questions or ideas for how to improve ESPHome-econet, please [come visit us on our Discord](https://discord.gg/cRpxtjkQQ3).
