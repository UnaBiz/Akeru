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

#endif  //  ARDUINO

#ifndef BEAN_BEAN_BEAN_H  //  Not supported on Bean+
#include "SIGFOX.h"
#include "Akeru.h"

static NullPort nullPort2;

Akeru::Akeru(): Akeru(AKERU_RX, AKERU_TX) {}  //  Forward to constructor below.

Akeru::Akeru(unsigned int rx, unsigned int tx)
{
  //  Init the module with the specified transmit and receive pins.
  //  Default to no echo.
  //Serial.begin(9600); Serial.print(String(F("Akeru.Akeru: (rx,tx)=")) + rx + ',' + tx + '\n');
  serialPort = new SoftwareSerial(rx, tx);
  echoPort = &nullPort2;
  lastEchoPort = &Serial;
  _lastSend = 0;
}

void Akeru::echoOn()
{
  //  Echo commands and responses to the echo port.
  echoPort = lastEchoPort;
  //  echoPort->println(F("Akeru.echoOn"));
}

void Akeru::echoOff()
{
  //  Stop echoing commands and responses to the echo port.
  lastEchoPort = echoPort;
  echoPort = &nullPort2;
}

void Akeru::setEchoPort(Print *port) {
  //  Set the port for sending echo output.
  lastEchoPort = echoPort;
  echoPort = port;
}

void Akeru::echo(String msg) {
  //  Echo debug message to the echo port.
  echoPort->println(msg);
}

bool Akeru::begin()
{
  //  Wait for the module to power up. Return true if module is ready to send.
	// Let the modem warm up a bit
	delay(2000);
	_lastSend = -1;
	
	// Check TD1208 communication
	if (sendAT()) return true;
	else return false;
}

bool Akeru::isReady()
{
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
  if (_lastSend == 0) return true;  //  First time sending.
  const unsigned long elapsedTime = currentTime - _lastSend;
  //  For development, allow sending every 5 seconds.
  if (elapsedTime <= 5 * 1000) {
    echoPort->println(F("Must wait 5 seconds before sending the next message"));
    return false;
  }  //  Wait before sending.
  if (elapsedTime <= SEND_DELAY && _id != "1AE8E2")
    echoPort->println(F("Warning: Should wait 10 mins before sending the next message"));
  return true;
}

bool Akeru::sendAT()
{
  String data = "";
	return sendATCommand(ATCOMMAND, ATCOMMAND_TIMEOUT, data);
}

bool Akeru::sendMessage(const String payload)
{
  // Payload must be a String formatted in hexadecimal, 12 bytes max, use toHex()
	if (!isReady()) return false; // prevent user from sending to many messages

  //  Construct the message.
  //  For emulation mode, send message locally to another TD module using TD LAN mode.
	String message = _emulationMode ? ATTDLANTX : ATSIGFOXTX;
	if (_emulationMode) {
    //  Emulation message format: device ID (4 bytes) + sequence number (1 byte) + payload (max 12 bytes)
    String id, pac;
    getID(id, pac);
    id = String("00000000").substring(id.length()) + id; // Prepad to 4 bytes.
    message.concat(id);  //  4 bytes
    message.concat(toHex((char) _sequenceNumber));  //  1 byte
	}
	message.concat(payload);  //  Max 12 bytes
  _sequenceNumber++;
  if (_sequenceNumber > 255) _sequenceNumber = 0;

  //  Send the message.
	String data = "";
	if (sendATCommand(message, ATSIGFOXTX_TIMEOUT, data))
	{
    echoPort->println(data);
		_lastSend = millis();
		return true;
	}
	else
	{
		return false;
	}
}

bool Akeru::getTemperature(int &temperature)
{
	String data = "";
	if (sendATCommand(ATTEMPERATURE, ATCOMMAND_TIMEOUT, data))
	{
		temperature = (int) data.toInt();
		return true;
	}
	else
	{
		return false;
	}
}

bool Akeru::getID(String &id, String &pac)
{
  //  Get the SIGFOX ID and PAC for the module.  PAC is not available for Akene.
  //  Reuse the previous data if available.
  if (_id.length() > 0) {
    id = _id; pac = _pac; return true;
  }
	String data = "";
	if (sendATCommand(ATID, ATCOMMAND_TIMEOUT, data))
	{
		id = data; pac = "";  //  PAC is not available for Akene.
    _id = id; _pac = pac;  //  Cache for later use.
		return true;
	}
	else
	{
		return false;
	}
}

bool Akeru::getVoltage(float &voltage)
{
	String data = "";
	if (sendATCommand(ATVOLTAGE, ATCOMMAND_TIMEOUT, data))
	{
    //  Returns the response "3.28".
		//  voltage = data.toFloat();
    //  Since Bean+ doesn't support toFloat(), we convert to int and divide by the decimal place.
    int dotPos = data.indexOf('.');
    int divisor = 0;
    if (dotPos >= 0) {  //  Expect 1.
      divisor = data.length() - dotPos - 1;
      data = data.substring(0, (unsigned int) dotPos) + data.substring((unsigned int) dotPos + 1);
    }
    voltage = data.toInt();
    for (int i = 0; i < divisor; i++) {
      voltage = voltage / 10.0;
    }
		return true;
	}
	else
	{
		return false;
	}
}

bool Akeru::getHardware(String &hardware)
{
	String data = "";
	if (sendATCommand(ATHARDWARE, ATCOMMAND_TIMEOUT, data))
	{
		hardware = data;
		return true;
	}
	else
	{
		return false;
	}
}

bool Akeru::getFirmware(String &firmware)
{
	String data = "";
	if (sendATCommand(ATFIRMWARE, ATCOMMAND_TIMEOUT, data))
	{
		firmware = data;
		return true;
	}
	else
	{
		return false;
	}
}

bool Akeru::getPower(int &power)
{	
	String message = (String) ATPOWER + '?';
	String data = "";
	if (sendATCommand(message, ATCOMMAND_TIMEOUT, data))
	{
		power = data.toInt();
		return true;
	}
	else
	{
		return false;
	}
}

// Power value: 0...14
bool Akeru::setPower(int power)
{
	String message = (String) ATPOWER + "=" + power;
	String data = "";
	if (sendATCommand(message, ATCOMMAND_TIMEOUT, data))
	{
		echoPort->println(data);
		return true;
	}
	else
	{
		return false;
	}
}

bool Akeru::receive(String &data)
{
	if (!isReady()) return false;
	
	if (sendATCommand(ATDOWNLINK, ATSIGFOXTX_TIMEOUT, data))
	{
		// Restart serial interface
		serialPort->begin(9600);
		delay(200);	
		serialPort->flush();
		serialPort->listen();
		
		// Read response 
		String response = "";
		
		unsigned int startTime = millis();
		volatile unsigned int currentTime = millis();
		volatile char rxChar = '\0';

		// RX management : two ways to break the loop
		// - Timeout
		// - Receive 
		do
		{
			if (serialPort->available() > 0)
			{
				rxChar = (char)serialPort->read();
				response.concat(rxChar);
			}
			currentTime = millis();
		}while(((currentTime - startTime) < ATDOWNLINK_TIMEOUT) && response.endsWith(DOWNLINKEND) == false);

		serialPort->end();
    echoPort->println(response);

		// Now that we have the full answer we can look for the received bytes
		if (response.length() != 0)
		{
			// Search for transmission start
			int index = response.indexOf("=");
			// Get the chunk, it's 8 bytes long + separating spaces = 23 chars
			String chunk = response.substring(index, index+24);
			if (chunk.length() > 0)
			{
				// Remove the spaces
				chunk.replace(" ", "");
				data = chunk;
				return true;
			}
		}
	}
	else 
	{
		return false;
	}
}

String Akeru::toHex(int i)
{
	byte *b = (byte*) & i;
	
	String bytes = "";
	for (int j=0; j<2; j++)
	{
		if (b[j] <= 0xF) // single char
		{
			bytes.concat("0"); // add a "0" to make sure every byte is read correctly
		}
		bytes.concat(String(b[j], 16));
	}
	return bytes;
}

String Akeru::toHex(unsigned int ui)
{
	byte * b = (byte*) & ui;
	
	String bytes = "";
	for (int i=0; i<2; i++)
	{
		if (b[i] <= 0xF) // single char
		{
			bytes.concat("0"); // add a "0" to make sure every byte is read correctly
		}
		bytes.concat(String(b[i], 16));
	}
	return bytes;
}

String Akeru::toHex(long l)
{
	byte * b = (byte*) & l;
	
	String bytes = "";
	for (int i=0; i<4; i++)
	{
		if (b[i] <= 0xF) // single char
		{
			bytes.concat("0"); // add a "0" to make sure every byte is read correctly
		}
		bytes.concat(String(b[i], 16));
	}
	return bytes;
}

String Akeru::toHex(unsigned long ul)
{
	byte * b = (byte*) & ul;
	
	String bytes = "";
	for (int i=0; i<4; i++)
	{
		if (b[i] <= 0xF) // single char
		{
			bytes.concat("0"); // add a "0" to make sure every byte is read correctly
		}
		bytes.concat(String(b[i], 16));
	}
	return bytes;
}

String Akeru::toHex(float f)
{
	byte * b = (byte*) & f;

	String bytes = "";
	for (int i=0; i<4; i++)
	{
		if (b[i] <= 0xF) // single char
		{
			bytes.concat("0"); // add a "0" to make sure every byte is read correctly
		}
		bytes.concat(String(b[i], 16));
	}
	return bytes;
}

String Akeru::toHex(double d)
{
	byte * b = (byte*) & d;

	String bytes = "";
	for (int i=0; i<4; i++)
	{
		if (b[i] <= 0xF) // single char
		{
			bytes.concat("0"); // add a "0" to make sure every byte is read correctly
		}
		bytes.concat(String(b[i], 16));
	}
	return bytes;
}

String Akeru::toHex(char c)
{
	byte *b = (byte*) & c;
	
	String bytes = "";
	if (b[0] <= 0xF) // single char
	{
		bytes.concat("0"); // add a "0" to make sure every byte is read correctly
	}
	bytes.concat(String(b[0], 16));
	return bytes;
}

String Akeru::toHex(char *c, int length)
{
	byte * b = (byte*) c;
	
	String bytes = "";
	for (int i=0; i<length; i++)
	{
		if (b[i] <= 0xF) // single char
		{
			bytes.concat("0"); // add a "0" to make sure every byte is read correctly
		}
		bytes.concat(String(b[i], 16));
	}
	return bytes;
}

bool Akeru::sendATCommand(const String command, const int timeout, String &dataOut)
{
	// Start serial interface
	serialPort->begin(9600);
	delay(200);	
	serialPort->flush();
	serialPort->listen();

	// Add CRLF to the command
	String ATCommand = "";
	ATCommand.concat(command);
	ATCommand.concat("\r\n");
  echoPort->print((String)"\n>> " + ATCommand);

	// Send the command : need to write/read char by char because of echo
	for (int i = 0; i < ATCommand.length(); ++i)
	{
		serialPort->print(ATCommand.c_str()[i]);
		serialPort->read();
	}
  echoPort->print("<< ");

	// Read response 
	String response = "";
	unsigned int startTime = millis();
	volatile unsigned int currentTime = millis();
	volatile char rxChar = '\0';

	// RX management : two ways to break the loop
	// - Timeout
	// - Receive 
	do
	{
		if (serialPort->available() > 0)
		{
			rxChar = (char)serialPort->read();
			response.concat(rxChar);
		}

		currentTime = millis();
	}while(((currentTime - startTime) < timeout) && response.endsWith(ATOK) == false);

	serialPort->end();
  String res = response;
  while (res.length() > 0 && (res.charAt(0) == '\r' || res.charAt(0) == '\n'))
    res = res.substring(1);  //  Strip off leading newline.
  echoPort->print(res);

	// Split the response
	int index = 0;
	String firstData = "";
	String secondData = "";

	if (response.length() != 0)
	{
		// Split CRLF
		do
		{
			// Save previous index
			int previous = index;

			// Get next index
			index = response.indexOf("\r\n", index);

			// Check that index change
			if (previous != index)
			{
				// Get the chunk
				String chunk = response.substring(previous+1, index);
				if (chunk.length() > 0)
				{
					// Remove \r\n 
					chunk.replace("\r\n", "");
					
					if (firstData != "")
					{
						secondData = chunk;
					}
					else if (firstData == "" && secondData == "")
					{
						firstData = chunk;
					}
					else
					{
						echoPort->println("ERROR on rx frame");
						return false;
					}
				}
			}

			// Increment index
			if (index >= 0) index++;
		
		} while (index < response.length() && index >= 0);
	}
	else
	{
		return false;
	}

	// Check if we have data on the first string and OK on second = data + OK
	if (firstData != "" && secondData == ATOK)
	{
		dataOut = firstData;
		return true;
	}
	// Check if we have only an OK
	else if (firstData == ATOK && secondData == "")
	{
		return true;
	}
	else
	{
		echoPort->println("Wrong AT response");
		return false;
	}
}

//  Singapore and Taiwan: 920.8 MHz Uplink, 922.3 MHz Downlink
//  ETSI (Europe): 868.2 MHz

bool Akeru::getFrequency(String &result)
{
	//  Get the frequency used for the SIGFOX module, e.g.
	//  868130000
	String data = "";
	if (sendATCommand(ATGET_FREQUENCY, ATCOMMAND_TIMEOUT, data))
	{
		result = data;
		return true;
	}
	else
	{
		return false;
	}
}

bool Akeru::setFrequencySG(String &result)
{
	//  Set the frequency for the SIGFOX module to Singapore frequency.
	//  Must be followed by writeSettings and reboot commands.
	String data = "";
	if (sendATCommand(ATSET_FREQUENCY_SG, ATCOMMAND_TIMEOUT, data))
	{
		result = data;
		return true;
	}
	else
	{
		return false;
	}
}

bool Akeru::setFrequencyTW(String &result)
{
	//  Set the frequency for the SIGFOX module to Taiwan frequency, which is same as Singapore frequency.
	//  Must be followed by writeSettings and reboot commands.
	return setFrequencySG(result);
}

bool Akeru::setFrequencyETSI(String &result)
{
	//  Set the frequency for the SIGFOX module to ETSI frequency for Europe or demo for 868 MHz base station.
	//  Must be followed by writeSettings and reboot commands.
	String data = "";
	if (sendATCommand(ATSET_FREQUENCY_ETSI, ATCOMMAND_TIMEOUT, data))
	{
		result = data;
		return true;
	}
	else
	{
		return false;
	}
}

bool Akeru::writeSettings(String &result)
{
	//  Write frequency and other settings to flash memory of the SIGFOX module.  Must be followed by reboot command.
	String data = "";
	if (sendATCommand(ATWRITE_SETTINGS, ATCOMMAND_TIMEOUT, data))
	{
		result = data;
		return true;
	}
	else
	{
		return false;
	}
}

bool Akeru::reboot(String &result)
{
	//  Reboot the SIGFOX module.
	String data = "";
	if (sendATCommand(ATREBOOT, ATCOMMAND_TIMEOUT, data))
	{
		result = data;
		return true;
	}
	else
	{
		return false;
	}
}

bool Akeru::enableEmulator(String &result)
{
	//  Enable emulator mode.
  result = "";
  _emulationMode = true;
  return true;
}

bool Akeru::disableEmulator(String &result)
{
	//  Disable emulator mode.
  result = "";
  _emulationMode = false;
  return true;
}

bool Akeru::getModel(String &result)
{
	//  Get manufacturer and model.
	String data = "";
	if (sendATCommand(ATMODEL, ATCOMMAND_TIMEOUT, data))
	{
		result = data;
		return true;
	}
	else
	{
		return false;
	}
}

bool Akeru::getRelease(String &result)
{
	//  Get firmware release date.
	String data = "";
	if (sendATCommand(ATRELEASE, ATCOMMAND_TIMEOUT, data))
	{
		result = data;
		return true;
	}
	else
	{
		return false;
	}
}

bool Akeru::getBaseband(String &result)
{
	//  Get baseband unique ID.
	String data = "";
	if (sendATCommand(ATBASEBAND, ATCOMMAND_TIMEOUT, data))
	{
		result = data;
		return true;
	}
	else
	{
		return false;
	}
}

bool Akeru::getRFPart(String &result)
{
	///  Get RF chip part number.
	String data = "";
	if (sendATCommand(ATRF_PART, ATCOMMAND_TIMEOUT, data))
	{
		result = data;
		return true;
	}
	else
	{
		return false;
	}
}

bool Akeru::getRFRevision(String &result)
{
	//  Get RF chip revision number.
	String data = "";
	if (sendATCommand(ATRF_REVISION, ATCOMMAND_TIMEOUT, data))
	{
		result = data;
		return true;
	}
	else
	{
		return false;
	}
}

bool Akeru::getPowerActive(String &result)
{
	//  Get module RF active power supply voltage
	String data = "";
	if (sendATCommand(ATPOWER_ACTIVE, ATCOMMAND_TIMEOUT, data))
	{
		result = data;
		return true;
	}
	else
	{
		return false;
	}
}

bool Akeru::getLibraryVersion(String &result)
{
	//  Get RF library version.
	String data = "";
	if (sendATCommand(ATLIBRARY, ATCOMMAND_TIMEOUT, data))
	{
		result = data;
		return true;
	}
	else
	{
		return false;
	}
}

// For convenience, allow sending of a text string with automatic encoding into bytes.  Max 12 characters allowed.
bool Akeru::sendString(const String str)
{
	//  TODO: If string is over 12 characters, split into multiple messages.
	//  Convert each character into 2 bytes.
	String payload = "";
	for (int i = 0; i < str.length(); i++) {
		char ch = str.charAt(i);
		payload += toHex(ch);
	}
	//  Send the encoded payload.
	return sendMessage(payload);
}

#endif  //  BEAN_BEAN_BEAN_H  //  Not supported on Bean+

/*
Demo sketch for Akeru library :)

>> AT$IF?
<< 
920800000

OK
Before setting frequency to Singapore: Frequency = 920800000

>> AT$IF=920800000
<< 
OK
Set frequency to Singapore: Result = 

>> AT$IF?
<< 
920800000

OK
After setting frequency to Singapore: Frequency = 920800000

>> AT&W
<< 
OK
Write frequency to flash memory: Result = 

>> ATZ
<< 
OK
Reboot: Result = 

>> AT$IF?
<< 
After reboot: Frequency = 

>> ATI26
<< 
24

OK
Temperature = 24 C

>> ATI27
<< 
3.29

OK
Supply voltage = 3.29 V

>> ATI7
<< 
1AE65E
TDID: 130257003339

OK
ID = 1AE65E

>> ATI11
<< 
0F

OK
Hardware version = 0F

>> ATI13
<< 
SOFT2069

OK
Firmware version = SOFT2069

>> ATS302?
<< 
14

OK
Power level = 14 dB

>> AT$SS=18005c8f5240
<< 
OK

Message sent !
*/

/*
Demo sketch for Akeru library :)

>> AT$IF?
<< 
868130000

OK
Frequency = 868130000

>> ATI26
<< 
24

OK
Temperature = 24 C

>> ATI27
<< 
3.29

OK
Supply voltage = 3.29 V

>> ATI7
<< 
1AE65E
TDID: 130257003339

OK
ID = 1AE65E

>> ATI11
<< 
0F

OK
Hardware version = 0F

>> ATI13
<< 
SOFT2069

OK
Firmware version = SOFT2069

>> ATS302?
<< 
14

OK
Power level = 14 dB

>> AT$SS=18005c8f5240
<< 
OK

Message sent !
*/


/*
Demo sketch for Akeru library :)

>> AT$IF?
<< 
868130000

OK
Before setting frequency to Singapore: Frequency = 868130000

>> AT$IF=920800000
<< 
OK
Set frequency to Singapore: Result = 

>> AT$IF?
<< 
920800000

OK
After setting frequency to Singapore: Frequency = 920800000

>> ATI26
<< 
24

OK
Temperature = 24 C

>> ATI27
<< 
3.29

OK
Supply voltage = 3.29 V

>> ATI7
<< 
1AE65E
TDID: 130257003339

OK
ID = 1AE65E

>> ATI11
<< 
0F

OK
Hardware version = 0F

>> ATI13
<< 
SOFT2069

OK
Firmware version = SOFT2069

>> ATS302?
<< 
14

OK
Power level = 14 dB

>> AT$SS=18005c8f5240
<< 
OK

Message sent !
*/

//  End UnaBiz
