//#include <SimpleDHT.h>
//#include <WiFiNINA.h>

#include <stdarg.h>

#include <DHTesp.h>
#include <ESP8266WiFi.h>

#include <Eventually.h>   //event driven calbacks
#include <ArduinoJson.h>
#include <Arduino.h>
#include <ArduinoNATS.h>

/*
 * wifi access
 */
const char* WIFI_SSID = "keygrip";
const char* WIFI_PSK = "sne2keygrip";

WiFiClient client;
Client* clientPointer = &client;
EvtManager mgr;

/*
 * some counters available as get requests
 */
int _count = 0;
int _high = 0;
int _low = 100;


////////////////////////////////////SENSOR ID
//sensor1
//String ID = "c2b420b0-36eb-11e9-b56e-0800200c9a66";
//String NAME = "sensor1";
//String LOCATION = "location1";

//sensor2
String ID = "8d1da580-bd43-4417-90fc-1ce525e4fa24";
String NAME = "sensor2";
String LOCATION = "location2";


NATS nats(
  &client,
  //"demo.nats.io",
  //NATS_DEFAULT_PORT
  //"192.168.86.27",  //rPi
  "192.168.86.30",    //maclinbook
  4222
);

// for DHT11,
//      VCC: 5V or 3V
//      GND: GND
//      DATA: 2

//int pinDHT11 = 2;
//SimpleDHT11 dht11(pinDHT11);

DHTesp dht;

/*
 * connect to wifi
 */
void connect_wifi()
{
  /* Explicitly set the ESP8266 to be a WiFi-client, otherwise, it by default,
     would try to act as both a client and an access-point and could cause
     network-issues with your other WiFi-devices on your WiFi-network. */
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PSK);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

////////////////////////////////////NATS HANDLERS
/*
 * handle action requests
 */
void nats_action_handler(NATS::msg msg)
{
}

/*
 * handle set requests
 */
void nats_set_handler(NATS::msg msg)
{
  Serial.println("setting:" + String(msg.subject) + " with:" + String(msg.data));

  char s[50];
  char* what = "name";

  what = strtok((char*)msg.subject, "."); //ID
  what = strtok(NULL, ".");               //SET
  what = strtok(NULL, ".");               //WHAT

  //  while (what != NULL)
  //  {
  //    Serial.println(what);
  //    what = strtok(NULL, ".");
  //  }


  if (what == NULL)
    Serial.println("OOOOOOOOOPS");

  if (String(what) == String("name"))
  {
    Serial.println("posting reply: set name");

    NAME = String(msg.data);

    nats.publish(msg.reply, "Success");
  }

  if (String(what) == String("location"))
  {
    Serial.println("posting reply: set location");

    LOCATION = String(msg.data);

    nats.publish(msg.reply, "Success");
  }

  //TODO
  //SEND EVENTS AS JSON
  //"EVENT":"PROPERTY_CHANGED"
  //"PROPERTY":"WHICH"
  //

  String topic = ID + ".event";
  nats.publish(topic.c_str(), "PROPERTY_CHANGED");
}

void nats_get_handler(NATS::msg msg)
{
  Serial.println("getting:" + String(msg.data));

  char s[50];

  if (String(msg.data) == String("role"))
  {
    Serial.println("posting reply: role");
    nats.publish(msg.reply, "Sensor");
  }

  if (String(msg.data) == String("type"))
  {
    //Serial.println("posting reply");
    nats.publish(msg.reply, "Temperature");
  }

  if (String(msg.data) == String("subtype"))
  {
    //Serial.println("posting reply");
    nats.publish(msg.reply, "None");
  }

  if (String(msg.data) == String("name"))
  {
    //Serial.println("posting reply");
    nats.publish(msg.reply, NAME.c_str());
  }

  if (String(msg.data) == String("location"))
  {
    //Serial.println("posting reply");
    nats.publish(msg.reply, LOCATION.c_str());
  }

  if (String(msg.data) == String("low"))
  {
    //Serial.println("low");

    sprintf(s, "%d", _low);
    nats.publish(msg.reply, s);
  }

  if (String(msg.data) == String("high"))
  {
    Serial.println("high");

    sprintf(s, "%d", _high);
    nats.publish(msg.reply, s);
  }
}

////////////////////////////////////NATS PUBLISHING
void nats_status(char* status)
{
  String topic = ID + ".status.environment";
  nats.publish(topic.c_str(), status);
}

void nats_status(int temperature, int humidity)
{
  //const int capacity = JSON_OBJECT_SIZE(3);
  //StaticJsonDocument<capacity> jb;

  //JsonObject obj = jb.createObject();

  //obj["temperature"] = temperature;
  //obj["humidity"] = humidity;

  char status[50];
  //obj.printTo(status);

  //
  DynamicJsonDocument doc(50);
  doc["temperature"] = temperature;
  doc["humidity"] = humidity;
  serializeJson(doc, status);
  //

  Serial.println(status);

  nats_status(status);
}

void nats_status(int temperature, int humidity, float batteryLevel)
{
  char status[50];

  //
  DynamicJsonDocument doc(50);
  doc["temperature"] = temperature;
  doc["humidity"] = humidity;
  doc["level"] = batteryLevel;

  serializeJson(doc, status);
  //

  nats_status(status);
}

void nats_heartbeat()
{
  //Serial.println("heartbeat");
  nats.publish("heartbeat", ID.c_str());
}

////////////////////////////////////NATS CALLBACKS
void nats_on_connect()
{
  Serial.println("subscribing");

  String gettopic = ID + ".get";
  String settopic = ID + ".set.>";
  nats.subscribe(gettopic.c_str(), nats_get_handler);
  nats.subscribe(settopic.c_str(), nats_set_handler);
}

void nats_on_disconnect()
{ 
}

void nats_on_error()
{
}

////////////////////////////////////EVENTUALLY CALLBACKS
bool processNats()
{
  nats.process();
  return true;
}

bool processHeartbeat()
{
  nats_heartbeat();
  return true;
}

bool processSensor()
{
  if (WiFi.status() != WL_CONNECTED)
    connect_wifi();

  /////////////////////////////////////////////
  //TEMPERATURE HUMIDITY
  float humidity = dht.getHumidity();
  float temperature = dht.getTemperature();
  
  //LOG TO CONSOLE
  String dhtStatus = dht.getStatusString();
  
  Serial.print(dht.getStatusString());
  Serial.print("\t");
  Serial.print(humidity, 1);
  Serial.print("\t\t");
  Serial.print(temperature, 1);
  Serial.print("\t\t");
  Serial.print(dht.toFahrenheit(temperature), 1);
  Serial.print("\t\t");
  Serial.print(dht.computeHeatIndex(temperature, humidity, false), 1);
  Serial.print("\t\t");
  Serial.println(dht.computeHeatIndex(dht.toFahrenheit(temperature), humidity, true), 1);
    
  //create status to publish over ecomms
  char status[50];
  sprintf(status, "C:%d F:%d H:%d",
          (int)temperature,
          (int)dht.toFahrenheit(temperature),
          (int)humidity);

  if (dht.toFahrenheit(temperature) > _high)
    _high = dht.toFahrenheit(temperature);

  if (dht.toFahrenheit(temperature) < _low)
    _low = dht.toFahrenheit(temperature);

  int a0 = 0;
  
  //NOT SUPPORTED BY ESP8266 NODEMCU
  //int a0 = analogRead(ADC_BATTERY);
  float voltage = a0 * (4.3 / 1023.0);

  //Serial.println(voltage);
  //Serial.print(voltage);
  //Serial.println("V");

  //WAS DHT STATUS OK?
  if(dhtStatus == "OK")
    nats_status(dht.toFahrenheit(temperature), (int)humidity, voltage);

  return true;
}

bool doit()
{
  processNats();
  processSensor();
  processHeartbeat();
}

////////////////////////////////////SETUP
void setup()
{
  Serial.begin(115200);
  Serial.println("Starting!");
  
  mgr.addListener(new EvtTimeListener(2000, true, (EvtAction)doit));
  //mgr.addListener(new EvtTimeListener(1000, true, (EvtAction)processNats));
  //mgr.addListener(new EvtTimeListener(1000, true, (EvtAction)processHeartbeat));  
  //mgr.addListener(new EvtTimeListener(3000, true, (EvtAction)processSensor));

  connect_wifi();

  Serial.println("connecting to nats");
  nats.on_connect = nats_on_connect;
  nats.on_disconnect = nats_on_disconnect;
  nats.on_error = nats_on_error;
  nats.connect();
  
  Serial.println("connected to nats!");

  //SETUP DHT
  // Autodetect is not working reliable, don't use the following line
  // dht.setup(17);
  // use this instead: 
  dht.setup(D4, DHTesp::DHT11);

  Serial.println("Status\tHumidity (%)\tTemperature (C)\t(F)\tHeatIndex (C)\t(F)");
  String thisBoard= ARDUINO_BOARD;
  Serial.println(thisBoard);  
}

////////////////////////////////////LOOP
USE_EVENTUALLY_LOOP(mgr) // Use this instead of your loop() function.
