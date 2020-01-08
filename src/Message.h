//  Library for sending and receiving structured SIGFOX messages
#ifndef UNABIZ_ARDUINO_MESSAGE_H
#define UNABIZ_ARDUINO_MESSAGE_H

#ifdef ARDUINO
  #if (ARDUINO >= 100)
    #include <Arduino.h>
  #else  //  ARDUINO >= 100
    #include <WProgram.h>
  #endif  //  ARDUINO  >= 100
#endif  //  ARDUINO

class Message
{
public:
  Message(Radiocrafts &transceiver);  //  Construct a message for Radiocrafts.
  Message(Wisol &transceiver);  //  Construct a message for Wisol.
  bool addField(const String name, int value);  //  Add an integer field scaled by 10.
  bool addField(const String name, float value);  //  Add a float field with 1 decimal place.
  bool addField(const String name, double value);  //  Add a double field with 1 decimal place.
  bool addField(const String name, const String value);  //  Add a string field with max 3 chars.
  bool send();  //  Send the structured message.
  bool sendAndGetResponse(String &response);  //  Send the structured message and get the downlink response.
  String getEncodedMessage();  //  Return the encoded message to be transmitted.
  static String decodeMessage(String msg);  //  Decode the encoded message.

private:
  bool addIntField(const String name, int value);  //  Add an integer field already scaled.
  bool addName(const String name);  //  Encode and add the 3-letter name.
  void echo(String msg);
  String encodedMessage;  //  Encoded message.
  Radiocrafts *radiocrafts = 0;  //  Reference to Radiocrafts transceiver for sending the message.
  Wisol *wisol = 0;  //  Reference to Wisol transceiver for sending the message.
};

#endif // UNABIZ_ARDUINO_MESSAGE_H
