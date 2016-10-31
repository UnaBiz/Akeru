//  radiocrafts-test
//  Send a sample message to SIGFOX base station from RC1692HP-SIG.
//  Based on https://github.com/Snootlab/Akeru

#include "Radiocrafts.h"

// TD1208 Sigfox module IO definition
/*   Snootlab device | TX | RX
               Radiocrafts | D4 | D5
               Akene | D5 | D4
            Breakout | your pick */
//#define AKENE_TX 5  //  For UnaBiz / Akene
//#define AKENE_RX 4  //  For UnaBiz / Akene

#define RADIOCRAFTS_TX 4  //  For UnaBiz / Radiocrafts Dev Kit
#define RADIOCRAFTS_RX 5  //  For UnaBiz / Radiocrafts Dev Kit

// Sigfox instance management 
Radiocrafts Radiocrafts(RADIOCRAFTS_RX, RADIOCRAFTS_TX);

void setup()
{
  // initialize console serial communication at 9600 bits per second:
  Serial.begin(9600);
  Serial.println("Demo sketch for Radiocrafts library :)");
  
  // Check TD1208 communication
  if (!Radiocrafts.begin())
  {
    Serial.println("TD1208 KO");
    while(1);
  }
  Radiocrafts.echoOn(); // comment this line to hide AT commands
}

void loop()
{
  String result = "";
  //  Enter command mode.  TODO: Confirm response = '>'
  Serial.println(F("\nEntering command mode (expecting '>')"));
  Radiocrafts.enterCommandMode();

  //  Disable emulation mode.
  Serial.println(F("\nDisable emulation mode"));
  Radiocrafts.disableEmulator(result);

  //  Check whether emulator is used for transmission.
  Serial.println(F("\nEmulator Enabled (expecting 0) = ")); int emulator = 0;
  Radiocrafts.getEmulator(emulator);

  //  Get network mode for transmission.  TODO: Confirm network mode = 0 for uplink only, no downlink.
  Serial.println(F("\nNetwork Mode (expecting 0) = "));
  Radiocrafts.getParameter(0x3b, result);
  
  //  Get baud rate.  TODO: Confirm baud rate = 5 for 19200 bps.
  Serial.println(F("\nBaud Rate (expecting 5) = "));
  Radiocrafts.getParameter(0x30, result);
  
  //  Set the frequency of SIGFOX module to SG/TW.
  Serial.println(F("\nSetting frequency "));
  result = "";
  Radiocrafts.setFrequencySG(result);
  Serial.print(F("Set frequency result = "));
  Serial.println(result);

  //  Get and display the frequency used by the SIGFOX module.  TODO: Confirm that it returns 3 for RCZ4 (SG/TW).
  Serial.println(F("\nGetting frequency "));
  String frequency = "";
  Radiocrafts.getFrequency(frequency);
  Serial.print(F("Frequency (expecting 3) = "));
  Serial.println(frequency);

  // Read module temperature
  Serial.println(F("\nGetting temperature "));
  int temperature = 0;
  if (Radiocrafts.getTemperature(temperature))
  {
    Serial.print(F("Temperature = "));
    Serial.print(temperature);
    Serial.println(F(" C"));
  }
  else
  {
    Serial.println(F("Temperature KO"));
  }

  // Read module supply voltage
  Serial.println(F("\nGetting voltage "));
  float voltage = 0.0;
  if (Radiocrafts.getVoltage(voltage))
  {
    Serial.print(F("Supply voltage = "));
    Serial.print(voltage);
    Serial.println(F(" V"));
  }
  else
  {
    Serial.println(F("Supply voltage KO"));
  }

  // Read module identification
  // Returns with 12 bytes: 4 bytes ID (LSB first) and 8 bytes PAC (MSB first).
  String id = "";
  Serial.println(F("\nGetting ID "));
  if (Radiocrafts.getID(id))
  {
    Serial.print(F("\n4 bytes ID (LSB first) and 8 bytes PAC (MSB first) = "));
    Serial.println(id);
  }
  else
  {
    Serial.println(F("ID KO"));
  }

  // Read module hardware version
  Serial.println(F("\nGetting hardware "));
  String hardware = "";
  if (Radiocrafts.getHardware(hardware))
  {
    Serial.print(F("Hardware version = "));
    Serial.println(hardware);
  }
  else
  {
    Serial.println(F("Hardware version KO"));
  }

  // Read module firmware version
  Serial.println(F("\nGetting firmware "));
  String firmware = "";
  if (Radiocrafts.getFirmware(firmware))
  {
    Serial.print(F("Firmware version = "));
    Serial.println(firmware);
  }
  else
  {
    Serial.println(F("Firmware version KO"));
  }

  // Read power 
  Serial.println(F("\nGetting power "));
  int power = 0;
  if (Radiocrafts.getPower(power))
  {
    Serial.print(F("Power level = "));
    Serial.print(power);
    Serial.println(F(" dB"));
  }
  else
  {
    Serial.println(F("Power level KO"));
  }

  //  Exit command mode.
  Radiocrafts.exitCommandMode();

  // Convert to hexadecimal before sending
  String temp = Radiocrafts.toHex(temperature);
  String volt = Radiocrafts.toHex(voltage);
  
  String msg = temp + volt; // Put everything together

  for (int i = 0; i < 10; i++) {
    Serial.println(F("\nSending payload "));
    if (Radiocrafts.sendPayload(msg))
    {
      Serial.println(F("Message sent !"));
    }
    else
    {
      Serial.println(F("Message not sent !"));
    }
    delay(6000);
  }

  // End of tests
  while (1);
}

/*
Demo sketch for Radiocrafts library :)

}} 
}} [00]
{{ >
{{ [3e]

}} Y(
}} [59 28]
{{  >
{{ [00 3e]

}} Y;
}} [59 3b]
{{  >
{{ [00 3e]

}} Y0
}} [59 30]
{{ >
{{ [05 3e]

}} M
}} [4d 00 03]
{{ 
{{ []

}} �
}} [ff]
{{ >
{{ [3e]
Set frequency result = 

}} Y
}} [59 00]
{{ >
{{ [03 3e]
Frequency = 00

}} U
}} [55]
{{ �>
{{ [9d 3e]
Temperature = 0 C

}} V
}} [56]
{{ t>
{{ [74 3e]
Supply voltage = 0.00 V

}} 9
}} [39]
{{ e� �,�@c�>
{{ [65 86 1c 00 ca 2c 11 f3 a4 40 63 f8 3e]
ID = �
Hardware version = TODO
Firmware version = TODO

}} Y
}} [59 01]
{{  >
{{ [00 3e]
Power level = 0 dB

}} X
}} [58]
{{ 
{{ []
sendPayload: 000000000000
sendPayload2: [ 0 0 0 0 0 0]

}}  0 00 
}} [06 00 00 00 00 33 20]
{{ 
{{ []
Message not sent !
sendPayload: 000000000000
sendPayload2: [ 0 0 0 0 0 0]

}}   00 0
}} [06 00 00 00 00 33 20]
{{ 
{{ []
Message not sent !
sendPayload: 000000000000
sendPayload2: [ 0 0 0 0 0 0]

}}   00 0
}} [06 00 00 00 00 33 20]
{{ 
{{ []
Message not sent !
sendPayload: 000000000000
sendPayload2: [ 0 0 0 0 0 0]

}}   00 0
}} [06 00 00 00 00 33 20]
{{ 
{{ []
Message not sent !
sendPayload: 000000000000
sendPayload2: [ 0 0 0 0 0 0]

}}   00 0
}} [06 00 00 00 00 33 20]
{{ 
{{ []
Message not sent !
sendPayload: 000000000000
sendPayload2: [ 0 0 0 0 0 0]

}}   00 0
}} [06 00 00 00 00 33 20]
{{ 
{{ []
Message not sent !
sendPayload: 000000000000
sendPayload2: [ 0 0 0 0 0 0]

}}   00 0
}} [06 00 00 00 00 33 20]
{{ 
{{ []
Message not sent !
sendPayload: 000000000000
sendPayload2: [ 0 0 0 0 0 0]

}}   00 0
}} [06 00 00 00 00 33 20]
{{ 
{{ []
Message not sent !
sendPayload: 000000000000
sendPayload2: [ 0 0 0 0 0 0]

}}   00 0
}} [06 00 00 00 00 33 20]
{{ 
{{ []
Message not sent !
sendPayload: 000000000000
sendPayload2: [ 0 0 0 0 0 0]

}}   00 0
}} [06 00 00 00 00 33 20]
{{ 
{{ []
Message not sent !

*/
