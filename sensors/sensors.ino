#include <Eventually.h>

#include <ArduinoJson.h>
#include <SimpleDHT.h>
#include <stdarg.h>
#include <WiFiNINA.h>
#include <Arduino.h>
#include <ArduinoNATS.h>

const char* WIFI_SSID = "keygrip";
const char* WIFI_PSK = "sne2keygrip";
const int ldr_pin = 3;

WiFiClient client;
Client* clientPointer = &client;
EvtManager mgr;

//not used yet!
//start thinking about how to organize code into class library
class ECommsEntity
{
    NATS* _nats;
    String _id;

  public:
    ECommsEntity(NATS* nats, String id): _nats(nats), _id(id)
    {
    }

    //publish
    //register ( subscribe )
};

class ECommsClient : public ECommsEntity
{
  public:
    ECommsClient(NATS* nats, String id): ECommsEntity(nats, id)
    {
    }

    void doGet(NATS::msg msg)
    {
    }
};

class ECommsParticipant : public ECommsEntity
{
  public:
    ECommsParticipant(NATS* nats, String id): ECommsEntity(nats, id)
    {
    }

    void onGet(NATS::msg msg)
    {
    }
};

//until an ecomms address service is written and running
//each sensor needs a unique id

//sensor1
//String ID = "c2b420b0-36eb-11e9-b56e-0800200c9a66";

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
int pinDHT11 = 2;
SimpleDHT11 dht11(pinDHT11);

void connect_wifi()
{
  //WiFi.mode(WIFI_STA);

  //WiFi.begin(WIFI_SSID, WIFI_PSK);
  //while (WiFi.status() != WL_CONNECTED) {
  //  yield();
  //}

  // attempt to connect to Wifi network:
  //  Serial.print("Attempting to connect to WPA SSID: ");
  //  Serial.println(WIFI_SSID);
  while (WiFi.begin(WIFI_SSID, WIFI_PSK) != WL_CONNECTED) {
    // failed, retry
    //Serial.print(".");
    //delay(5000);
    yield();
  }

  //Serial.println("connected");
}

int _count = 0;
int _high = 0;
int _low = 100;

void nats_action_handler(NATS::msg msg)
{
}

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
  nats.publish("heartbeat", ID.c_str());
}

void nats_on_connect()
{
  Serial.println("subscribing");

  String gettopic = ID + ".get";
  String settopic = ID + ".set.>";
  nats.subscribe(gettopic.c_str(), nats_get_handler);
  nats.subscribe(settopic.c_str(), nats_set_handler);
}

bool processNats()
{
  nats.process();
  return false;
}

bool processHeartbeat()
{
  nats_heartbeat();
  return false;
}

void setup()
{
  ECommsParticipant snp(&nats, ID.c_str());

  pinMode(ldr_pin, INPUT);

  mgr.addListener(new EvtTimeListener(1000, true, (EvtAction)processNats));
  mgr.addListener(new EvtTimeListener(2000, true, (EvtAction)processHeartbeat));
  mgr.addListener(new EvtTimeListener(2000, true, (EvtAction)processSensor));

  connect_wifi();

  nats.on_connect = nats_on_connect;
  nats.connect();
}

bool _didit = false;

USE_EVENTUALLY_LOOP(mgr) // Use this instead of your loop() function.

bool processSensor()
{
  if (WiFi.status() != WL_CONNECTED)
    connect_wifi();

  // read without samples.
  byte temperature = 0;
  byte humidity = 0;
  int err = SimpleDHTErrSuccess;
  if ((err = dht11.read(&temperature, &humidity, NULL)) != SimpleDHTErrSuccess) {
    Serial.print("Read DHT11 failed, err="); Serial.println(err); delay(1000);
    return false;
  }

  Serial.println("Sample OK: ");

  Serial.println((int)temperature);

  int f = (temperature * 9 / 5) + 32;

  Serial.println((int)f);

  char status[50];
  sprintf(status, "C:%d F:%d H:%d",
          (int)temperature,
          (int)(temperature * 9 / 5) + 32,
          (int)humidity);

  if (f > _high)
    _high = f;

  if (f < _low)
    _low = f;

  int a0 = analogRead(ADC_BATTERY);
  float voltage = a0 * (4.3 / 1023.0);

  //Serial.println(voltage);
  //Serial.print(voltage);
  //Serial.println("V");

  if (digitalRead(ldr_pin ) == 1)
    Serial.println("DARK");

  //Serial.println(digitalRead(ldr_pin));

  nats_status((int)(temperature * 9 / 5) + 32, (int)humidity, voltage);

  return false;
}
