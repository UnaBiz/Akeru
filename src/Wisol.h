//  Library for sending and receiving SIGFOX messages with Arduino shield based on Wisol WSSFM10R.
#ifndef UNABIZ_ARDUINO_WISOL_H
#define UNABIZ_ARDUINO_WISOL_H

#ifdef ARDUINO
  #if (ARDUINO >= 100)
    #include <Arduino.h>
  #else  //  ARDUINO >= 100
    #include <WProgram.h>
  #endif  //  ARDUINO  >= 100

  #ifdef CLION
    #include <src/SoftwareSerial.h>
  #else  //  CLION
    #if defined(ARDUINO_ARCH_SAMD)
      // SAMD boards do not have a standard SoftwareSerial library, but we can 
      // import a 3rd party one
      #include "SamdSoftwareSerial.h"
    #elif defined(BEAN_BEAN_BEAN_H)
      // Bean+ firmware 0.6.1 can't receive serial data properly. We provide
      // an alternative class BeanSoftwareSerial to work around this.
      // See SIGFOX.h.
    #else
      #include <SoftwareSerial.h>
    #endif // BEAN_BEAN_BEAN_H
  #endif  //  CLION

#else  //  ARDUINO
#endif  //  ARDUINO

const uint8_t WISOL_TX = 4;  //  Transmit port for For UnaBiz / Wisol Dev Kit
const uint8_t WISOL_RX = 5;  //  Receive port for UnaBiz / Wisol Dev Kit
const unsigned int WISOL_COMMAND_TIMEOUT = 60000;  //  Wait up to 60 seconds for response from SIGFOX module.  Includes downlink response.

class Wisol
{
public:
  Wisol(Country country, bool useEmulator, const String device, bool echo);
  Wisol(Country country, bool useEmulator, const String device, bool echo,
              uint8_t rx, uint8_t tx);
  Wisol(Country country, bool useEmulator, const String device, bool echo,
              HardwareSerial *serial);
  bool begin();
  void echoOn();  //  Turn on send/receive echo.
  void echoOff();  //  Turn off send/receive echo.
  void setEchoPort(Print *port);  //  Set the port for sending echo output.
  void echo(const String &msg);  //  Echo the debug message.
  bool isReady();
  bool sendMessage(const String &payload);  //  Send the payload of hex digits to the network, max 12 bytes.
  bool sendMessageAndGetResponse(const String &payload, String &response);  //  Send the payload of hex digits to the network and get response.
  bool sendString(const String &str);  //  Sending a text string, max 12 characters allowed.
  bool receive(String &data);  //  Receive a message.
  bool enterCommandMode();  //  Enter Command Mode for sending module commands, not data.
  bool exitCommandMode();  //  Exit Command Mode so we can send data.

  //  Commands for the module, must be run in Command Mode.
  bool getEmulator(int &result);  //  Return 0 if emulator mode disabled, else return 1.
  bool enableEmulator(String &result);  //  Enable emulator mode.
  bool disableEmulator(String &result);  //  Disable emulator mode.
  //  Get the frequency used for the SIGFOX module.
  bool getFrequency(String &result);
  //  Set the frequency for the SIGFOX module to Singapore frequency (RCZ4).
  bool setFrequencySG(String &result);
  //  Set the frequency for the SIGFOX module to Taiwan frequency (RCZ4).
  bool setFrequencyTW(String &result);
  //  Set the frequency for the SIGFOX module to ETSI frequency for Europe (RCZ1).
  bool setFrequencyETSI(String &result);
  //  Set the frequency for the SIGFOX module to US frequency (RCZ2).
  bool setFrequencyUS(String &result);
  //  Set the frequency for the SIGFOX module to Japan frequency (RCZ3).
  bool setFrequencyJP(String &result);
  bool writeSettings(String &result); //  Write frequency and other settings to flash memory of the module.
  bool reboot(String &result);  //  Reboot the SIGFOX module.
  bool getTemperature(float &temperature);
  bool getID(String &id, String &pac);  //  Get the SIGFOX ID and PAC for the module.
  bool getVoltage(float &voltage);

  bool setSleep();
  bool setWakeup();

  bool getHardware(String &hardware);
  bool getFirmware(String &firmware);
  bool getPower(int &power);
  bool setPower(int power);
  bool getParameter(uint8_t address, String &value);  //  Return the parameter at that address.

  //  Message conversion functions.
  String toHex(int i);
  String toHex(unsigned int i);
  String toHex(long l);
  String toHex(unsigned long ul);
  String toHex(float f);
  String toHex(double d);
  String toHex(char c);
  String toHex(char *c, int length);

private:
  bool sendCommand(const String &cmd, uint8_t expectedMarkers,
                   String &result, uint8_t &actualMarkers);
  bool sendBuffer(const String &buffer, int timeout, uint8_t expectedMarkers,
                  String &dataOut, uint8_t &actualMarkers);
  bool setFrequency(int zone, String &result);
  uint8_t hexDigitToDecimal(char ch);
  void logBuffer(const __FlashStringHelper *prefix, const char *buffer,
                 uint8_t markerPos[], uint8_t markerCount);

  int zone;  //  1 to 4 representing SIGFOX frequencies RCZ 1 to 4.
  Country country;   //  Country to be set for SIGFOX transmission frequencies.
  bool useEmulator;  //  Set to true if using UnaBiz Emulator.
  String device;  //  Name of device if using UnaBiz Emulator.
  Stream *serialPort;  //  Serial port for the SIGFOX module.
  SoftwareSerial *swSerialPort;
  HardwareSerial *hwSerialPort;
  Print *echoPort;  //  Port for sending echo output.  Defaults to Serial.
  Print *lastEchoPort;  //  Last port used for sending echo output.
  unsigned long lastSend;  //  Timestamp of last send.
  bool setOutputPower();
};

#endif // UNABIZ_ARDUINO_WISOL_H
