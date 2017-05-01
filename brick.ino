#include "DHT.h";
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_TSL2561_U.h>

Adafruit_TSL2561_Unified tsl = Adafruit_TSL2561_Unified(TSL2561_ADDR_FLOAT, 12345);

const int tempPin = D3;
DHT dht(tempPin, DHT11);

void setup() {
  Serial.begin(9600);

  /* Initialise the sensor */
  if(!tsl.begin())
  {
    Serial.print("No TSL2561 detected");
    while(1);
  }

  tsl.enableAutoRange(true);
  tsl.setIntegrationTime(TSL2561_INTEGRATIONTIME_13MS);

  Serial.println("");
}

void loop()
{

  /* Get a new sensor event */
  sensors_event_t event;
  tsl.getEvent(&event);

  /* Display the results (light is measured in lux) */
  if (event.light)
  {
    Serial.print("Ilumination = ");
    Serial.print(event.light);
    Serial.println(" lux");
  }
  else
  {
    Serial.println("LUX Sensor overload");
  }

  Serial.print("Temperature = ");
  Serial.println( dht.readTemperature());
  Serial.print("Humidity = ");
  Serial.println(dht.readHumidity());
  Serial.println();

  delay(5000);
}
