# Compression Tester for Rotary Engine
This project is a fork of [rocotest](https://github.com/joel-loube/rocotest)

This repository contains code, models, and instructions for a DIY compression tester for rotary engines.

## Why is a special compression tester needed for a rotary engine?

Because each rotor has 3 faces, any one of which can have differing compression numbers from the other two, and there is only one hole to measure compression from. This means that we need a compression tester that is able to figure out which face each compression pulse is coming from and separate them.

## Why cant I use a regular compression tester?

You certainly can, but it doenst show you the whole picture. If the compression on one face of a rotor is bad, that will not show up on a regular tester, because the pulses from the other two faces are higher.

## What components do I need for this?

1. A set of 3d printed parts for the case.
2. a screen: https://www.amazon.ca/gp/product/B07YNP2L95
3. an arduino nano: https://www.amazon.ca/gp/product/B071NMD14Y
4. a 200 psi pressure transducer: https://www.amazon.ca/gp/product/B0748CVN3F
5. a 1/4 to 1/8 npt adapter https://www.amazon.ca/gp/product/B0044F8YXW
6. a 14mm long-rech spark plug non-fouler https://www.amazon.ca/gp/product/B000BYGJQY
7. some hookup wire (get the small stuff, nothing here needs high amperage, and its easier to fit into small spaces)
8. at least 9 heat set inserts, M2 X D4.0 X L3.0: https://www.aliexpress.com/item/4000232858343.html
9. at least 9 screws, m2 x 5mm long: https://www.aliexpress.com/item/4000282581926.html
10. a soldering iron
11. a 10K resistor (optional)
12. a usb cable
13. jbweld

## How do I assemble this?

1. Print the parts
2. heat set all of the inserts into the parts (2 in the top of the case, and 7 in the bottom)
3. drop the arduino into the slot, and use the small tab to fix it down
4. insert your selected sensor cable into the hole, fix it down with the clamp (if the clamp is too loose, use layers of heatshrink to enlarge it until there is a snug fit.
5. solder the wires from the sensor to the board, power to the 5v pin, ground to the GND pin, and signal to the A0 pin
6. remove any headers from the screen, you will not be able to fit them in
7. solder wires from the back of the screen to the arduino, power to the 5v pin (2 wires there, annoying but doable), ground to the other GND pin, SCL to A5, SDA to A4.
8. Put the 10K resistor between the sensor pin (A0) and GND if you want the device to detect if the sensor is connected or not
9. use the strap and 2 screws to fix the screen to the front panel. These screens are fragile so be careful. You should not need to trim anything, but it is possible.
10. plug your usb cable in and into a computer
11. load up arduino and load the sketch
12. make sure you have the Adafruit_SSD1306 library and the Adafruit_GFX library in your installed arduino libraries
13. load the firmware onto the board
14. jbweld the npt adapter into the spark plug non fouler
15. teflon tape the sensor into the npt adapter
16. put some big heatshrink or tape around the whole thing if you want some extra grip or damage resistance
17. probably a good idea to put a rubber oring onto the non fouler, makes a better seal

## How do I use this?

1. disconnect the sensor from the harness
2. screw it into a spark plu hole
3. plug it back in
4. plug the tester into a usb power bank, or a pc
5. wait until it says you can crank the engine
6. do so
7. stop cranking when the device display 'STOP'
8. read the results. They will altern beteen raw results and results normalized at 250RPM

## Fine tuning and accuracy
### +5V accuracy

Depending on where you purchase your Arduino Nano, some models may include a diode in series between the +5VUSB and the +5V pin, resulting in the +5V pin actually outputting only about 4.6V. Other models use a transistor instead, which allows the +5V pin to maintain a full 5V output. This difference is significant because the Arduino's analog-to-digital converter uses the +5V as its reference voltage. Here’s how you can test and correct this, if necessary:

1. Connect the Arduino Nano to a power source using a USB cable.
2. Use a multimeter to measure the voltage between the 'GND' and '+5V' pins.
3. If the voltage reads less than 4.9V, it indicates a diode is present between +5VUSB and +5V. To fix this:
   - Remove the voltage regulator by cutting its three legs and desoldering the tab.
   - Locate and remove the diode near the USB connector.
   - Solder a wire across the pads where the diode was previously connected.
4. Re-test to check if the voltage is now 5V.

### Dead space coorrection factor
The RX-8 has a displacement of 1308cc on two rotors. This means that each rotor has 654cc. However, when the sensor is inserted into the rotor assembly, the room inside the sensor adds-up to the overall displacement. Suppose the sensor contains 3cc, this means the total displacement will be 657cc for the rotor and the sensor. For the best results:
1. Meseare the sensor volume with a syringe, in cubic centimeters
2. Enter the mesured value in the `SENSOR_VOLUME_CC` preprocessor directive

The code automatically caclulates a correction factor based on the sensor volume you have entered.
