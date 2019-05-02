# Totally Useless LED64
### Wuuut?
It's just a useless way to use a RGB led as powerled in a C64. Why? Why not?

# Hardware
The hardware is pretty much a stripped down Arduno Uno: an ATmega328p, an 16MHz xtal, two capacitors to piggyback on CIA2 of the C64. (The xtal and capacitors could be skipped, runnig at 8Mhz internal clock ought to be enough).

| Part | Footprint | Value |
|--|--|--|
| R1 | 1206 |Value depends on your led |
| R2 | 1206 |Value depends on your led |
| R3 |1206 |Value depends on your led |
| U1 | TQFP32 |ATMega 328P  |
| C1 | 1206 | Capacitor 22pf |
| C2 | 1206 | Capacitor 22pf |
| Y2 | HC49-US | Xtal 16MHz |

## Operation
Int1 (Arduino pin3 or ATmega pin1) is connected to  the port control (/PC) of CIA2, detecting when something is being sent from the C64 by attatching interrupt to falling state. This initializes the sequence counter interrupt function; not a pretty sight in the code, but it works. 

# Software
## 	Protocol
### Data 
| Byte | Data  |
|--|--|
|0-1  | Header: 0xff 0x99   |
| 2 | Data length, CMD + data  |
| 3 | CMD  |
| 4-n | Data, max 27 bytes  |
| n+1 | Checksum   0x100 - ((CMD + data) ^ 0xff) |

**The checksum part:**
CMD plus data is added together, xor'd and substracted from 256, let's say we're setting program speed to 10 (in blink mode: blink every 1 second). We'll be sending, header (0xff 0x99), length (0x02), set speed (0x03), speed itself (0x0a) and checksum (0x100 - ((0x03 + 0x0a) ^ 0xff) ).
| Byte | Data  |
|--|--|
|0|0xff|
|1|0x99|
|2|0x02|
|3|0x03|
|4|0x0a|
|5|0x0e|




### Commands

|Command| CMD |Size | Payload |
|--|--|--|--|
| Set RGB  | 0x01 | 3  | R, G, B value (0-255). Sets max value for all colors. |
| Set Mode | 0x02 | 1 | Mode, see "Mode table" |
| Set Speed | 0x03  | 1 | Speed: 10000 / X millis, used in fade, blink and others.  |
| Program select | 0x04 |1  | Program, 0-31. |
| Program save | 0x05 | 1 | Save to current program.  |
| Set color max | 0x06  | 2 | Color (Red = 0, Green = 1, Blue = 2), value (0-255) |
| Set color min | 0x07  | 2 | Color (Red = 0, Green = 1, Blue = 2), value (0-255) |
| Set program | 0x08  | 8 | Write program, see "Program" |
| Set start up program | 0x09 | 1 | Program number 0-31 |
| Next mode | 0x10 | - |  |
| Previous mode | 0x11  | -  |  |
| Next program | 0x12 | - |  |
| Previous program | 0x13 | - |  |
| Red + 1 | 0x14 | - |  |
| Red - 1 | 0x15 | - |  |
| Green + 1 | 0x16 | - |  |
| Green - 1 | 0x17 | - |  |
| Blue + 1 | 0x18 | - |  |
| Blue - 1 | 0x19 | - |  |
| Red + 10 | 0x1a | - |  |
| Red - 10 | 0x1b | - |  |
| Green + 10 | 0x1c | - |  |
| Green - 10 | 0x1d | - |  |
| Blue + 10 | 0x1e | - |  |
| Blue - 10 | 0x1f | - |  |
| Speed + 1 | 0x20 | - |  |
| Speed - 1 | 0x21 | - |  |
| Speed + 10 | 0x22 | - |  |
| Speed - 10 | 0x23 | - |  |
| Save current program | 0x24 | - |  |
| Write to EEPROM | 0x22 | 2 | Addr, data. |

## Programs

  32 programs are stored in EEPROM: 0-31 * 8 = 256 bytes
| Byte | Data | Explanation |
|--|--|--|
| 0 | Red max |  |
| 1 | Red min |  |
| 2 | Green max |  |
| 3 | Green min |  |
| 4 | Blue max |  |
| 5 | Blue min |  |
| 6 | Mode | |
| 7 | Speed | **10000 / X millis**, used in fade, blink and others. 39 millis is the fastest speed (10000/ 255), 10 seconds is the slowest. | 




## Arduino

### Configuration
#### Modes
When creating a new mode one have to update the maxMode byte and the switch statement in setup() adding a function call to the new mode function. 

A mode function is nothing but a loop that waits for exitLoop to become not zero. exitLoop is set in the interrupt routine if the command sent from the C64 was executed. **Good practice**: don't use *delay()* in  your mode.

modeZero(), just a normal static led. Sets the color and then goes into a loop.

    void modeZero() {
	  analogWrite(9, redMax);
	  analogWrite(10, greenMax);
	  analogWrite(11, blueMax);
	  while ( ! exitLoop ); // wait for next command.
	}

modeOne(), blinking led.

	void modeOne() {
	  unsigned long last = millis();
	  int ledStatus = 0;
	  while ( ! exitLoop ) {
	    if ( millis() - last >= (10000 / speed))  {
	      if ( ledStatus ) {
	        analogWrite(9, 0);
	        analogWrite(10, 0);
	        analogWrite(11, 0);
	        ledStatus = 0;
	      } else {
	        analogWrite(9, redMax);
	        analogWrite(10, greenMax);
	        analogWrite(11, blueMax);
	        ledStatus = 1;
	      }
	      last = millis();
	    }
	  }
	}

### FYI: How data is read.
As described in operetion an interrupt is attached, to read the data. However since there aren't 8 input pins available in a port the full byte sent from the C64 is pieced together using nibbles from PORTD and C. 

	void readData() {
	  value = (PIND & 0xF0) | (PINC & 0x0F); // Add nibbles to a byte.
	  if ( ! sequence && value == 0xff ) {
	    sequence++;
	    data[0] = value;
	  } else if ( sequence == 1 ) {
	    if ( value == 0x99 ) {
		      sequence++;
		      data[1] = value;
		      headerOk = 1;
		} else if ( value = 0xff ) {
	      // Just in case.. We might have gotten an extra 0xff from setting the serial port to output on the C64.
		} else {
		      sequence++;
		}
	  } else if ( sequence == 2 && headerOk ) {
	    sequence++;
	    len = value;
	    bytesLeft = value;
	    data[2] = value;
	    if ( len > 27 ) { // Payload too beaucoup
	      clearData();
	      sequence = 0;
	    }
	  } else if ( bytesLeft && sequence > 2) {
	    data[3 + ((bytesLeft * -1) + len)] = value;
	    bytesLeft--;
	  } else if ( ! bytesLeft && sequence > 2) {
	    checksum = value;
	    data[len + 3] = value;
	    if ( validateChecksum() ) {
			executeCommand();
	    } else {
			// Not a valid command, do nothing.
	    }
	    clearData();
	    sequence = 0;
	  } else { // Catch all.
	    clearData();
	    sequence = 0;
	  }
	}

### EEPROM layout

| Byte | Data | Explanation |
|--|--|--|
| 0-255 | Programs | As described above. |
| 257-508 | Reserved | |
| 509 | Program at start | Which program to run at start? 0-31 |
| 510 | Enable int0 | Enable interrupt on Arduino pin 2. |
| 511 | Version | Not implemented |

## C64
### Basic (yes, BASIC!) code
I'm using dustlayer as IDE so I'm not using row numbers in the example.

Setting speed to 10.

	rem set port to output
	poke 56579, 255 
	rem calculate checksum, 13 ^ 255 as described above.
	let chk = not 13 and 255
	let c = 256 - chk
	rem send header
	poke 56577, 255
	poke 56577, 153
	rem send length
	poke 56577, 2
	rem send command
	poke 56577, 3
	rem send data
	poke 56577, 10
	rem send checksum
	poke 56577, c
	rem port back to input.
	poke 56579, 0
	
	