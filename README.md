# esphome-econet [![Made for ESPHome](https://img.shields.io/badge/Made_for-ESPHome-black?logo=esphome)](https://esphome.io) [![Nightly Build](https://github.com/esphome-econet/esphome-econet/actions/workflows/build-nightly.yml/badge.svg)](https://github.com/esphome-econet/esphome-econet/actions/workflows/build-nightly.yml) [![Discord](https://img.shields.io/discord/1148015790038188073?logo=discord&logoColor=%23FFFFFF&label=Discord&labelColor=%235865F2&color=%2399AAB5)](https://discord.gg/cRpxtjkQQ3) 

This [ESPHome](https://esphome.io) package creates a local network connection to econet-based device, like a Rheem Heat Pump Water Heater (HPWH), and creates entities in Home Assistant to control and monitor these devices.  This package provides more detailed and reliable sensors than Rheem's cloud-based [econet integration](https://www.home-assistant.io/integrations/econet/) available in Home Assistant.  This package and the Rheem econet integration can, however, coexist if desired.

## Supported Rheem Hardware

Rheem Heat Pump Water Heaters, Tankless Water Heaters, Electric Tank Water Heaters, and HVAC Systems are supported.

## Suggested Hardware Setup

For a simple and proven platform on which to run this package you will need an RS485 interface and an ESP32 board. These are available as follows:

- One M5Stack ATOM RS485 K045 Kit: [mouser](https://www.mouser.com/ProductDetail/M5Stack/K045?qs=81r%252BiQLm7BQ2ho0A5VkoNw%3D%3D) has them while stock is available. M5Stack has discontinued the combined K045 Package.
- Or, the same thing, but ordered as two separate parts: The M5Stack *A131 RS485 Base* [M5Stack A131](https://shop.m5stack.com/products/atomic-rs485-base) and the M5Stack *C008 Atom Lite* [M5Stack C008](https://shop.m5stack.com/products/atom-lite-esp32-development-kit?variant=32259605200986).
  - Both Digikey and Mouser have the Atom. The RS485 Base is not available from either vendor as of this update. 
    - [Digikey C008](https://www.digikey.com/en/products/detail/m5stack-technology-co-ltd/C008/12088545) (Never mind the image Digikey displays, see the datasheet)
    - [Mouser C008](https://www.mouser.com/ProductDetail/M5Stack/C008?qs=sGAEpiMZZMuqBwn8WqcFUj1SFkunHY10TJ3jnDGC7E4NSjPubczP2Q%3D%3D)

- One cable: [digikey](https://www.digikey.com/en/products/detail/assmann-wsw-components/AT-S-26-6-4-S-7-OE/1972674) or [mouser](https://www.mouser.com/ProductDetail/Bel/BC-64SS007F?qs=wnTfsH77Xs4cyAAV7TLsUQ%3D%3D), or any other similar RJ11/12 cable as long as it is 6P/6C or 6P/4C (we only need 3 wires for this but the 6 wire version is fine too)

You can also use many other ESP32 or ESP8266 development boards with the required RS485 converter - the above is just an easy pre-wired setup that only requires you to cut, strip, and attach 3 wires to the screw terminal with no soldering required.

### Installation Overview

Three wires from a RJ11/12 cable attach via screw terminals to the RS485 interface, and a USB-C cable provides power and an initial programming interface to the ATOM.  Once deployed, the ATOM communicates via WiFi to the local network and the RS485 converter provides the interface between the water heater and the ATOM.  No configuration is required for the converter. The esphome-econet software is compiled and loaded onto the ATOM using standard ESPHome tools.

### Hardware Installation

Cut the connector off one one end of the RJ11/12 cable, then strip and connect the Red, Green, and Yellow cable wires to the RS485 device's screw terminals, Red to B, Green to A, and Yellow to G.  A and B are for data communication, and G is ground. 12V is left empty.

*NOTE: Wire colors can vary from cable to cable. Ensure you are matching the pins as shown in the diagram below regardless of cable color, i.e. Pin 3 to B, Pin 4 to A, Pin 5 to G.*

```text
    6P4C RJ11/RJ12 male connector end view   
    
               +---------+
            1  ---       |
            2  ---       +--+      
Red     B   3  ---          |     
Green   A   4  ---          |        
Yellow  GND 5  ---       +--+
            6  ---       |
               +---------+
```

## Software Installation

Setup can be done from the command line using the esphome docker image, or via a (mostly) graphical interface by using an esphome dashboard installation; see Getting Started at <https://esphome.io/index.html> for full ESPHome installation and usage instructions. Initial installation must be done over USB. After this, updates and configuration changes can be made entirely over WiFi.

Installation is a three-step process:

1. Customize the basic configuration yaml for your device and environment
2. Deploy to the device via command line or ESPHome Dashbaord
3. Add your device to Home Assistant

### Step 1: Customizing the esphome-econet yaml for Your Device

Before installing, you will need to create a customized yaml for your device/environment. Start with either the [example-esp32.yaml](https://github.com/stockmopar/esphome-econet/blob/main/example-esp32.yaml) or [example-esp8266.yaml](https://github.com/stockmopar/esphome-econet/blob/main/example-esp8266.yaml) files as a template, depending on your chosen hardware.

At a minimum, you will need to either [put in your own WiFi network details](https://esphome.io/components/wifi) or provide a `secrets.yaml` file with the `wifi_ssid` and `wifi_password` fields configured. An example `secret.yaml` would look like:

```yaml
wifi_ssid: "your ssid"
wifi_password: "your password"
```

You will also need to update the `packages -> econet -> file` entry with the name of the configuration file that corresponds to your device:

* **Heatpump Water Heaters**: `econet_heatpump_water_heater.yaml`
* **Tankless Water Heaters**: `econet_tankless_water_heater.yaml`
* **Electric Tank Water Heaters**: `econet_electric_tank_water_heater.yaml`
* **HVAC Systems**: `econet_hvac.yaml`

If you are using hardware other than the kit recommended above, you may also need to update the GPIO Pin fields. See the individual device configuration files for more customizeable options.

### Step 2: Compiling and Uploading esphome-econet to Your Device

Once you've customzied your YAML you can install it either by copying your yaml file (and `secrets.yaml` file) to the config directory of your esphome-dashboard installation and running the install command, or by using the ESPHome command line via Docker.

#### Installing via ESPHome in a Docker Container

Run `ls /dev/ttyUSB*` from a command line to list the USB ports currently in use.  Then, plug the USB-C cable into the ATOM and into the computer running ESPHome. Run `ls /dev/ttyUSB*` again and note the new port shown, e.g. `/dev/ttyUSB1` (there should be one more than before).  That is the port used by the ATOM ESP32 board, and is the port to which the esphome-econet program will need to be installed.

Run the following command from the directory containing your configuration file and your `secrets.yaml` to compile and upload esphome-econet to the ATOM.  This assumes that your configuration file is `example-esp32.yaml` and that the ATOM is found on `/dev/ttyUSB1`; change as necessary.

```bash
docker run --rm -v "${PWD}":/config --device=/dev/ttyUSB1 -it esphome/esphome run example-esp32.yaml
```

The program should compile and ask whether to install OTA or via USB. Choose USB.  It should then upload.  

Once uploaded, unplug the USB cable from the computer and move the device to the water heater.  Connect the RJ11/12 cable to the port on the display panel and provide power from a wall wart to the USB cable that is plugged into the ATOM.

### Step 3: Adding New Device to Home Assistant

Open Home Assistant and add a new [ESPHome Integration](https://my.home-assistant.io/redirect/config_flow_start?domain=esphome), inputing the hostname of your device (e.g. "econet-hpwh.local" by default for Heat Pump Water Heaters).  The device may be discovered automatically, in which case just accept the new device and wait a few moments.  Once added, you can visit the device's details page (via `Settings -> Devices & Services`) to see all of the provided sensors are working.
