# sim-camera
Arduino project for autonomous photography over 2G network

# Introduction
I wanted an standalone module able to take pictures of a field I own. This is part of a weather and garden/grass monitoring. This field is 30 minutes away from home and I wanted to know which garden equipment to bring.

Constraints:
* There is no electricity neither Internet connection. I wanted something powered by a "powerbank" and capable to last weeks or even months.
* The pictures are sent over Internet with a 2G cellular network (I use a french "Free 2eur" SIM Card).
* The delay between two shots can be controlled remotely.

# Diagram
![Diagram](http://www.plantuml.com/plantuml/proxy?cache=no&src=https://raw.githubusercontent.com/vstebe/sim-camera/main/diagram.txt)

# PCB
The PCB is powered on 5V and is composed of 
* Arduino Pro Mini
* TTL Camera (https://www.adafruit.com/product/397 or cheaper equivalent)
* SIM800L
* 5V->3.3V Regulator
* A MOSFET used to shut down the SIM800L and the camera module when not in use.

![PCB](=https://raw.githubusercontent.com/vstebe/sim-camera/main/pcb.png)

Note that U5 is used for the camera and U6/U8 are resevered for later use.

# Arduino code
The code is available in the "arduino" folder. You will have to replace "BASE_URL" by your own server.
This has only be tested in France with the "Free" operator. It might require some adaptation to work with other SIM cards.

# Server API
The backend must implement the following API:
* GET  /interval returns the number of seconds before the next shot in text/plain, no formatting
* POST  /upload/%id%/%n% retrieves the chunks of the picture %id% in octet/stream
* POST  /upload/%id%/last retrieves the last chunk of the picture %id% in octet/stream, and then concats all the chunks.

A Swagger file will be available, as well as a AWS Gateway/AWS Lambda implementation.

# Contributions
I am not an electronical engineer and I am sure the PCB can be optimised and could draw less current.

# Credits
* https://github.com/rocketscream/Low-Power
* https://github.com/ostaquet/Arduino-SIM800L-driver
