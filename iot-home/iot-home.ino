#include <Arduino.h>
#include <Stream.h>

#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>

//AWS
#include "sha256.h"
#include "Utils.h"
#include "AWSClient2.h"

//WEBSockets
#include <Hash.h>
#include <WebSocketsClient.h>

//MQTT PAHO
#include <SPI.h>
#include <IPStack.h>
#include <Countdown.h>
#include <MQTTClient.h>

//AWS MQTT Websocket
#include "Client.h"
#include "AWSWebSocketClient.h"
#include "CircularByteBuffer.h"

//SENSORS
#include "DHT.h";
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_TSL2561_U.h>

//JSON
#include <ArduinoJson.h>

//PROGRAM CONFIGURATION
#include "config.h"

//-----------------------------
// CONFIGURATION
//-----------------------------

int INFO_LED = D5;

//Delay between messages
int periodSec = 300;

int port = 443;

//MQTT config
const int maxMQTTpackageSize = 512;
const int maxMQTTMessageHandlers = 1;

//DHT config
const int DHT_PIN = D3;


//-----------------------------
// VARIABLE DECLARATION
//-----------------------------

//# of connections
long connection = 0;

//count messages arrived
int arrivedcount = 0;


//DHT
DHT dht(DHT_PIN, DHT11);

//TSL2561
Adafruit_TSL2561_Unified tsl = Adafruit_TSL2561_Unified(TSL2561_ADDR_FLOAT, 12345);

ESP8266WiFiMulti WiFiMulti;

AWSWebSocketClient awsWSclient(1000);

IPStack ipstack(awsWSclient);
MQTT::Client<IPStack, Countdown, maxMQTTpackageSize, maxMQTTMessageHandlers> *client = NULL;


//-----------------------------
// PROCEDURES
//-----------------------------

void establishWIFIConnection () {
  //TODO: Add easy config for selecting wifi endpoint
  //fill with ssid and wifi password
  WiFiMulti.addAP(wifi_ssid, wifi_password);
  Serial.println ("connecting to wifi");
  while(WiFiMulti.run() != WL_CONNECTED) {
      delay(100);
      Serial.print (".");
  }
  Serial.println ("\nconnected");
}

void initAWS() {
  //fill AWS parameters
  awsWSclient.setAWSRegion(aws_region);
  awsWSclient.setAWSDomain(aws_endpoint);
  awsWSclient.setAWSKeyID(aws_key);
  awsWSclient.setAWSSecretKey(aws_secret);
  awsWSclient.setUseSSL(true);

  if (connect ()){
    subscribe ();

    /*build init message
    StaticJsonBuffer<200> jsonBuffer;
    JsonObject& reported = jsonBuffer.createObject();
    reported["online"] = true;

    sendmessage(reported);*/
  }
}

// initialise TSL2561 sensor
void initTSL2561 () {
  if(!tsl.begin())
  {
    Serial.println("No TSL2561 detected");
    while(1);
  }

  tsl.enableAutoRange(true);
  tsl.setIntegrationTime(TSL2561_INTEGRATIONTIME_13MS);

  Serial.println("TSL2561 initialised");
}

//generate random mqtt clientID
char* generateClientID () {
  char* cID = new char[23]();
  for (int i=0; i<22; i+=1)
    cID[i]=(char)random(1, 256);
  return cID;
}

//callback to handle mqtt messages
void messageArrived(MQTT::MessageData& md) {
  MQTT::Message &message = md.message;

  Serial.print("Message ");
  Serial.print(++arrivedcount);
  Serial.print(" arrived: qos ");
  Serial.print(message.qos);
  Serial.print(", retained ");
  Serial.print(message.retained);
  Serial.print(", dup ");
  Serial.print(message.dup);
  Serial.print(", packetid ");
  Serial.println(message.id);
  Serial.print("Payload ");
  char* msg = new char[message.payloadlen+1]();
  memcpy (msg,message.payload,message.payloadlen);
  Serial.println(msg);
  delete msg;
}

//connects to websocket layer and mqtt layer
bool connect () {

    if (client == NULL) {
      client = new MQTT::Client<IPStack, Countdown, maxMQTTpackageSize, maxMQTTMessageHandlers>(ipstack);
    } else {

      if (client->isConnected ()) {
        client->disconnect ();
      }
      delete client;
      client = new MQTT::Client<IPStack, Countdown, maxMQTTpackageSize, maxMQTTMessageHandlers>(ipstack);
    }


    //delay is not necessary... it just help us to get a "trustful" heap space value
    delay (1000);
    Serial.print (millis ());
    Serial.print (" - conn: ");
    Serial.print (++connection);
    Serial.print (" - (");
    Serial.print (ESP.getFreeHeap ());
    Serial.println (")");




    int rc = ipstack.connect(aws_endpoint, port);
    if (rc != 1)
    {
      Serial.println("error connection to the websocket server");
      return false;
    } else {
      Serial.println("websocket layer connected");
    }


    Serial.println("MQTT connecting");
    MQTTPacket_connectData data = MQTTPacket_connectData_initializer;
    data.MQTTVersion = 3;
    char* clientID = generateClientID ();
    data.clientID.cstring = clientID;
    rc = client->connect(data);
    delete[] clientID;
    if (rc != 0)
    {
      Serial.print("error connection to MQTT server");
      Serial.println(rc);
      return false;
    }
    Serial.println("MQTT connected");
    return true;
}

//subscribe to a mqtt topic
void subscribe () {
   //subscript to a topic
    int rc = client->subscribe(aws_topic, MQTT::QOS0, messageArrived);
    if (rc != 0) {
      Serial.print("rc from MQTT subscribe is ");
      Serial.println(rc);
      return;
    }
    Serial.println("MQTT subscribed");
}

//send a message to a mqtt topic
void sendmessage (JsonObject& reported) {
    //send a message
    MQTT::Message message;
    StaticJsonBuffer<200> jsonBuffer;
    JsonObject& state = jsonBuffer.createObject();
    JsonObject& payload = jsonBuffer.createObject();
    state["reported"] = reported;
    payload["state"] = state;
    payload.printTo(Serial);


    char buf[200];
    payload.printTo(buf);
    //strcpy(buf, "{\"state\":{\"reported\":{\"on\": false}, \"desired\":{\"on\": false}}}");
    message.qos = MQTT::QOS0;
    message.retained = false;
    message.dup = false;
    message.payload = (void*)buf;
    message.payloadlen = strlen(buf)+1;
    int rc = client->publish(aws_topic, message);
}


void readSensorsData(JsonObject& data) {

  /* Get a new sensor event */
  sensors_event_t event;
  tsl.getEvent(&event);

  /* Display the results (light is measured in lux) */
  if (event.light)
  {
    data["light"] = event.light;

    /* Serial.print("Ilumination = ");
    Serial.print(event.light);
    Serial.println(" lux"); */
  }
  else
  {
    Serial.println("LUX Sensor overload");
  }

  data["temp"] = dht.readTemperature();
  data["hum"] = dht.readHumidity();

  /* Serial.print("Temperature = ");
  Serial.println( dht.readTemperature());
  Serial.print("Humidity = ");
  Serial.println(dht.readHumidity());
  Serial.println(); */
}

void work() {
  digitalWrite(INFO_LED, LOW);   // turn the LED on (HIGH is the voltage level)

  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& sensor_data = jsonBuffer.createObject();

  //keep the mqtt up and running
  if (awsWSclient.connected ()) {
      client->yield();
  } else {
    //handle reconnection
    if (connect ()){
      subscribe ();
    }
  }

  readSensorsData(sensor_data);
  sendmessage(sensor_data);


  digitalWrite(INFO_LED, HIGH);
}

//-----------------------------
// MAIN FLOW
//-----------------------------

void setup() {
    pinMode(INFO_LED, OUTPUT);

    Serial.begin (115200);
    while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB
	  }
    Serial.setDebugOutput(1);

    establishWIFIConnection();

    initAWS();

    initTSL2561();

    work();
    Serial.println("ESP8266 in sleep mode");
    ESP.deepSleep(periodSec * 1000000);
    delay(1000);
}

void loop() {

}
