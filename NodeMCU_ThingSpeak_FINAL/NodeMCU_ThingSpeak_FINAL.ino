#include <ESP8266WiFi.h>
#include <ThingSpeak.h>
#include <DHT.h>
#include <SoftwareSerial.h>
                  
SoftwareSerial mySerial(D7, D8);

WiFiClient client;
DHT dht(D5, DHT11);

long myChannelNumber = 1272933;
const char myWriteAPIKey[] = "KHUQ89KU554JG8VZ";
#define windDir A0

int sensorExp[] = {66,84,93,126,184,244,287,406,461,599,630,702,785,827,886,945};
float dirDeg[] = {112.5,67.5,90,157.5,135,202.5,180,22.5,45,247.5,225,337.5,0,292.5,315,270};
char* dirCard[] = {"ESE","ENE","E","SSE","SE","SSW","S","NNE","NE","WSW","SW","NNW","N","WNW","NW","W"};

int sensorMin[] = {63,80,89,120,175,232,273,385,438,569,613,667,746,812,869,931};
int sensorMax[] = {69,88,98,133,194,257,301,426,484,612,661,737,811,868,930,993};

int incoming = 0;
float angle = 0;
#include "RTClib.h"
#include <Wire.h>
#define RainPin 2                         // The Rain input is connected to digital pin 2 on the arduino

bool bucketPositionA = false;             // one of the two positions of tipping-bucket               
const double bucketAmount = 0.01610595;   // inches equivalent of ml to trip tipping-bucket
double dailyRain = 0.0;                   // rain accumulated for the day
float hourlyRain = 0.0;                  // rain accumulated for one hour
double dailyRain_till_LastHour = 0.0;     // rain accumulated for the day till the last hour          
bool first;                               // as we want readings of the (MHz) loops only at the 0th moment 

RTC_Millis rtc;                           // software RTC time

const byte   interruptPin = D1; // Or other pins that support an interrupt
unsigned int Debounce_Timer, Current_Event_Time, Last_Event_Time, Event_Counter;
float        WindSpeed;

  float h = angle;
  float t = hourlyRain;
  float s = WindSpeed;
  float q = dht.readHumidity();
  float r = dht.readTemperature();
  
void setup() {
  // put your setup code here, to run once:
   Serial.begin(9600);
   mySerial.begin(115200);   // Setting the baud rate of GSM Module 
  Serial.begin(115200);    // Setting the baud rate of Serial Monitor (Arduino)
  WiFi.begin("VARUN-2G", "VARUN259696");
  while(WiFi.status() != WL_CONNECTED)
  {
    delay(200);
    Serial.print("..");
  }
  Serial.println();
  Serial.println("NodeMCU is connected!");
  Serial.println(WiFi.localIP());
  ThingSpeak.begin(client);
   rtc.begin(DateTime(__DATE__, __TIME__));       // start the RTC
  pinMode(RainPin, INPUT);                       // set the Rain Pin as input.
  delay(4000);                                   // i'm slow in starting the seiral monitor (not necessary)
  Serial.println("Ready!!!");
  pinMode(interruptPin, INPUT_PULLUP);
  noInterrupts();                                // Disable interrupts during set-up
  attachInterrupt(digitalPinToInterrupt(interruptPin), WSpeed_ISR, RISING); //Respond to a LOW on the interrupt pin and goto WSpeed_ISR
  timer0_isr_init();                             // Initialise Timer-0
  timer0_attachInterrupt(Timer_ISR);             // Goto the Timer_ISR function when an interrupt occurs
  timer0_write(ESP.getCycleCount() + 80000000L); // Pre-load Timer-0 with a time value of 1-second
  interrupts();
   Serial.begin(9600);
  
  //Begin serial communication with Arduino and SIM800L
  mySerial.begin(9600);

  Serial.println("Initializing..."); 
  delay(1000);

  mySerial.println("AT"); //Once the handshake test is successful, it will back to OK
  updateSerial();

  mySerial.println("AT+CMGF=1"); // Configuring TEXT mode
  updateSerial();
  mySerial.println("AT+CMGS=\"+918277235177\"");//change ZZ with country code and xxxxxxxxxxx with phone number to sms
  updateSerial();
  mySerial.print("HELLO"); //text content
  updateSerial();
  mySerial.write(26);               
}

void loop() {
  incoming = analogRead(windDir);
  for(int i=0; i<=15; i++){
    if(incoming >= sensorMin[i] && incoming <= sensorMax[i]){
      angle = dirDeg[i];
      break;
    }
  }
  Serial.print(angle);
  Serial.print(" ");
  delay(25);

    DateTime now = rtc.now();
    
  // ++++++++++++++++++++++++ Count the bucket tips ++++++++++++++++++++++++++++++++
  if ((bucketPositionA==false)&&(digitalRead(RainPin)==HIGH)){
    bucketPositionA=true;
    dailyRain+=bucketAmount;                               // update the daily rain
  }
  
  if ((bucketPositionA==true)&&(digitalRead(RainPin)==LOW)){
    bucketPositionA=false;  
  } 
  // +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  
  if(now.minute() != 0) first = true;                     // after the first minute is over, be ready for next read
  
  if(now.minute() == 0 && first == true){
 
    hourlyRain = dailyRain - dailyRain_till_LastHour;      // calculate the last hour's rain
    dailyRain_till_LastHour = dailyRain;                   // update the rain till last hour for next calculation
    
    // facny display for humans to comprehend
    Serial.println();
    Serial.print(now.hour());
    Serial.print(":");
    Serial.print(now.minute());
    Serial.print(":  Total Rain for the day = ");
    Serial.print(dailyRain,8);                            // the '8' ensures the required accuracy
    Serial.println(" inches");
    Serial.println();
    Serial.print("     :  Rain in last hour = ");
    Serial.print(hourlyRain,8);
    Serial.println(" inches");
    Serial.println();
    
    first = false;                                        // execute calculations only once per hour
  }
  
  if(now.hour()== 0) {
    dailyRain = 0.0;                                      // clear daily-rain at midnight
    dailyRain_till_LastHour = 0.0;                        // we do not want negative rain at 01:00
  }
  // put your main code here, to run repeatedly:
 
  Serial.println("Amount of rain: " + (String) t);
  Serial.println("Wind Direction: " + (String) h);
  Serial.println("Humidity: " + (String) q);
  Serial.println("Temperature: " + (String) r);
  Serial.print("WindSpeed:"+String(WindSpeed)+ " MPH");
  SendMessage();
  ThingSpeak.writeField(myChannelNumber, 1, t, myWriteAPIKey);
  ThingSpeak.writeField(myChannelNumber, 2, h, myWriteAPIKey);
  ThingSpeak.writeField(myChannelNumber, 3, s, myWriteAPIKey);
  ThingSpeak.writeField(myChannelNumber, 4, q, myWriteAPIKey);
  ThingSpeak.writeField(myChannelNumber, 5, r, myWriteAPIKey);
  delay(2000);
}

ICACHE_RAM_ATTR void WSpeed_ISR (void) {
  if (!(millis() - Debounce_Timer) < 5) {
    Debounce_Timer = millis();                                        // Set debouncer to prevent false triggering
    Event_Counter++;
  }
}

void Timer_ISR (void) {                                                       // Timer reached zero, now re-load it to repeat
  timer0_write(ESP.getCycleCount() + 80000000L);                              // Reset the timer, do this first for timing accuracy
  WindSpeed = Event_Counter*2.5/2;
  Serial.println(Event_Counter);
  Event_Counter =0;
}

void updateSerial()
{
  delay(500);
  while (Serial.available()) 
  {
    mySerial.write(Serial.read());//Forward what Serial received to Software Serial Port
  }
  while(mySerial.available()) 
  {
    Serial.write(mySerial.read());//Forward what Software Serial received to Serial Port
  }
}

 void SendMessage()


{

  mySerial.println("AT+CMGF=1");    //Sets the GSM Module in Text Mode

  delay(1000);  // Delay of 1000 milli seconds or 1 second

  mySerial.println("AT+CMGS=\"+918277235177\"\r"); // Replace x with mobile number

  delay(1000);

  mySerial.println("Amount of rain: " + (String) t+"\n"+"Wind Direction: " + (String) h+"\n"+"Humidity: " + (String) q+"\n"+"Temperature: " + (String) r+"\n"+"WindSpeed:"+String(WindSpeed)+ " MPH");// The SMS text you want to send
  delay(1000);

  mySerial.println((char)26);// ASCII code of CTRL+Z

  delay(1000);

}
