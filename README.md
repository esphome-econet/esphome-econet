# esphome-econet

This ESPHome package creates a local network connection to econet-based devices, currently water heaters like the Rheem Heat Pump Water Heater (HPWH), and creates entities in Home Assistant to control and monitor the devices.  This package also provides more and more detailed sensors than does the Rheem econet cloud-based integration available in Home Assistant.  However, both can coexist.  At present, this package does not support mode-switching; use the Rheem integration for that.

## Hardware ##

You will need one M5Stack ATOM RS485 K045 Kit:

https://www.digikey.com/en/products/detail/m5stack-technology-co-ltd/K045/14318599 

https://www.mouser.com/ProductDetail/M5Stack/K045?qs=81r%252BiQLm7BQ2ho0A5VkoNw%3D%3D

And one cable:

https://www.digikey.com/en/products/detail/assmann-wsw-components/AT-S-26-6-4-S-7-OE/1972674

https://www.mouser.com/ProductDetail/Bel/BC-64SS007F?qs=wnTfsH77Xs4cyAAV7TLsUQ%3D%3D

Or any other similar RJ11/12 cable as long as it is 6P/6C or 6P/4C - we only need 3 wires for this but the 6 wire version is fine too. 

You can also use many other ESP32 development boards with the required RS485 converter - this is just an easy pre-wired setup that only requires you to cut, strip, attach 3 wires to the screw terminal, no soldering. 

## Installation ##

The  M5Stack ATOM RS485 K045 Kit includes two components, the RS485 interface, and an ESP32 board (ATOM Lite) plugged into that interface.  Three wires from a RJ11/12 cable attach via screw terminals to the RS485 interface, and a USB-C cable provides the power and the initial programming interface to the ATOM.  Once deployed, the ATOM communicates via WIFI to the local network and the RS485 converter provides the interface between the water heater and the ATOM.  No configuration is required for the converter. The esphome-econet software is compiled and loaded onto the ATOM using standard ESPHome tools.

### Hardware Installation ###

Cut the connector off one one end of the RJ11/12 cable, then strip and connect the Red, Green, and Yellow cable wires to the RS485 device's screw terminals, Red to B, Green to A, and Yellow to G.  A and B are for data communication, and G is ground. 12V is left empty.
 
```
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
![image](https://github.com/kaijk/esphome-econet/assets/49624034/add8e296-1b95-407b-b94c-c751e91cd591)

### Software Installation ###

Installation of esphome-econet requires having first installed ESPHome. See Getting Started at https://esphome.io/index.html for ESPHome installation instructions. 

Three econet based water heater types are supported.  Each has its own `yaml` configuration file in this repo that defines the applicable Home Assistant Sensors and Controls.  These are:

**Tankless Water Heater**

- econet_tankless_water_heater.yaml

**Heat Pump Water Heater**

- econet_heatpump_water_heater.yaml

**HVAC**

- econet_hvac.yaml

**Electric Water Heater**

- Not currently supported

### Compiling and Uploading esphome-econet ###

#### ESPHome in a Docker Container ####

Once ESPHome is installed, run `ls /dev/ttyUSB*` from a command line to list the USB ports currently in use.  Then, plug the USB-C cable into the ATOM and into the computer running ESPHome. 
Run `ls /dev/ttyUSB*` again and note the new port shown, perhaps /dev/ttyUSB2 (there should be one more than before).  That is the port used by the ATOM ESP32 board, and is the port to which the esphome-econet program will be uploaded.

Extract the contents of the water heater's configuration file from the repo to a new file in the Home Assistant /config directory.

Edit the configuration file:
- Insert the local network's wifi credentials or `secret` references in place of the sample entries.
- Make sure `external_components` is set to:
```
external_components:
  - source: github://stockmopar/esphome-econet
```
- Save and exit the file

Run the following command to compile and upload esphome-econet to the ATOM.  This assumes the docker container is named `esphome` and that the ATOM is found on `/dev/ttyUSB2`. Change as necessary.
```
sudo docker run --rm -v "${PWD}":/config --device=/dev/ttyUSB2 -it esphome/esphome run econet_heatpump.yaml
```
The program should compile and ask whether to install OTA or via USB. Choose USB.  It should then upload.  

Once uploaded, unplug the USB cable from the computer and move the device to the water heater.  Connect the RJ11/12 cable to the port on the display panel and provide power from a wall wart to the USB cable that is plugged into the ATOM.

Open Home Assistant and Add a New ESPHome Integration choosing esphome-econet.  However, the device may be discovered automatically, in which case just accept the new device and wait a few moments.  A number of sensors will soon appear in `Developer Tools > States`. These will reflect the name of your water heater that you may have configured in the Rheem econet app on a mobile device.  This package though is strictly local and does not require a cloud account or its credentials.

## Protocol Documentation ##

Example commands to change a heat pump water heater settings here: https://github.com/stockmopar/esphome-econet/blob/main/m5atom-rs485-econet.yaml

Decode this into Charcode here:  https://gchq.github.io/CyberChef/#recipe=From_Charcode('Space',16)Strings('Single%20byte',4,'Alphanumeric%20%2B%20punctuation%20(A)',false,false,false/disabled)&input=MHg4MCwgMHgwMCwgMHgxMiwgMHg4MCwgMHgwMCwgMHg4MCwgMHgwMCwgMHgwMywgMHg0MCwgMHgwMCwgMHgxMiwgMHgwMCwgMHgwMCwgMHgxRiwgMHgwMSwgMHgwMSwgMHgwMCwgMHgwNywgMHgwMCwgMHgwMCwgMHg1NywgMHg0OCwgMHg1NCwgMHg1MiwgMHg1MywgMHg0NSwgMHg1NCwgMHg1MCwgMHg0MiwgMHhGOCwgMHgwMCwgMHgwMCwgMHhFNCwgMHhFRQ


Addresses

Commands
* READ_COMMAND (30)
* ACK (6)
* WRITE_COMMAND (31)

Object Strings

| String        | Units         | Gas Tankless | Heat Pump    | Electric | Values     |
| ------------- | ------------- |------------- |------------- |--------- |------------|
| WHTRENAB      |               | Y            |Y             |          | 1,0        |
| WHTRSETP      |               | Y            |Y             |          |            |
| WHTRMODE      |               | Y            |              |          |            |
| WHTRCNFG      |               |              |Y             |          | 0,1,2,3,4  |
| FLOWRATE      |               | Y            |              |          |            |
| TEMP_OUT      |               | Y            |              |          |            |
| TEMP__IN      |               | Y            |              |          |            |
| WTR_USED      |               | Y            |              |          |            |
| WTR_BTUS      |               | Y            |              |          |            |
| IGNCYCLS      |               | Y            |              |          |            |
| BURNTIME      |               | Y            |              |          |            |

Credits:

https://github.com/syssi/esphome-solax-x1-mini

https://github.com/glmnet/esphome_devices

Donate:

https://www.paypal.com/donate/?hosted_button_id=NKVWX2AJRLDT2
