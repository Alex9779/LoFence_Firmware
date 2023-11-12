# LoFence V2 Firmware

This is the firmware project for a LoFence V2 device with a [Dragino LA66 module](https://www.dragino.com/products/lora/item/230-la66-lorawan-module.html).

The firware is based on the orignal LoFence firmware and so created with [Atmel/Microchip Studio 7](https://www.microchip.com/en-us/tools-resources/develop/microchip-studio) utilizing [Atmel Start](https://start.atmel.com/) (just because the original project did so and I wasn't keen enough to take care of all the basic initializations and such without it).

The LA66 communication has been moved to an own header and source file. A future plan is to make a module for this and remove the code from this project.

## Uplink remarks

### Normal data uplinks

All uplinks by the device are sent unconfirmed except the "low battery" uplink on application port (fPort) **1**.

### Low battery uplink

This uplink is the last uplink before the device deactivates itself to prevent the battery from being deep discharged and is sent confirmed on application port (fPort) **1**.

It includes the same fence data as the last normal uplink but a battery voltage value of **0** to be able to distinguish a fence error condition from a battery low error condition.

The LA66 tries to get a confirmation for about 30 seconds and tries to send the uplink again if no confirmation has been received twice per DR up to current DR - 3.

### Settings uplinks

With special downlink commands (see [Downlink commands](#downlink-commands)) the device can be triggered to sent its settings the next uplink.

These uplinks do not contain fence or battery data and are sent unconfirmed on application ports (fPort) **2** or **3**.

## Flashing the firmware

### SPI programmer

Use any AVR SPI suitable programmer (I use an [USBasp](https://www.fischl.de/usbasp/) or a [BusPirate](http://dangerousprototypes.com/docs/Bus_Pirate) for PC to device) and connect it to the SPI port on the PCB and flash the binary firmware file for example with [AVRDUDE](https://www.nongnu.org/avrdude/).

Remember if you want to run updates and preserve the EEPROM when clearing the flash, to set the high fuse for that.

#### Example AVRDUDE call using USBasp on Windows

    avrdude.exe -c usbasp -p atmega328pb -B 125kHz -U hfuse:w:0xD1:m -U lfuse:w:0xE2:m -U flash:w:"LoFence-V2.hex":i -U eeprom:w:"LoFence-V2.eep":i -U lock:w:0xFC:m

#### Example AVRDUDE call using USBasp on Windows with EEPROM preserving

    avrdude.exe -c usbasp -p atmega328pb -B 125kHz -U hfuse:w:0xD1:m -U lfuse:w:0xE2:m -U flash:w:"LoFence-V2.hex":i -U lock:w:0xFC:m

### ISPnub

For easier bulk programming and programming and updating in the wild I use a custom built ISPnub devices called *ISPnub-Stick*.

Scripts to generate the ISPnub hex files are included you just might need to adjust the paths in the scripts and the path to the ISPnub creator jar file in the post-build events.

## Downlink commands

The firmware is able to change some settings via downlinks sent to the device after an uplink has been sent.

### Get settings commands

These commands make the device send its settings at the next schedule uplink instead of sending the normal data uplink.

`0xFF01` --> send settings part 1

The sent uplink includes:

- *version*: an integer number for the firmware version on the device
- *tdc*: transmit duty cycle, the time the device sleeps in seconds between each full measurement cycle, a full measurement takes about the time to measure each polarity plus about 8 seconds
- *msr_ms*: time in milliseconds to measure each fence polarity
- *max_volt*: maximum measurable voltage

`0xFF02` --> send settings part 2

The sent uplink includes:

- *version*: an integer number for the firmware version on the device
- *bat_low*: battery voltage in mV which triggers deactivation
- *bat_low_count_max*: amount of subsequent duty cycles the battery has to be unter *bat_low* to trigger self-deactivation
- *bat_low_min*: battery voltage in mV which triggers immediate deactivation

#### Remark

In the firmware is a hard-coded value of 3100mV as the absolute minimum battery voltage. If the battery goes lower than this value the deactivation is triggered immediatly the next duty cycle.

### Write settings commands

`0x01` --> set *tdc* (transmit duty cycle) in seconds, value must be 3-byte hexadecimal value  
Example: `0x0100012C` --> 300 seconds (default value)

`0x10` --> set *msr_ms* (time in milliseconds to measure each fence polarity), value must be 2-byte hexadecimal value  
Example: `0x101770` --> 6000 milliseconds (default value)

`0x11` --> set *max_volt* (maximum measurable voltage), value must be 2-byte hexadecimal value  
Example: `0x112EE0` --> 12000 volt (default value)

`0x12` --> set *bat_low* (battery voltage in mV which triggers deactivation), value must be 2-byte hexadecimal value  
Example: `0x120D48` --> 3400 millivolt (default value)

`0x13` --> set *bat_low_count_max* (amount of subsequent duty cycles the battery has to be unter *bat_low* to trigger self-deactivation), value must be 1-byte hexadecimal value  
Example: `0x1305` --> 5 cycles (default value)

`0x14` --> set *bat_low_min* (battery voltage in mV which triggers deactivation), value must be 2-byte hexadecimal value  
Example: `0x120C1C` --> 3100 millivolt (default value)

#### Remarks

If you accidentially set the duty cycle to a too high value, schedule a downlink with the correct value, reset the device and activate it, the device starts with a normal cycle and gets the new value right after the uplink and uses it.

Ensure the downlink is sent if you schedule it before the device joins, Chirpstack for example has a setting in a device's profile to flush the downlink queue if a device joins.

## Flashing the LA66 firmware

At the time of writing the guide how to do this is [here](http://wiki.dragino.com/xwiki/bin/view/Main/User%20Manual%20for%20LoRaWAN%20End%20Nodes/LA66%20USB%20LoRaWAN%20Adapter%20User%20Manual/#H1.10A0UpgradeFirmwareofLA66USBLoRaWANAdapter).
Actually this is for the USB adapter with an LA66 but the procedure can be adjusted.
A link to the software needed is on that page, the firmware files should be [here](https://www.dropbox.com/sh/sa4uitwn6xdku9u/AAACKj4j7lPeYg1T2OU2t0dfa/LoRaWAN%20End%20Node/LA66%20LoRaWAN%20module).

To flash the LA66 you have to short *BOOT* PIN with *RX* PIN and then reset the device by grounding *RST* PIN.

To make this easier the PCB contains an unpopulated PIN header labelled *FLASH LA*.

The PINs are directly connected to the LA66 module as follows from top to bottom:

- BOOT
- RX
- RST
- GND

I usually do not solder PIN headers for this to the PCB but just use loose ones, on for *FLASH LA* and the other for *LA*.

Short *BOOT* and *RX* with a jumper and connect a jumper cable to *RST*.

Then I connect an CP2102 based USB TTL adapter with 3.3V to the LA serial connector on the PCB.
