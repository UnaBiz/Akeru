/* Akeru.h - v4 [2016.07.29]
 * 
 * Copyleft Snootlab 2016 - inspired by TD1208 lib by IoTHerd (C) 2016
 *
 * Akeru is a library for Sigfox TD1208 use with the Arduino platform. 
 * The library is designed to use SoftwareSerial library for serial communication between TD1208 module and Arduino.
 * Current library coverage is:
 *	 - AT command 
 *	 - Sigfox payload transfer
 *	 - TD1208 temperature read
 *	 - TD1208 ID read
 *	 - TD1208 supply voltage read
 *	 - TD1208 hardware version read
 *	 - TD1208 firmware version read
 *	 - TD1208 power read
 *	 - TD1208 power set
 *   - TD1208 downlink request
 *   - Data conversion in hexadecimal
 */

#ifndef BEAN_BEAN_BEAN_H  //  Not supported on Bean+
#ifndef AKERU_H
#define AKERU_H

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

#define ATOK "OK"
#define ATCOMMAND "AT"
#define ATID "ATI7"
#define ATHARDWARE "ATI11"
#define ATFIRMWARE "ATI13"
#define ATTEMPERATURE "ATI26"
#define ATVOLTAGE "ATI27"
#define ATPOWER "ATS302"
#define ATDOWNLINK "AT$SB=1,2,1"
#define ATSIGFOXTX "AT$SS="
#define ATTDLANTX "AT$SL="
#define DOWNLINKEND "+RX END"

const unsigned int AKERU_RX = 4;  //  Receive port for UnaBiz / Akene Dev Kit
const unsigned int AKERU_TX = 5;  //  Transmit port for UnaBiz / Akene Dev Kit

//  Set frequency of the SIGFOX module to Singapore and Taiwan (920.8 MHz):
#define ATSET_FREQUENCY_SG "AT$IF=920800000"

//  Set frequency of the SIGFOX module to ETSI (Europe, 868.2 MHz):
#define ATSET_FREQUENCY_ETSI "AT$IF=868200000"

//  Get frequency used by the SIGFOX module.
#define ATGET_FREQUENCY "AT$IF?"

//  Write settings to Flash memory of the SIGFOX module.
#define ATWRITE_SETTINGS "AT&W"

//  Reboot the SIGFOX module.
#define ATREBOOT "ATZ"

#define ATMODEL "ATI0"  //  Get manufacturer and model.
#define ATRELEASE "ATI5"  //  Get firmware release date.
#define ATBASEBAND "ATI10"  //  Get baseband unique ID.
#define ATRF_PART "ATI21"  //  Get RF chip part number.
#define ATRF_REVISION "ATI25"  //  Get RF chip revision number.
#define ATPOWER_ACTIVE "ATI28"  //  Get module RF active power supply voltage
#define ATLIBRARY "ATI30"  //  Get RF library version.

#define ATCOMMAND_TIMEOUT (3000)
#define ATSIGFOXTX_TIMEOUT (30000)
#define ATDOWNLINK_TIMEOUT (45000)

// Set to 1 if you want to print the AT commands and answers
// on the serial monitor, set to 0 otherwise.
//#define _cmdEcho 1

class Akeru
{
	public:
    Akeru();
		Akeru(unsigned int rx, unsigned int tx);
    bool begin();
    void echoOn();  //  Turn on send/receive echo.
    void echoOff();  //  Turn off send/receive echo.
    void setEchoPort(Print *port);  //  Set the port for sending echo output.
		void echo(String msg);  //  Echo the debug message.
    bool isReady();
    bool sendMessage(const String payload);  //  Send the payload of hex digits to the network, max 12 bytes.
		bool sendString(const String str);  //  Sending a text string, max 12 characters allowed.
    bool receive(String &data);  //  Receive a message.
    bool enterCommandMode() {}  //  Enter Command Mode for sending module commands, not data.
    bool exitCommandMode() {}  //  Exit Command Mode so we can send data.

    //  Commands for the module, must be run in Command Mode.
    bool getEmulator(int &result)  //  Return 0 if emulator mode disabled, else return 1.
      { result = _emulationMode ? 1 : 0; return true; }
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
    bool writeSettings(String &result); //  Write frequency and other settings to flash memory of the module.
    bool reboot(String &result);  //  Reboot the SIGFOX module.
    bool getTemperature(int &temperature);
    bool getID(String &id, String &pac);  //  Get the SIGFOX ID and PAC for the module.
    bool getVoltage(float &voltage);
    bool getHardware(String &hardware);
    bool getFirmware(String &firmware);
    bool getPower(int &power);
    bool setPower(int power);
    bool getParameter(uint8_t address, String &value) {
      //  Return the parameter at that address.  Not used for Akene.
      value = "";
      return false;
    }

    //  Message conversion functions.
    String toHex(int i);
    String toHex(unsigned int i);
    String toHex(long l);
    String toHex(unsigned long ul);
    String toHex(float f);
    String toHex(double d);
    String toHex(char c);
    String toHex(char *c, int length);

		bool getModel(String &model);  //  Get manufacturer and model.
		bool getRelease(String &release);  //  Get firmware release date.
		bool getBaseband(String &baseband);  //  Get baseband unique ID.
		bool getRFPart(String &part);  //  Get RF chip part number.
		bool getRFRevision(String &revision);  //  Get RF chip revision number.
		bool getPowerActive(String &power);  //  Get module RF active power supply voltage
		bool getLibraryVersion(String &version); //  Get RF library version.

private:
    bool sendAT();
		bool sendATCommand(const String command, const int timeout, String &dataOut);
		SoftwareSerial* serialPort;
    Print *echoPort;  //  Port for sending echo output.  Defaults to Serial.
    Print *lastEchoPort;  //  Last port used for sending echo output.
    bool _emulationMode = false;  //  True if using emulation (TD LAN) mode.
		unsigned long _lastSend;  //  Timestamp of last send.
    unsigned int _sequenceNumber;  //  Sequence number for the message.
    String _id = "";  //  SIGFOX device ID.
    String _pac = "";  //  SIGFOX PAC.
};

#endif // AKERU_H
#endif // BEAN_BEAN_BEAN_H  //  Not supported on Bean+
