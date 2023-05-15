# esphome-econet
You will need one M5Stack ATOM RS485 K045 Kit:

https://www.digikey.com/en/products/detail/m5stack-technology-co-ltd/K045/14318599 

https://www.mouser.com/ProductDetail/M5Stack/K045?qs=81r%252BiQLm7BQ2ho0A5VkoNw%3D%3D

And one cable:

https://www.digikey.com/en/products/detail/assmann-wsw-components/AT-S-26-6-4-S-7-OE/1972674

https://www.mouser.com/ProductDetail/Bel/BC-64SS007F?qs=wnTfsH77Xs4cyAAV7TLsUQ%3D%3D

Or any other similar RJ11/12 cable as long as it is 6P/6C or 6P/4C - we only need 4 wires for this but the 6 wire version is fine too. 

You can also use many other ESP32 development boards with the required RS485 converter - this is just an easy pre-wired setup that only requires you to cut, strip, attach 3 wires to the screw terminal, no soldering. 

Tankless Water Heater


Heat Pump Water Heater


Electric Water Heater

Protocol Documentation

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
