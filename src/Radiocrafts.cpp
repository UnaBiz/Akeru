//  Library for sending and receiving SIGFOX messages with Arduino shield based on Radiocrafts RC1692HP-SIG.
#ifdef ARDUINO
  #if (ARDUINO >= 100)
    #include <Arduino.h>
  #else  //  ARDUINO >= 100
    #include <WProgram.h>
  #endif  //  ARDUINO  >= 100
#endif  //  ARDUINO

#include "SIGFOX.h"

//  Use a macro for logging because Flash strings not supported with String class in Bean+
#define log1(x) { echoPort->println(x); }
#define log2(x, y) { echoPort->print(x); echoPort->println(y); }
// #define log3(x, y, z) { echoPort->print(x); echoPort->print(y); echoPort->println(z); }
#define log4(x, y, z, a) { echoPort->print(x); echoPort->print(y); echoPort->print(z); echoPort->println(a); }

#define MODEM_BITS_PER_SECOND 19200
#define END_OF_RESPONSE '>'  //  Character '>' marks the end of response.
#define CMD_READ_MEMORY 'Y'  //  'Y' to read memory.
#define CMD_ENTER_CONFIG 'M'  //  'M' to enter config mode.
#define CMD_EXIT_CONFIG (char) 0xff  //  Exit config mode.

static NullPort nullPort;

/* TODO: Run some sanity checks to ensure that Radiocrafts module is configured OK.
  //  Get network mode for transmission.  Should return network mode = 0 for uplink only, no downlink.
  Serial.println(F("\nGetting network mode (expecting 0)..."));
  transceiver.getParameter(0x3b, result);

  //  Get baud rate.  Should return baud rate = 5 for 19200 bps.
  Serial.println(F("\nGetting baud rate (expecting 5)..."));
  transceiver.getParameter(0x30, result);
*/

Radiocrafts::Radiocrafts(Country country0, bool useEmulator0, const String device0, bool echo):
    Radiocrafts(country0, useEmulator0, device0, echo, RADIOCRAFTS_RX, RADIOCRAFTS_TX) {}  //  Forward to constructor below.

Radiocrafts::Radiocrafts(Country country0, bool useEmulator0, const String device0, bool echo,
                         uint8_t rx, uint8_t tx) {
  //  Init the module with the specified transmit and receive pins.
  //  Default to no echo.
  mode = SEND_MODE;
  country = country0;
  useEmulator = useEmulator0;
  device = device0;
  //  Bean+ firmware 0.6.1 can't receive serial data properly. We provide
  //  an alternative class BeanSoftwareSerial to work around this.
  //  For Bean, SoftwareSerial is a #define alias for BeanSoftwareSerial.
  serialPort = new SoftwareSerial(rx, tx);
  if (echo) echoPort = &Serial;
  else echoPort = &nullPort;
  lastEchoPort = &Serial;
}

bool Radiocrafts::begin() {
  //  Wait for the module to power up, configure transmission frequency.
  //  Return true if module is ready to send.
  lastSend = 0;
  for (int i = 0; i < 5; i++) {
    //  Retry 5 times.
#ifdef BEAN_BEAN_BEAN_H
    Bean.sleep(7000);  //  For Bean, delay longer to allow Bluetooth debug console to connect.
#else  // BEAN_BEAN_BEAN_H
    delay(2000);
#endif // BEAN_BEAN_BEAN_H
    String result;
    if (useEmulator) {
      //  Emulation mode.
      if (!enableEmulator(result)) continue;
    } else {
      //  Disable emulation mode.
      log1(F(" - Disabling emulation mode..."));
      if (!disableEmulator(result)) continue;

      //  Check whether emulator is used for transmission.
      log1(F(" - Checking emulation mode (expecting 0)...")); int emulator = 0;
      if (!getEmulator(emulator)) continue;
    }

    //  Read SIGFOX ID and PAC from module.
    log1(F(" - Getting SIGFOX ID..."));  String id, pac;
    if (!getID(id, pac)) continue;
    echoPort->print(F(" - SIGFOX ID = "));  Serial.println(id);
    echoPort->print(F(" - PAC = "));  Serial.println(pac);

    //  Set the frequency of SIGFOX module.
    log2(F(" - Setting frequency for country "), (int) country);
    if (country == COUNTRY_US) {  //  US runs on different frequency (RCZ2).
      if (!setFrequencyUS(result)) continue;
    } else if (country == COUNTRY_FR) {  //  France runs on different frequency (RCZ1).
      if (!setFrequencyETSI(result)) continue;
    } else { //  Rest of the world runs on RCZ4.
      if (!setFrequencySG(result)) continue;
    }
    log2(F(" - Set frequency result = "), result);

    //  Get and display the frequency used by the SIGFOX module.  Should return 3 for RCZ4 (SG/TW).
    log1(F(" - Getting frequency (expecting 3)..."));  String frequency;
    if (!getFrequency(frequency)) continue;
    log2(F(" - Frequency (expecting 3) = "), frequency);
    return true;  //  Init module succeeded.
  }
  return false;  //  Failed to init module.
}

bool Radiocrafts::sendMessage(const String &payload) {
  //  Payload contains a string of hex digits, up to 24 digits / 12 bytes.
  //  We convert to binary and send to SIGFOX.  Return true if successful.
  //  We represent the payload as hex instead of binary because 0x00 is a
  //  valid payload and this causes string truncation in C libraries.
  //  Assumes we are in Send Mode.
  log2(F(" - Radiocrafts.sendMessage: "), device + ',' + payload);
  if (!isReady()) return false;  //  Prevent user from sending too many messages without sufficient delay.

  //  Decode and send the data.
  //  First byte is payload length, followed by rest of payload.
  String message = toHex((char) (payload.length() / 2)) + payload, data;
  uint8_t markers = 0;
  if (sendBuffer(message, COMMAND_TIMEOUT, 0, data, markers)) {  //  No markers expected.
    log1(data);
    lastSend = millis();
    return true;
  }
  return false;
}

bool Radiocrafts::sendCommand(const String &cmd, uint8_t expectedMarkerCount,
                              String &result, uint8_t &actualMarkerCount) {
  //  Send a Radiocrafts command in Command Mode.
  //  Switches to Command Mode and returns to Send Mode after sending.
  //  Device must be already in Send Mode.
  //  cmd contains a string of hex digits, up to 24 digits / 12 bytes.
  //  We convert to binary and send to SIGFOX.  Return true if successful.
  String data;
  //  Enter command mode.
  if (!enterCommandMode()) return false;
  bool status = sendBuffer(cmd, COMMAND_TIMEOUT, expectedMarkerCount,
    data, actualMarkerCount);
  if (status) result = data;
  //  Always exit command mode so that the device is normally in send mode.
  if (!exitCommandMode()) return false;
  return status;
}

bool Radiocrafts::sendConfigCommand(const String &cmd, String &result) {
  //  Send a Radiocrafts config command in Config Mode.
  //  Switches to Config Mode and returns to Send Mode after sending.
  //  Device must be already in Send Mode.
  //  cmd contains a string of hex digits, up to 24 digits / 12 bytes.
  //  We convert to binary and send to SIGFOX.  Return true if successful.
  String data;
  //  Enter config mode.
  if (!enterConfigMode()) return false;
  uint8_t actualMarkerCount = 0;
  bool status = sendBuffer(cmd, COMMAND_TIMEOUT, 0,
                           data, actualMarkerCount);
  if (status) result = data;
  //  Always exit config mode so that the device is normally in send mode.
  if (!exitConfigMode()) return false;
  return status;
}

//  Remember where in response the '>' markers were seen.
const uint8_t markerPosMax = 5;
static uint8_t markerPos[markerPosMax];

bool Radiocrafts::sendBuffer(const String &buffer, const int timeout,
                             uint8_t expectedMarkerCount, String &response,
                             uint8_t &actualMarkerCount) {
  //  buffer contains a string of hex digits, up to 24 digits / 12 bytes.
  //  We convert to binary and send to SIGFOX.  Return true if successful.
  //  We represent the payload as hex instead of binary because 0x00 is a
  //  valid payload and this causes string truncation in C libraries.
  //  expectedMarkerCount is the number of end-of-command markers '>' we
  //  expect to see.  actualMarkerCount contains the actual number seen.
  log2(F(" - Radiocrafts.sendBuffer: "), buffer);
  response = "";
  if (useEmulator) return true;

  actualMarkerCount = 0;
  //  Start serial interface.
  serialPort->begin(MODEM_BITS_PER_SECOND);
#ifdef BEAN_BEAN_BEAN_H
  Bean.sleep(200);
#else  // BEAN_BEAN_BEAN_H
  delay(200);
#endif // BEAN_BEAN_BEAN_H
  serialPort->flush();
  serialPort->listen();

  //  Send the buffer: need to write/read char by char because of echo.
  const char *rawBuffer = buffer.c_str();
  //  Send buffer and read response.  Loop until timeout or we see the end of response marker.
  unsigned long startTime = millis(); int i = 0;
  //  Previous code for verifying that data was sent correctly.
  //static String echoSend = "", echoReceive = "";
  for (;;) {
    //  If there is data to send, send it.
    if (i < buffer.length()) {
      //  Convert 2 hex digits to 1 char and send.
      uint8_t txChar = hexDigitToDecimal(rawBuffer[i]) * 16 +
                       hexDigitToDecimal(rawBuffer[i + 1]);
      //echoSend.concat(toHex((char) txChar) + ' ');
      serialPort->write(txChar);
#ifdef BEAN_BEAN_BEAN_H
      Bean.sleep(10);
#else  // BEAN_BEAN_BEAN_H
      delay(10);  //  Need to wait a while because SoftwareSerial has no FIFO and may overflow.
#endif // BEAN_BEAN_BEAN_H
      i = i + 2;
      startTime = millis();  //  Start the timer only when all data has been sent.
    }

    //  If timeout, quit.
    const unsigned long currentTime = millis();
    if (currentTime - startTime > timeout) break;

    //  If data is available to receive, receive it.
    if (serialPort->available() > 0) {
      int rxChar = serialPort->read();
      //  echoReceive.concat(toHex((char) rxChar) + ' ');
      if (rxChar == -1) continue;
      if (rxChar == END_OF_RESPONSE) {
        if (actualMarkerCount < markerPosMax)
          markerPos[actualMarkerCount] = response.length();  //  Remember the marker pos.
        actualMarkerCount++;  //  Count the number of end markers.
        if (actualMarkerCount >= expectedMarkerCount) break;  //  Seen all markers already.
      } else {
        response.concat(toHex((char) rxChar));
      }
    }

    //  TODO: Check for downlink response.

  }
  serialPort->end();
  //  Log the actual bytes sent and received.
  //log2(F(">> "), echoSend);
  //  if (echoReceive.length() > 0) { log2(F("<< "), echoReceive); }
  logBuffer(F(">> "), rawBuffer, 0, 0);
  logBuffer(F("<< "), response.c_str(), markerPos, actualMarkerCount);

  //  If we did not see the terminating '>', something is wrong.
  if (actualMarkerCount < expectedMarkerCount) {
    if (response.length() == 0) {
      log1(F(" - Radiocrafts.sendBuffer: Error: No response"));  //  Response timeout.
    } else {
      log2(F(" - Radiocrafts.sendBuffer: Error: Unknown response: "), response);
    }
    return false;
  }
  log2(F(" - Radiocrafts.sendBuffer: response: "), response);
  //  TODO: Parse the downlink response.
  return true;
}

bool Radiocrafts::sendString(const String &str) {
  //  For convenience, allow sending of a text string with automatic encoding into bytes.  Max 12 characters allowed.
  //  Convert each character into 2 bytes.
  log2(F(" - Radiocrafts.sendString: "), str);
  String payload;
  for (unsigned i = 0; i < str.length(); i++) {
    char ch = str.charAt(i);
    payload.concat(toHex(ch));
  }
  //  Send the encoded payload.
  return sendMessage(payload);
}

bool Radiocrafts::isReady()
{
  // Check the duty cycle and return true if we can send data.
  // IMPORTANT WARNING. PLEASE READ BEFORE MODIFYING THE CODE
  //
  // The Sigfox network operates on public frequencies. To comply with
  // radio regulation, it can send radio data a maximum of 1% of the time
  // to leave room to other devices using the same frequencies.
  //
  // Sending a message takes about 6 seconds (it's sent 3 times for
  // redundancy purposes), meaning the interval between messages should
  // be 10 minutes.
  //
  // Also make sure your send rate complies with the restrictions set
  // by the particular subscription contract you have with your Sigfox
  // network operator.
  //
  // FAILING TO COMPLY WITH THESE CONSTRAINTS MAY CAUSE YOUR MODEM
  // TO BE BLOCKED BY YOUR SIFGOX NETWORK OPERATOR.
  //
  // You've been warned!

  unsigned long currentTime = millis();
  if (lastSend == 0) return true;  //  First time sending.
  const unsigned long elapsedTime = currentTime - lastSend;
  //  For development, allow sending every 2 seconds.
  if (elapsedTime <= 2 * 1000) {
    log1(F("***MESSAGE NOT SENT - Must wait 2 seconds before sending the next message"));
    return false;
  }  //  Wait before sending.
  if (elapsedTime <= SEND_DELAY)
    log1(F("Warning: Should wait 10 mins before sending the next message"));
  return true;
}

static String data;  //  Used by all functions except enter/exit command/config mode.
static String modeData;  //  Used by enter/exit command/config mode only.

bool Radiocrafts::enterCommandMode() {
  //  Enter Command Mode for sending module commands, not data.  Assumes we are in Send Mode.
  log1(F(" - Entering command mode..."));
  //  Confirm we are in SEND_MODE
  if (mode != SEND_MODE) {
    log1(F(" - Warning: Radiocrafts.enterCommandMode did not detect expected Send Mode, may be in incorrect mode"));
  }
  uint8_t markers = 0;
  if (!sendBuffer("00", COMMAND_TIMEOUT, 1, modeData, markers)) return false;
  //  Confirm response = '>'
  if (modeData != String("") || markers != 1) {
    log1(F(" - Warning: Radiocrafts.enterCommandMode did not receive expected '>', may be in incorrect mode"));
  }
  mode = COMMAND_MODE;
  log1(F(" - Radiocrafts.enterCommandMode: OK "));
  return true;
}

bool Radiocrafts::exitCommandMode() {
  //  Exit Command Mode and return to Send Mode so we can send data.  Assumes we are in Command Mode.
  log1(F(" - Exiting command mode..."));
  //  Confirm we are in COMMAND_MODE.
  if (mode != COMMAND_MODE) {
    log1(F(" - Warning: Radiocrafts.exitCommandMode did not detect expected Command Mode, may be in incorrect mode"));
  }
  for (;;) {
    //  Keep sending the exit command until we are really sure.  Sometimes we might out of sync.
    uint8_t markers = 0;
    if (!sendBuffer(toHex('X'), COMMAND_TIMEOUT, 0, modeData, markers)) return false;
    if (modeData == String("") && markers == 0) break;
    log1(F(" - Warning: Radiocrafts.exitCommandMode resending exit command, may be in incorrect mode"));
  }
  mode = SEND_MODE;
  log1(F(" - Radiocrafts.exitCommandMode: OK "));
  return true;
}

bool Radiocrafts::enterConfigMode() {
  //  Enter Config Mode for setting config.  Assumes we are in Send Mode.
  //  Device is normally in Send Mode.  We switch to Command Mode first.
  if (!enterCommandMode()) return false;
  //  Confirm we are in COMMAND_MODE
  if (mode != COMMAND_MODE) {
    log1(F(" - Warning: Radiocrafts.enterConfigMode did not detect expected Command Mode, may be in incorrect mode"));
  }
  //  Now switch from Command Mode to Config Mode.
  log1(F(" - Entering config mode from send mode..."));
  uint8_t markers = 0;
  if (!sendBuffer(toHex(CMD_ENTER_CONFIG), COMMAND_TIMEOUT, 1, modeData, markers)) return false;
  mode = CONFIG_MODE;
  log1(F(" - Radiocrafts.enterConfigMode: OK "));
  return true;
}

bool Radiocrafts::exitConfigMode() {
  //  Exit Config Mode and return to Send Mode so we can send data.
  //  We exit to Command Mode first.
  log1(F(" - Exiting config mode to send mode..."));
  //  Confirm we are in CONFIG_MODE
  if (mode != CONFIG_MODE) {
    log1(F(" - Warning: Radiocrafts.exitConfigMode did not detect expected Config Mode, may be in incorrect mode"));
  }
  uint8_t markers = 0;
  if (!sendBuffer(toHex(CMD_EXIT_CONFIG), COMMAND_TIMEOUT, 1, modeData, markers)) return false;
  mode = COMMAND_MODE;
  log1(F(" - Radiocrafts.exitConfigMode: OK "));
  //  Then exit to Send Mode.
  exitCommandMode();
  return true;
}

bool Radiocrafts::getID(String &id, String &pac) {
  //  Get the SIGFOX ID and PAC for the module.
  uint8_t markers = 0;
  if (!sendCommand(toHex('9'), 1, data, markers)) return false;
  //  Returns with 12 bytes: 4 bytes ID (LSB first) and 8 bytes PAC (MSB first).
  if (data.length() != 12 * 2) {
    if (useEmulator) { id = device; return true; }
    log2(F(" - Radiocrafts.getID: Unknown response: "), data);
    return false;
  }
  id = data.substring(6, 8) + data.substring(4, 6) + data.substring(2, 4) + data.substring(0, 2);
  pac = data.substring(8, 8 + 16);
  device = id;
  log2(F(" - Radiocrafts.getID: returned id="), id + ", pac=" + pac);
  return true;
}

bool Radiocrafts::getTemperature(int &temperature) {
  //  Returns the temperature of the SIGFOX module.
  uint8_t markers = 0;
  if (!sendCommand(toHex('U'), 1, data, markers)) return false;
  if (data.length() != 2) {
    if (useEmulator) { temperature = 36; return true; }
    log2(F(" - Radiocrafts.getTemperature: Unknown response: "), data);
    return false;
  }
  temperature = hexDigitToDecimal(data.charAt(0)) * 16 +
                   hexDigitToDecimal(data.charAt(1)) - 128;
  log2(F(" - Radiocrafts.getTemperature: returned "), temperature);
  return true;
}

bool Radiocrafts::getVoltage(float &voltage) {
  //  Returns one byte indicating the power supply voltage.
  uint8_t markers = 0;
  if (!sendCommand(toHex('V'), 1, data, markers)) return false;
  if (data.length() != 2) {
    if (useEmulator) { voltage = 12.3; return true; }
    log2(F(" - Radiocrafts.getVoltage: Unknown response: "), data);
    return false;
  }
  voltage = 0.030 * (hexDigitToDecimal(data.charAt(0)) * 16 +
                hexDigitToDecimal(data.charAt(1)));
  log2(F(" - Radiocrafts.getVoltage: returned "), voltage);
  return true;
}

bool Radiocrafts::getHardware(String &hardware) {
  //  TODO
  log1(F(" - Radiocrafts.getHardware: ERROR - Not implemented"));
  hardware = "TODO";
  return true;
}

bool Radiocrafts::getFirmware(String &firmware) {
  //  TODO
  log1(F(" - Radiocrafts.getFirmware: ERROR - Not implemented"));
  firmware = "TODO";
  return true;
}

bool Radiocrafts::getParameter(uint8_t address, String &value) {
  //  Read the parameter at the address.
  log2(F(" - Radiocrafts.getParameter: address=0x"), toHex((char) address));
  uint8_t markers = 0;
  if (!sendCommand(toHex(CMD_READ_MEMORY) +   //  Read memory ('Y')
                   toHex((char) address),  //  Address of parameter
                   2,  //  Expect 1 marker for command, 1 for response.
                   data, markers)) return false;
  value = data;
  log4(F(" - Radiocrafts.getParameter: address=0x"), toHex((char) address), F(" returned "), value);
  return true;
}

bool Radiocrafts::getPower(int &power) {
  //  Get the power step-down.
  if (!getParameter(0x01, data)) return false;  //  Address of parameter = RF_POWER (0x01)
  power = (int) data.toInt();
  log2(F(" - Radiocrafts.getPower: returned "), power);
  return true;
}

bool Radiocrafts::setPower(int power) {
  //  TODO: Power value: 0...14
  log1(F(" - Radiocrafts.receive: ERROR - Not implemented"));
  return true;
}

bool Radiocrafts::getEmulator(int &result) {
  //  Get the current emulation mode of the module.
  //  0 = Emulator disabled (sending to SIGFOX network with unique ID & key)
  //  1 = Emulator enabled (sending to emulator with public ID & key)
  if (!getParameter(0x28, data)) return false;  //  Address of parameter = PUBLIC_KEY (0x28)
  result = (int) data.toInt();
  return true;
}

bool Radiocrafts::disableEmulator(String &result) {
  //  Set the module key to the unique SIGFOX key.  This is needed for sending
  //  to a real SIGFOX base station.
  if (!sendConfigCommand(String() +
      "28" + //  Address of parameter = PUBLIC_KEY (0x28)
      "00",  //  Value of parameter = Unique ID & key (0x00)
      data)) return false;
  result = data;
  return true;
}

bool Radiocrafts::enableEmulator(String &result) {
  //  Set the module key to the public key.  This is needed for sending
  //  to an emulator.
  if (!sendConfigCommand(String() +
      "28" + //  Address of parameter = PUBLIC_KEY (0x28)
      "01",  //  Value of parameter = Public ID & key (0x00)
      data)) return false;
  result = data;
  return true;
}

bool Radiocrafts::getFrequency(String &result) {
  //  Get the frequency used for the SIGFOX module
  //  0: Europe (RCZ1)
  //  1: US (RCZ2)
  //  3: SG, TW, AU, NZ (RCZ4)
  uint8_t markers = 0;
  if (!sendCommand(toHex(CMD_READ_MEMORY) + "00", 1, data, markers)) return false;
  result = data;
  return true;
}

bool Radiocrafts::setFrequency(int zone, String &result) {
  //  Get the frequency used for the SIGFOX module
  //  0: Europe (RCZ1)
  //  1: US (RCZ2)
  //  3: AU/NZ (RCZ4)
  if (!sendConfigCommand(String() +
    "00" + //  Address of parameter = RF_FREQUENCY_DOMAIN (0x0)
    toHex((char) (zone - 1)),  //  Value of parameter = RCZ - 1
    data)) return false;
  result = data;
  return true;
}

bool Radiocrafts::setFrequencySG(String &result) {
  //  Set the frequency for the SIGFOX module to Singapore frequency (RCZ4).
  log1(F(" - Radiocrafts.setFrequencySG"));
  return setFrequency(4, result); }

bool Radiocrafts::setFrequencyTW(String &result) {
  //  Set the frequency for the SIGFOX module to Taiwan frequency (RCZ4).
  log1(F(" - Radiocrafts.setFrequencyTW"));
  return setFrequency(4, result); }

bool Radiocrafts::setFrequencyETSI(String &result) {
  //  Set the frequency for the SIGFOX module to ETSI frequency for Europe (RCZ1).
  log1(F(" - Radiocrafts.setFrequencyETSI"));
  return setFrequency(1, result); }

bool Radiocrafts::setFrequencyUS(String &result) {
  //  Set the frequency for the SIGFOX module to US frequency (RCZ2).
  log1(F(" - Radiocrafts.setFrequencyUS"));
  return setFrequency(2, result); }

bool Radiocrafts::writeSettings(String &result) {
  //  TODO: Write settings to module's flash memory.
  log1(F(" - Radiocrafts.writeSettings: ERROR - Not implemented"));
  return true;
}

bool Radiocrafts::reboot(String &result) {
  //  TODO: Reboot the module.
  log1(F(" - Radiocrafts.reboot: ERROR - Not implemented"));
  return true;
}

void Radiocrafts::echoOn() {
  //  Echo commands and responses to the echo port.
  echoPort = lastEchoPort;
  log1(F(" - Radiocrafts.echoOn"));
}

void Radiocrafts::echoOff() {
  //  Stop echoing commands and responses to the echo port.
  lastEchoPort = echoPort; echoPort = &nullPort;
}

void Radiocrafts::setEchoPort(Print *port) {
  //  Set the port for sending echo output.
  lastEchoPort = echoPort;
  echoPort = port;
}

void Radiocrafts::echo(const String &msg) {
  //  Echo debug message to the echo port.
  log2(F(" - "), msg);
}

bool Radiocrafts::receive(String &data) {
  //  TODO
  log1(F(" - Radiocrafts.receive: ERROR - Not implemented"));
  return true;
}

String Radiocrafts::toHex(int i) {
  //  Convert the integer to a string of 4 hex digits.
  byte *b = (byte *) &i;
  String bytes;
  for (int j=0; j<2; j++) {
    if (b[j] <= 0xF) bytes.concat('0');
    bytes.concat(String(b[j], 16));
  }
  return bytes;
}

String Radiocrafts::toHex(unsigned int ui) {
  //  Convert the integer to a string of 4 hex digits.
  byte *b = (byte *) &ui;
  String bytes;
  for (int i=0; i<2; i++) {
    if (b[i] <= 0xF) bytes.concat('0');
    bytes.concat(String(b[i], 16));
  }
  return bytes;
}

String Radiocrafts::toHex(long l) {
  //  Convert the long to a string of 8 hex digits.
  byte *b = (byte *) &l;
  String bytes;
  for (int i=0; i<4; i++) {
    if (b[i] <= 0xF) bytes.concat('0');
    bytes.concat(String(b[i], 16));
  }
  return bytes;
}

String Radiocrafts::toHex(unsigned long ul) {
  //  Convert the long to a string of 8 hex digits.
  byte * b = (byte *) &ul;
  String bytes;
  for (int i=0; i<4; i++) {
    if (b[i] <= 0xF) bytes.concat('0');
    bytes.concat(String(b[i], 16));
  }
  return bytes;
}

String Radiocrafts::toHex(float f) {
  //  Convert the float to a string of 8 hex digits.
  byte *b = (byte *) &f;
  String bytes;
  for (int i=0; i<4; i++) {
    if (b[i] <= 0xF) bytes.concat('0');
    bytes.concat(String(b[i], 16));
  }
  return bytes;
}

String Radiocrafts::toHex(double d) {
  //  Convert the double to a string of 8 hex digits.
  byte *b = (byte *) &d;
  String bytes;
  for (int i=0; i<4; i++) {
    if (b[i] <= 0xF) bytes.concat('0');
    bytes.concat(String(b[i], 16));
  }
  return bytes;
}

String Radiocrafts::toHex(char c) {
  //  Convert the char to a string of 2 hex digits.
  byte *b = (byte *) &c;
  String bytes;
  if (b[0] <= 0xF) bytes.concat('0');
  bytes.concat(String(b[0], 16));
  return bytes;
}

String Radiocrafts::toHex(char *c, int length) {
  //  Convert the string to a string of hex digits.
  byte *b = (byte *) c;
  String bytes;
  for (int i=0; i<length; i++) {
    if (b[i] <= 0xF) bytes.concat('0');
    bytes.concat(String(b[i], 16));
  }
  return bytes;
}

uint8_t Radiocrafts::hexDigitToDecimal(char ch) {
  //  Convert 0..9, a..f, A..F to decimal.
  if (ch >= '0' && ch <= '9') return (uint8_t) ch - '0';
  if (ch >= 'a' && ch <= 'z') return (uint8_t) ch - 'a' + 10;
  if (ch >= 'A' && ch <= 'Z') return (uint8_t) ch - 'A' + 10;
  log2(F(" - Radiocrafts.hexDigitToDecimal: Error: Invalid hex digit "), ch);
  return 0;
}

//  Convert nibble to hex digit.
static const char nibbleToHex[] = "0123456789abcdef";

void Radiocrafts::logBuffer(const __FlashStringHelper *prefix, const char *buffer,
                            uint8_t *markerPos, uint8_t markerCount) {
  //  Log the send/receive buffer for debugging.  markerPos is an array of positions in buffer
  //  where the '>' marker was seen and removed.
  echoPort->print(prefix);
  int m = 0, i = 0;
  for (i = 0; i < strlen(buffer); i = i + 2) {
    if (m < markerCount && markerPos[m] == i) {
      echoPort->write((uint8_t) nibbleToHex[END_OF_RESPONSE / 16]);
      echoPort->write((uint8_t) nibbleToHex[END_OF_RESPONSE % 16]);
      echoPort->write(' ');
      m++;
    }
    echoPort->write((uint8_t) buffer[i]);
    echoPort->write((uint8_t) buffer[i + 1]);
    echoPort->write(' ');
  }
  if (m < markerCount && markerPos[m] == i) {
    echoPort->write((uint8_t) nibbleToHex[END_OF_RESPONSE / 16]);
    echoPort->write((uint8_t) nibbleToHex[END_OF_RESPONSE % 16]);
    echoPort->write(' ');
    m++;
  }
  echoPort->write('\n');
}
