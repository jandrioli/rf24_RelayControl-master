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

#define DEFAULT_ACTIVATION 600    // 10h from now we activate (in case radio is down and can't program)
#define DEFAULT_DURATION   7200   // max 2h of activation time by default


// Set up nRF24L01 radio on SPI bus plus pins 8 & 9
RF24 radio(9,10);

// Radio pipe addresses for the 2 nodes to communicate.
//const uint64_t pipes[2] = { 0xEEFAFDFDEELL, 0xEEFDFAF50DFLL };
const uint8_t pipes[][6] = {"1Node","2Node"};

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
  float         temp_thres = 999;                // temperature at which the syatem is operational  4 bytes
  float         temp_now   = 20;                 // current temperature read on the sensor          4 bytes
  short         battery    =  0;                 // current temperature read on the sensor          2 bytes
  bool          state1 = false;                  // state of relay output 1                         1 byte
  bool          state2 = false;                  // "" 2                                            1 byte
  bool          waterlow = false;                // indicates whether water is low                  1 byte
} myData;

void setup() 
{
  //
  // Print preamble
  //
  Serial.begin(115200);
  Serial.println(F("RF24 Master - power socket controller"));  
  Serial.println(F("Warning! Always query the controller before attempting to program it!"));  
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
  radio.openWritingPipe(pipes[1]);
  radio.openReadingPipe(1,pipes[0]);


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
    else if ((s1.indexOf("upload")>=0))
    {
      Serial.println("Sending out new program...");
      s1 = "";
      printState(myData);
      // First, stop listening so we can talk.
      radio.stopListening();
      // send data OTA
      radio.write(&myData, sizeof(myData), false);
      // restart listening
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
        "** 1 - stop: deactivate everything and reset all counters\n\r"\
        "** 2 - status: returns the current status of relays and counters, temp, uptime");
      s1 = "";
    }
    
    if (s1 != "")
    {
      Serial.println("Sent command: " + s1);
      char qS[s1.length()+1] ;
      s1.toCharArray(qS, s1.length()+1);
      qS[s1.length()+1] = '\0';

      
      // First, stop listening so we can talk.
      radio.stopListening();
      // send data OTA
      radio.write(qS, s1.length()+1, false);
      // restart listening
      radio.startListening();

      // Wait here until we get a response, or timeout
      unsigned long started_waiting_at = millis();
      bool timeout = false;
      while ( ! radio.available() && ! timeout )
        if (millis() - started_waiting_at > 1500 )
          timeout = true;

      // Describe the results
      if ( timeout )
      {
        Serial.println(F("Failed, response timed out."));
        //return false;
      }
    }
  }
  if (radio.available())
  {
    // this else used to be a part of the timeout check few lines above. 
    //else
    {
      while (radio.available())
      {
        // Fetch the payload, and see if this was the last one.
        uint8_t len = radio.getDynamicPayloadSize();
        Serial.print(len);
        Serial.println(" bytes received:");
      
        if ( len == sizeof(relayctl) )
        {
          relayctl oTemp;
          radio.read( &oTemp, len);
          printState(oTemp);
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


void printState(relayctl& myData)
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
  Serial.println();
  
  /*unsigned long uBufSize = sizeof(myData);
  char pBuffer[uBufSize];
  memcpy(pBuffer, &myData, uBufSize);
  
  for(int i = 0; i<uBufSize;i++) {
     Serial.print(pBuffer[i]);
   }*/
   Serial.println("RF24-BLOB-BEGIN");
   Serial.write((uint8_t *)&myData, sizeof(myData));
   
}

