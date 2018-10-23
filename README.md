# PresenceDetector_Clock

## Presence detection with clock<br>
This project serves mainly 2 purposes: Control of a 'analog style' LED-Clock and routing the RADAR-Motion-Sensor signal to the Smart Home Processor (Raspberry Pi 3) executing SYMCON Automation Software.<br>
The code supports an _**Arduino PRO**_ mini located on a Multi-Purpose PCB which was designed by Makerspace Wiesbaden in order to support many applications.<br>Those PCBs contain just project related components which are assembled as required.<br><br>The PCB also provides space to accommodate a XBEE/ZBEE S2C Module from _**Digi**_ which acts as a Zigbee Network Router in order to communicate with the XBEE/ZBEE Module on our Raspberry Main Controller - configured as a Coordinator. This setup is giving the advantage of Zigbee Meshing build up by many other controller boards.<br>
The second board is an universal Power supply and provides 3 indication LEDs and Interface logic if needed.


#### Controller board with XBee Module and plugged RTC<br>
<img src='images/2018/10/Screenshot 2018-10-19 23.53.14.png' width='100%'><br>
#### Optocoupler Interface (IC5) to RADAR Motion Sensor<br>
<img src='images/2018/10/Screenshot 2018-10-19 23.57.21.png' width='50%' align="middle">
<br><br>
Major function of this application is an emulated Analog clock build with 60 RGB-LEDs mounted in a circle with a diameter of about 40 cm.
<br>The Hour, Minute and Second Pointers are simulated by a small different colored Areas projected to the wall.

<br>
The clock will react on following Commands which are received via Zigbee Network on the serial Port.




Commands  |  Description
--|--
TIME  |  Set new Time (Format is 'TIMEY,M,D,H,M,S')
CM  |  Set Clock Mode
CC  |  show color (rgb) cycle
BM  |  bouncing mode (shows bouncing colors)
AM  |  ambient mode (clock shows defined color)
GA  |  alert mode - Green Alert (clock flashes orange)
OA  |  alert mode - orange alert (clock flashes orange)
CO  |  set Clock Option X minute dots
  |  
