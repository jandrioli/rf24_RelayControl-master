/*
 Copyright (C) 2011 James Coliz, Jr. <maniacbug@ymail.com>

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 version 2 as published by the Free Software Foundation.
 */

/**
 * Example using Dynamic Payloads 
 *
 * This is an example of how to use payloads of a varying (dynamic) size. 
 */

#include <SPI.h>
#include "printf.h"
#include "RF24.h"
//
// Physical connections 
//
#define HW_CSN   10    // icsp
#define HW_CE     9    // icsp
// 
// SW Logic and firmware definitions
// 
#define THIS_NODE_ID 2                  // master is 0, unoR3 debugger is 1, promicro_arrosoir is 2, etc
#define DEFAULT_ACTIVATION 600          // 10h from now we activate (in case radio is down and can't program)
#define DEFAULT_DURATION 10             // max 10s of activation time by default

// Set up nRF24L01 radio on SPI bus plus pins HW_CE and HW_CSN
RF24 radio(HW_CE, HW_CSN);

// Radio pipe addresses for the 2 nodes to communicate.
// WARNING!! 3Node and 4Node are used by my testing sketches ping/pong
const uint8_t addresses[][5] = {
  "0Node", // master writes broadcasts here
  "1Node", // unor3 writes here
  "2Node", // unor3 reads here
  "3Node", // arrosoir reads here
  "4Node", // arrosoir writes here
  "5Node"};// not yet used by anybody


/**
 * exchange data via radio more efficiently with data structures.
 * we can exchange max 32 bytes of data per msg. 
 * schedules are reset every 24h (last for a day) so an INTEGER is
 * large enough to store the maximal value of a 24h-schedule.
 * temperature threshold is rarely used
 */
struct relayctl {
  unsigned long uptime = 0;                      // current running time of the machine (millis())  4 bytes  
  unsigned long sched1 = DEFAULT_ACTIVATION;     // schedule in minutes for the relay output nbr1   4 bytes
  unsigned long sched2 = DEFAULT_ACTIVATION;     // schedule in minutes for the relay output nbr2   4 bytes
  unsigned int  maxdur1 = DEFAULT_DURATION;      // max duration nbr1 is ON                         2 bytes
  unsigned int  maxdur2 = DEFAULT_DURATION;      // max duration nbr2 is ON                         2 bytes
  unsigned int  temp_thres = 999;                // temperature at which the syatem is operational  4 bytes
  float         temp_now   = 20;                 // current temperature read on the sensor          4 bytes
  short         battery    =  0;                 // current temperature read on the sensor          2 bytes
  bool          state1 = false;                  // state of relay output 1                         1 byte
  bool          state2 = false;                  // "" 2                                            1 byte
  bool          waterlow = false;                // indicates whether water is low                  1 byte
  byte          nodeid = THIS_NODE_ID;           // nodeid is the identifier of the slave           1 byte
} myData;

void setup() 
{
  /*a little something to tell we're alive*/
  for (int ii = 0; ii<= 5; ii++) 
  {  
    /*blinks the LEDS on the micro*/
#if defined(ARDUINO_AVR_LEONARDO) 
    RXLED1;
    TXLED0; //TX LED is not tied to a normally controlled pin
#else
    digitalWrite(13, HIGH);
#endif
    delay(500);              // wait for a second
#if defined(ARDUINO_AVR_LEONARDO) 
    TXLED1;
    RXLED0;
#else
    digitalWrite(13, LOW);
#endif
    delay(500);              // wait for a second
  }
#if defined(ARDUINO_AVR_LEONARDO) 
  TXLED0; 
  RXLED0;
#else
  digitalWrite(13, LOW);
#endif
  delay(500);
  
  //
  // Print preamble
  //
  Serial.begin(115200);
  delay(500);
  Serial.println(F("RF24 Master - power socket controller"));  
  delay(500);
  Serial.println(F("Warning! Always query the controller before attempting to program it!"));  
  delay(500);
  Serial.println(F("Warning! Always select the target node so that somebody hears you!"));  
  delay(500);
  Serial.println(F("- - - - -"));  
  
  //
  // Setup and configure rf radio
  //
  radio.begin();
  radio.setCRCLength( RF24_CRC_16 ) ;
  radio.setRetries( 15, 5 ) ;
  radio.setAutoAck( true ) ;
  radio.setPALevel( RF24_PA_MAX ) ;
  radio.setChannel( 108 ) ;
  radio.setDataRate( RF24_250KBPS ) ;
  radio.enableDynamicPayloads(); //dont work with my modules :-/

  //
  // Open pipes to other nodes for communication
  //
  // This simple sketch opens two pipes for these two nodes to communicate
  // back and forth.
  // Open 'our' pipe for writing
  // Open the 'other' pipe for reading, in position #1 (we can have up to 5 pipes open for reading)
  radio.openWritingPipe(addresses[0]);
  radio.openReadingPipe(1,addresses[1]);
  radio.openReadingPipe(4,addresses[4]);

  //
  // Dump the configuration of the rf unit for debugging
  //
  printf_begin();
  Serial.println(F("Radio setup:"));  
  radio.printDetails();
  Serial.println(F("- - - - -"));  
  
  //
  // Start listening
  //
  radio.powerUp() ;
  radio.startListening();
}

void loop(void)
{
  if (Serial.available())
  {
    String s1 = Serial.readString();
    Serial.print("You typed:");
    Serial.println(s1);
    if (s1.indexOf("sched1 ")>=0)
    {
      String sched = s1.substring(s1.indexOf(" ")+1);
      myData.sched1 = sched.substring(0, sched.length()).toInt() ;
      s1 = "";
    }
    else if (s1.indexOf("select ")>=0)
    {
      String sched = s1.substring(s1.indexOf(" ")+1);
      myData.nodeid = sched.substring(0, sched.length()).toInt() ;
      Serial.print("WARNING!!! Do you know what pipe address to target?!?\n\rSelected pipe address for node ");
      Serial.println(myData.nodeid);
      for(int i = 0; i < sizeof(addresses[myData.nodeid]); i++)
      {
        Serial.print(" ");
        Serial.print(addresses[myData.nodeid][i]);
        Serial.print(" ");
      }
      radio.stopListening();
      radio.openWritingPipe(addresses[myData.nodeid]);
      radio.startListening();
      s1 = "";
    }
    else if (s1.indexOf("maxdur1 ")>=0)
    {
      String sched = s1.substring(s1.indexOf(" ")+1);
      myData.maxdur1 = sched.substring(0, sched.length()).toInt() ;
      s1 = "";
    }
    else if (s1.indexOf("sched2 ")>=0)
    {
      String sched = s1.substring(s1.indexOf(" ")+1);
      myData.sched2 = sched.substring(0, sched.length()).toInt() ;
      s1 = "";
    }
    else if (s1.indexOf("maxdur2 ")>=0)
    {
      String sched = s1.substring(s1.indexOf(" ")+1);
      myData.maxdur2 = sched.substring(0, sched.length()).toInt() ;
      s1 = "";
    }
    else if (s1.indexOf("temp ")>=0)
    {
      String sched = s1.substring(s1.indexOf(" ")+1);
      myData.temp_thres = sched.substring(0, sched.length()).toInt() ;
      s1 = "";
    }
    else if (s1.indexOf("debug")>=0)
    {
      Serial.println(F("Radio setup:"));  
      radio.printDetails();
      Serial.println(F("- - - - -"));  
      Serial.println(F("Signal quality:"));  
      bool goodSignal = radio.testRPD();
      Serial.println(goodSignal ? "Strong signal > 64dBm" : "Weak signal < 64dBm" );
      Serial.println(F("- - - - -"));  
      s1 = "";
    }
    else if ((s1.indexOf("upload")>=0))
    {
      Serial.println("Sending out new program...");
      s1 = "";
      printState(myData, 0);
      radio.stopListening();
      radio.write(&myData, sizeof(myData), false);
      radio.startListening();
    }
    else if ((s1.indexOf("stop")!=0) && (s1.indexOf("status")!=0))
    {
      Serial.println("** Serial interface expects:\n\r"\
        "** 0 - sched1 5:       activate relay1 after 5 minutes of uptime\n\r"\
        "** 0 - sched2 60000:   activate relay2 after 10h of uptime\n\r"\
        "** 0 - maxdur1 120: maximum duration for relay 1 to be active in seconds \n\r"\
        "** 0 - maxdur2 120: maximum duration for relay 1 to be active in seconds \n\r"\
        "** 0 - temp -8: activate relays if temperature is under -8C\n\r"\
        "** 0 - upload: send over a new program to relay ctler\n\r"\        
        "** 1 - select: choose a node to work with\n\r"\
        "** 2 - stop: deactivate everything and reset all counters\n\r"\
        "** 3 - status: returns the current status of relays and counters, temp, uptime");
      s1 = "";
    }
    
    if (s1 != "")
    {      
      Serial.println("Sending command: " + s1);
      char qS[s1.length()+1] ;
      s1.toCharArray(qS, s1.length()+1);
      qS[s1.length()+1] = '\0';

      for (int retrysending=0; retrysending<10; retrysending++)
      {
      // First, stop listening so we can talk.
      radio.stopListening();
      // send data OTA
      radio.write(qS, s1.length()+1, false);
      // restart listening
      radio.startListening();

      // Wait here until we get a response, or timeout (arrosoir sleeps for 8s)
      unsigned long started_waiting_at = millis();
      bool timeout = false;
      while ( ! radio.available() && ! timeout )
        if (millis() - started_waiting_at > 1500 )
          timeout = true;

      // Describe the results
      if ( timeout )
      {
        Serial.println(F("Failed, response timed out."));
      }
      else
      {
        break; // get out of looping for
      }
      }
    }
  }
  if (radio.available())
  {
    uint8_t pipeNumber = 0; // ZERO is my own pipe, validate it
    {
      while (radio.available(&pipeNumber))
      {
        Serial.print("From Node(#ID) ");
        Serial.print(pipeNumber);
        Serial.print(", bytes received:");
        
        // Fetch the payload, and see if this was the last one.
        uint8_t len = radio.getDynamicPayloadSize();
        Serial.println(len);
      
        if ( len == sizeof(relayctl) )
        {
          relayctl oTemp;
          radio.read( &oTemp, len);
          printState(oTemp, pipeNumber);
        }
        else
        {
          char* rx_data = NULL;
          rx_data = (char*)calloc(len+1, sizeof(char));
          radio.read( rx_data, len );
      
          // Put a zero at the end for easy printing
          rx_data[len+1] = 0;
          // Spew it
          Serial.print(F(" Got size:"));
          Serial.print(len);
          Serial.print(F(" which doesnt match my datastructure, is this text msg:"));
          Serial.println(rx_data);
          free(rx_data);
          rx_data = NULL;
        }
      }
    }
  }
}


void printState(relayctl& myData, uint8_t node)
{
  Serial.print("Plug 1: ");
  Serial.print(myData.sched1);
  Serial.print("min, during ");
  Serial.print(myData.maxdur1);
  Serial.print("s(currently ");
  Serial.print(myData.state1);
  Serial.print(")\nPlug 2: ");
  Serial.print(myData.sched2);
  Serial.print("min, during ");
  Serial.print(myData.maxdur2);
  Serial.print("s(currently ");
  Serial.print(myData.state2);
  Serial.print(")\nTemperature: ");
  Serial.print(myData.temp_now);
  Serial.print("/");
  Serial.print(myData.temp_thres);
  Serial.print("\nUptime: ");
  Serial.print(myData.uptime);
  Serial.print("min\nBattery:");
  Serial.print(myData.battery);
  Serial.print("V\nWaterLow:");
  Serial.print(myData.waterlow);
  Serial.print("\nNodeID#:");
  Serial.print(myData.nodeid);
  Serial.println();
  
  Serial.println("RF24-BLOB-BEGIN");
  Serial.write((uint8_t *)&myData, sizeof(myData));
  Serial.println();
}

