#include <EEPROM.h>

#define DEBUG

/*
   LED64


  Protocol:


  0 - 1 : Header FF99
  2     : Data length, CMD + data
  3 .   : CMD
  4 - n : Data, 27 bytes max.
  n + 1 : Checksum 0x100 - ((CMD + data) ^ 0xff)


  FF  99  01 10 EF
  255 153 01 16 239


  CMDs:
        0x01 - set RGB 3 bytes
        0x02 - set mode 1 byte
             mode:
                   0x00 - static
                   0x01 - blink
                   0x02 - fade
                   0x03 - cycle
        0x03 - set speed 1 byte - 10000 / speed  millis.
        0x04 - set program 1 byte
        0x05 - save to program 1 byte
        0x06 - set color max 2 bytes
               color (red = 0), value
        0x07 - set color min 2 bytes
               color (red = 0), value
        0x08 - set program 8 bytes.

        No data commands.
        0x10 - next mode.
        0x11 - prev mode.
        0x12 - next program.
        0x13 - prev program.
        0x14 - Red + 1
        0x15 - Red - 1
        0x16 - Green + 1
        0x17 - Green - 1
        0x18 - Blue + 1
        0x19 - Blue + 1
        0x1A - Red + 10
        0x1B - Red - 10
        0x1C - Green + 10
        0x1D - Green - 10
        0x1E - Blue + 10
        0x1F - Blue + 10
        0x20 - Speed + 1
        0x21 - Speed - 1
        0x22 - Speed + 10
        0x23 - Speed - 10



        0x24 - save current program.



        0xFF - direct write to EEPROM 2 byte (addr + data)


  32 programs eeprom 0-31 * 8 = 256 bytes in EEPROM
  Program: 8 bytes
    Rmax
    Rmin
    Gmax
    Gmin
    Bmax
    Bmin
    Mode
    Speed


  EEPROM layout:
    0 - 255 : Programs.
    256-511: Config data.



  Config:
  256: enable int0 (pin2). 1 byte



*/



byte value = 0;

volatile byte data[32];
volatile byte sequence = 0;
byte headerOk = 0;
byte len = 0;
byte bytesLeft = 0;
byte checksum = 0;
byte speed = 10;

byte mode = 0;

byte redMax = 255;
byte greenMax = 0;
byte blueMax = 0;
byte redMin;
byte greenMin;
byte blueMin;
byte program;
byte maxMode = 1;
byte programData[8];

volatile byte exitLoop = 0;

volatile unsigned long int0Pressed = 0;



void setup() {
  if ( EEPROM.read(256) )
    attachInterrupt(digitalPinToInterrupt(2), restoreButton, FALLING); // Connecteded to PC pin.

  attachInterrupt(digitalPinToInterrupt(3), readData, FALLING); // Connecteded to PC pin.
  pinMode(9, OUTPUT); // Red
  pinMode(10, OUTPUT); // Green
  pinMode(11, OUTPUT); // Blue



  for ( int cnt = 0; cnt < 6; cnt++ ) {
    analogWrite(9, 255);
    analogWrite(10, 0);
    analogWrite(11, 0);

    delay(100);
    analogWrite(9, 0);
    delay(100);
  }

#ifdef DEBUG
  Serial.begin(115200);
  Serial.println("Begin..");
  delay(400);
#endif
}

void loop() {
  exitLoop = 0; // Used in the different modes.
  switch (mode) {
    case 0:
      modeZero();
      break;
    case 1:
      modeOne();
      break;
    default:
      fallbackMode();
      break;
  }
}

void modeZero() {
#ifdef DEBUG
  Serial.println("Mode zero");
#endif
  analogWrite(9, redMax);
  analogWrite(10, greenMax);
  analogWrite(11, blueMax);
  while ( ! exitLoop ); // wait for next command.
}

void modeOne() {
  // Blink mode
#ifdef DEBUG
  Serial.println("Mode one");
#endif
  unsigned long last = millis();
  int ledStatus = 0;
  while ( ! exitLoop ) {
    if ( millis() - last >= (10000 / speed))  {
      if ( ledStatus ) {
        analogWrite(9, 1);
        analogWrite(10, 1);
        analogWrite(11, 1);
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

void fallbackMode() {
#ifdef DEBUG
  Serial.println("Mode fallback");
#endif
  analogWrite(9, 255);
  analogWrite(10, 0);
  analogWrite(11, 0);
  while ( ! exitLoop );
}


void readData() {
  value = (PIND & 0xF0) | (PINC & 0x0F);

#ifdef DEBUG
  Serial.println("PC low!");
  Serial.println(sequence);
  Serial.println(value, BIN);
#endif
  if ( ! sequence && value == 0xff ) {
    sequence++;
    data[0] = value;
#ifdef DEBUG

    Serial.println("Read first header byte ok");
#endif
  } else if ( sequence == 1 ) {
    if ( value == 0x99 ) {
      sequence++;
      data[1] = value;

#ifdef DEBUG
      Serial.println("Read second header byte ok");
#endif
      headerOk = 1;
    } else if ( value = 0xff ) {
      // Just in case..
#ifdef DEBUG
      Serial.println("Got 0xff, skipping sequence stepping.");
#endif
    } else {
      sequence++;
    }
  } else if ( sequence == 2 && headerOk ) {


    sequence++;
    len = value;
    bytesLeft = value;
    data[2] = value;
    if ( len > 27 ) {
      clearData();
      sequence = 0;
    }
#ifdef DEBUG
    Serial.print("Got length: ");
    Serial.println(data[2], DEC);
#endif
  } else if ( bytesLeft && sequence > 2) {
    data[3 + ((bytesLeft * -1) + len)] = value;
    bytesLeft--;
#ifdef DEBUG

    Serial.print("Got value: ");
    Serial.println(value, DEC);
    Serial.print("Bytes left to read: ");
    Serial.println(bytesLeft, DEC);
#endif
  } else if ( ! bytesLeft && sequence > 2) {
    checksum = value;
    data[len + 3] = value;
    Serial.println("Done reading! Executing!");
    if ( validateChecksum() ) {
      executeCommand();

    } else {
#ifdef DEBUG
      Serial.println("Command ****NOT**** validated!");
      Serial.println("Data: ");
      Serial.println("0\t1\t2\t3\t4\t5\t6\t7\t8\t9\t10\t11\t12\t13\t14\t15\t16\t17\t18\t19\t20\t21\t22\t23\t24\t25\t26\t27\t28\t29\t30\t31");

      for ( int i = 0; i < 32; i++) {
        Serial.print(data[i]);
        Serial.print("\t");
      }
      Serial.println("");
#endif
    }

    clearData();
    sequence = 0;
  } else {
#ifdef DEBUG

    Serial.println("Clearing data.");
#endif
    clearData();
    sequence = 0;
  }
}

void executeCommand() {
  byte tempVal;
  Serial.println("Executing..");
  Serial.println("0\t1\t2\t3\t4\t5\t6\t7\t8\t9\t10\t11\t12\t13\t14\t15\t16\t17\t18\t19\t20\t21\t22\t23\t24\t25\t26\t27\t28\t29\t30\t31");

  for ( int i = 0; i < 32; i++) {
    Serial.print(data[i]);
    Serial.print("\t");
  }
  Serial.println("");
  switch ( data[3] ) {
    case 0x01:
      // Direct write RGB
      /*     analogWrite(9, data[4]);
           analogWrite(10, data[5]);
           analogWrite(11, data[6]);
      */
      redMax = data[4];
      greenMax = data[5];
      blueMax = data[6];

      break;

    case 0x02:
      // Set mode
      mode = data[4];
      break;
    case 0x03:
      //Set speed
      speed = data[4];
      break;
    case 0x04:
      // Select program
      program = data[4];
      break;
    case 0x05:
      // Write to program
      programData[0] = redMax;
      programData[1] = redMin;
      programData[2] = greenMax;
      programData[3] = greenMin;
      programData[4] = blueMax;
      programData[5] = blueMin;
      programData[6] = mode;
      programData[7] = speed;
      EEPROM.put(data[4] * 8, programData);
      Serial.print("Wrote: ");
      for ( int i = 0; i < 8; i++ ) {
        Serial.print(programData[i]);
        Serial.print(" ");
      }
      Serial.print(" to program: ");
      Serial.println(data[4]);
      break;
    case 0x06:
      // Set color max
      switch ( data[4] ) {
        case 0:
          redMax = data[5];
          break;
        case 1:
          greenMax = data[5];
          break;
        case 2:
          blueMax = data[5];
          break;
      }
      break;
    case 0x07:
      // Set color min
      switch ( data[4] ) {
        case 0:
          redMin = data[5];
          break;
        case 1:
          greenMin = data[5];
          break;
        case 2:
          blueMin = data[5];
          break;
      }
      break;
    case 0x08:
      redMax = data[4];
      redMin = data[5];
      greenMax = data[6];
      greenMin = data[7];
      blueMax = data[8];
      blueMin = data[9];
      mode = data[10];
      speed = data[11];
      break;
    case 0x10:
      // Next mode
      if ( mode == maxMode )
        mode = 0;
      else
        mode++;
      break;

    case 0x11:
      // Previous mode
      if ( !mode )
        mode = maxMode;
      else
        mode--;
      break;

    case 0x12:
      // Next program
      if ( program == 31 )
        program = 0;
      else
        program++;
      break;
    case 0x13:
      // Previous program
      if ( !program )
        program = 31;
      else
        program--;
      break;

    // 0x14 - 0x1F Color increase / descrease 1 or 10 steps.
    case 0x14:
      tempVal = analogRead(9);
      analogWrite(9, tempVal++);
      break;

    case 0x15:
      tempVal = analogRead(9);
      analogWrite(9, tempVal--);
      break;

    case 0x16:
      tempVal = analogRead(10);
      analogWrite(10, tempVal++);
      break;
    case 0x17:
      tempVal = analogRead(10);
      analogWrite(10, tempVal--);
      break;
    case 0x18:
      tempVal = analogRead(11);
      analogWrite(11, tempVal++);
      break;
    case 0x19:
      tempVal = analogRead(11);
      analogWrite(11, tempVal--);
      break;


    case 0x1A:
      tempVal = analogRead(9);
      if ( tempVal < 246 )
        analogWrite(9, tempVal + 10);
      break;

    case 0x1B:
      tempVal = analogRead(9);
      if ( tempVal > 9 )
        analogWrite(9, tempVal - 10);
      break;

    case 0x1C:
      tempVal = analogRead(10);
      if ( tempVal < 246 )
        analogWrite(10, tempVal + 10);
      break;

    case 0x1D:
      tempVal = analogRead(10);
      if ( tempVal > 9 )
        analogWrite(10, tempVal - 10);
      break;
    case 0x1E:
      tempVal = analogRead(11);
      if ( tempVal < 246 )
        analogWrite(11, tempVal + 10);
      break;

    case 0x1F:
      tempVal = analogRead(11);
      if ( tempVal > 9 )
        analogWrite(11, tempVal - 10);
      break;

    case 0x20:
      if ( speed < 255 )
        speed = speed++;
      break;
    case 0x21:
      if ( speed )
        speed = speed--;
      break;

    case 0x22:
      if ( speed < 246 )
        speed = speed + 10;
      break;
    case 0x23:
      if (speed > 9 )
        speed = speed - 10;
      break;

    case 0x24:
      // Save current program.
      programData[0] = redMax;
      programData[1] = redMin;
      programData[2] = greenMax;
      programData[3] = greenMin;
      programData[4] = blueMax;
      programData[5] = blueMin;
      programData[6] = mode;
      programData[7] = speed;
      EEPROM.put(program * 8, programData);
      break;

    case 0xff:
      EEPROM.write(data[4], data[5]);
      break;


    default:
      Serial.println("Unknown command!");
      break;
  }
#ifdef DEBUG
  Serial.println("End switch");
#endif
  exitLoop = 1;
}




byte validateChecksum() {
  byte temp = 0;
#ifdef DEBUG

  Serial.println("Validating.");
  Serial.print("Incoming checksum: ");
  Serial.println(data[len + 3], DEC);
#endif
  for ( int i = 3; i < len + 3; i++ ) {
    temp = temp + data[i];
#ifdef DEBUG
    Serial.print(i);
    Serial.print(": ");
    Serial.println(data[i], DEC);
#endif
  }
#ifdef DEBUG
  Serial.println(temp, DEC);
  Serial.print("Calculated checksum: ");
  Serial.println((0x100 - (temp ^ 0xff)), DEC);
#endif
  if ( ! (0x100 - (temp ^ 0xff)) - data[len + 3])
    return 1;
  else
    return 0;
}



void clearData() {
  for ( int i = 0; i < 32; i++)
    data[i] = 0;
}

void restoreButton() {

}
