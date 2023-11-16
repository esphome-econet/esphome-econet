# esphome-econet [![Made for ESPHome](https://img.shields.io/badge/Made_for-ESPHome-black?logo=esphome)](https://esphome.io) [![Nightly Build](https://github.com/esphome-econet/esphome-econet/actions/workflows/build-nightly.yml/badge.svg)](https://github.com/esphome-econet/esphome-econet/actions/workflows/build-nightly.yml) [![Discord](https://img.shields.io/discord/1148015790038188073?logo=discord&logoColor=%23FFFFFF&label=Discord&labelColor=%235865F2&color=%2399AAB5)](https://discord.gg/cRpxtjkQQ3)

This [ESPHome](https://esphome.io) package creates a local network connection to econet-based device, like a Rheem Heat Pump Water Heater (HPWH), and creates entities in Home Assistant to control and monitor these devices.  This package provides more detailed and reliable sensors than Rheem's cloud-based [econet integration](https://www.home-assistant.io/integrations/econet/) available in Home Assistant.  This package and the Rheem econet integration can, however, coexist if desired.

## Supported Rheem Hardware

Most modern Rheem Heat Pump Water Heaters, Tankless Water Heaters, Electric Tank Water Heaters, and HVAC Systems are supported.

## Required ESPHome Hardware

ESPHome-econet is a great first ESPHome project due to the easy hardware setup. All that's needed to run ESPHome-econet is an ESP32 or ESP8266 microcontroller and an RS485 Interface, plus a phone cord to hook it up to your Rheem appliance. For simplicity, we recommend the m5Stack K045 Kit, which includes both components in a simple package.

For full details on what hardware to buy and how to set it up, head over to the [Recommonded Hardware Purchase and Setup  page on our wiki](https://github.com/esphome-econet/esphome-econet/wiki/Recommended-Hardware-Purchase-and-Setup-Instructions).

<img src="https://github.com/esphome-econet/econet-docs/blob/main/photos/Correctly-Wired-K045.jpeg?raw=true" alt="A correctly wired K045 unit." width=25%>

## Software Installation

For simplified software installation, simply connect your ESP32 device to your computer via USB and [visit the Installation page on the ESPHome-econet website](https://esphome-econet.github.io/install/). Simplify select your Rheem hardware type and the site will allow you to image the device, connect it to wi-fi, and add it to Home Assistant.

For alternative software installation methods and details on how to customize your configuration, check out [the detailed Software Configuration and Installation Guided on our wiki](https://github.com/esphome-econet/esphome-econet/wiki/Initial-ESPHome%E2%80%90econet-Software-Configuration-and-Installation).

## Installation Overview

For a video overview of how to setup the recommended hardware and deploy ESPHome-econet to it, please check out this video helpfully created by community member [Ylianst](https://github.com/Ylianst).

[![Water Heater - Home Assistant - ESP-Home EcoNET](https://img.youtube.com/vi/4IVNuJEpytA/0.jpg)](https://www.youtube.com/watch?v=4IVNuJEpytA)

## Need More Help?

If you have further questions or ideas for how to improve ESPHome-econet, please [come visit us on our Discord](https://discord.gg/cRpxtjkQQ3).
