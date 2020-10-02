/**
 * TempHumidityMonitor
 *
 * Collect temperature and humidity data from a SHT10 sensor
 * attached to a NodeMCU-32s and send status and warning emails
 * to the defined user.
 * 
 * Requires:
 * https://github.com/practicalarduino/SHT1x
 * https://github.com/mobizt/ESP32-Mail-Client
 *
 * Copyright 2020 Mark Asselstine <mark@tundrafam.ca>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 */

#include <Arduino.h>
#include <SHT1x.h>
#include "ESP32_MailClient.h"

// WiFi details
#define WIFI_SSID "*****"
#define WIFI_PASSWORD "***************"

// SHT10 object config
#define dataPin  13
#define clockPin 12
SHT1x sht1x(dataPin, clockPin);

// Mail config
#define MAIL_HOST       "yourmail.com"
#define MAIL_PORT       587
#define MAIL_FROM_ADDR  "sensor@yourmail.com"
#define MAIL_TO_ADDR    "you@yourmail.com"
#define MAIL_PASSWD     "***********"
#define MAIL_FROM_USER  "Cottage Monitor"

// Data collection tuning
#define SAMPLE_MINUTES  30
#define SAMPLES_PER_DAY (24 * 60 / SAMPLE_MINUTES)

// Temperature thresholds
#define MIN_TEMP 10
#define MAX_TEMP 45

//The Email Sending data object contains config and data to send
SMTPData smtpData;

struct SensorData {
  float temp;
  float humidity;
};

// Call to read the SHT10 sensor in order to get the temp and humidity
void readTempAndHumidity(SensorData *sd)
{
  // Read values from the sensor
  sd->temp = sht1x.readTemperatureC();
  sd->humidity = sht1x.readHumidity();

  // Print the values to the serial port
  Serial.print("Temperature: ");
  Serial.print(sd->temp, DEC);
  Serial.print("C, Humidity: ");
  Serial.print(sd->humidity);
  Serial.println("%");
}

// Send a welcome message including current temperature and humidity
void sendWelcomeMsg(SensorData *sd)
{
  String str = "The cottage monitor is up and running. ";
  str = String(str + "It is currently ");
  str = String(str + sd->temp + "C ");
  str = String(str + "with " + sd->humidity + "% humidity. ");
  str = String(str + "The first daily report will be in 24hrs. ");
  str = String(str + "Threshold warnings will be sent if a temperature outside of the range ");
  str = String(str + MIN_TEMP + " - " + MAX_TEMP + " C is read.\n");
  
  smtpData.setSubject("Cottage monitor is OK.");
  smtpData.setMessage(str, false);

  if (!MailClient.sendMail(smtpData))
    Serial.println("Error sending welcome Email, " + MailClient.smtpErrorReason()); 
  else
    Serial.println("Welcome Email sent.");
}

// Send the current temperature and humidity without delay
void sendOnDemandMsg(SensorData *sd)
{
  String str = "It is currently ";
  str = String(str + sd->temp + "C ");
  str = String(str + "with " + sd->humidity + "% humidity. ");
  
  smtpData.setSubject("Cottage monitor on-demand data.");
  smtpData.setMessage(str, false);

  if (!MailClient.sendMail(smtpData))
    Serial.println("Error sending on-demand Email, " + MailClient.smtpErrorReason()); 
  else
    Serial.println("On-demand Email sent.");
}

// Send a daily summary including all samples in the last 24 hours
void sendDailyMsg(SensorData sd[])
{
  float avgTemp = 0, avgHumidity = 0;
  String str = "Here is your daily summary.\n\n";

  // Calculate average temp and humidity over the past 24 hours
  for(int i = 0; i < SAMPLES_PER_DAY; i++) {
    avgTemp += sd[i].temp;
    avgHumidity += sd[i].humidity;
  }
  avgTemp = avgTemp / SAMPLES_PER_DAY;
  avgHumidity = avgHumidity / SAMPLES_PER_DAY;
  
  str = String(str + "The average temperature has been: " + avgTemp + "C.\n");
  str = String(str + "The average humidity has been: " + avgHumidity + "%.\n");
  str = String(str + "Samples for the past 24 hours (taken every " + SAMPLE_MINUTES + " minutes) are:\n\n");
  str = String(str + "SampleID, Temperature, Humidity\n");
  for(int i = 0; i < SAMPLES_PER_DAY; i++) {
    str = String(str + i + ", " + sd[i].temp + ", " + sd[i].humidity + "\n");
  }
  str = String(str + "\nThe next daily report will be in 24 hours.\n");

  smtpData.setSubject("Cottage monitor daily summary.");
  smtpData.setMessage(str, false);

  if (!MailClient.sendMail(smtpData))
    Serial.println("Error sending daily summary Email, " + MailClient.smtpErrorReason());   
  else
    Serial.println("Daily summary Email sent.");
}

// Send a threshold warning message including a dump of the last 24 hours of samples
void sendThresholdMsg(SensorData sd[], int idx)
{
  String str = "!!! Temperature threshold has been crossed !!!\n";
  str = String(str + "Multiple threshold warnings over a short period of time could indicate an issue.\n");
  str = String(str + "The last temperature reading was: " + sd[idx].temp + "C.\n");
  str = String(str + "+++++++++\n");
  for(int i = 0; i < SAMPLES_PER_DAY; i++) {
    str = String(str + sd[i].temp + ", ");
  }
  str = String(str + "\n");
  
  smtpData.setSubject("Cottage monitor threshold warning.");
  smtpData.setMessage(str, false);

  smtpData.setPriority("High");
  if (!MailClient.sendMail(smtpData))
    Serial.println("Error sending threshold warning Email, " + MailClient.smtpErrorReason());
  else
    Serial.println("Threshold warning Email sent.");

  smtpData.setPriority("Normal");
}

// Measure the current temperature and humidity and save it in an array that will hold 24 hours of data
void collectSample()
{
  static int sampleNum = 0;
  static SensorData sd[SAMPLES_PER_DAY];

  readTempAndHumidity(&sd[sampleNum]);

  // Check that we haven't crossed a threshold
  if(sd[sampleNum].temp < MIN_TEMP || sd[sampleNum].temp > MAX_TEMP)
    sendThresholdMsg(sd, sampleNum);

  // Increment/wrap the sampleNum and send the daily summary on a wrap
  sampleNum += 1;
  if(sampleNum >= SAMPLES_PER_DAY)
  {
    sendDailyMsg(sd);
    sampleNum = 0;
  }  
}

// Run once at startup. Sets up the WiFi connection and sends the
// welcome message to let us know things are working as expected.
void setup()
{
  SensorData sd;
  
  Serial.begin(115200); // Open serial connection to report values to host
  Serial.println("Starting up");

  // Setup the mail on demand pin (IOO/Boot)
  pinMode(0, INPUT);
  Serial.println("Setup IOO/Boot Button for on demand email notification.");

  // WiFi connect
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(200);
  }

  Serial.println("");
  Serial.println("WiFi connected.");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.println();

  // Mail config
  // Set the Email host, port, account and password
  smtpData.setLogin(MAIL_HOST, MAIL_PORT, MAIL_FROM_ADDR, MAIL_PASSWD);

  // Set the sender name and Email
  smtpData.setSender(MAIL_FROM_USER, MAIL_FROM_ADDR);

  // Add recipients, can add more than one recipient
  smtpData.addRecipient(MAIL_TO_ADDR);

  // Read the sensor and send welcome msg
  readTempAndHumidity(&sd); // This first read is often crap, don't use it
  delay(2000);
  readTempAndHumidity(&sd);
  sendWelcomeMsg(&sd);
}

// Main loop. Loops every 10 seconds and checks if an on-demand message
// should be sent and/or if a new sample should be collected.
void loop()
{
  static int numloops = 0;
  
  // Send an on-demand email if the IOO/Boot button is being pressed
  if(digitalRead(0) == LOW)
  {
    SensorData sd;
    readTempAndHumidity(&sd);
    sendOnDemandMsg(&sd);
  }

  // Once enough 10s loops pass such that SAMPLE_MINUTES has passed,
  // collect a sample and return the numloops back to zero.
  if((numloops * 10) >= (SAMPLE_MINUTES * 60))
  {
    collectSample();
    numloops = 0;
  }

  // Always increment numloops, even after we sample and set numloops to zero
  // otherwise we will have an extra 10 seconds after each sample.
  numloops += 1;

  // Wait 10s for the next loop
  delay(1000 * 10);
}
