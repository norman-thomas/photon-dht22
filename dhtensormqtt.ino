#include <MQTT.h>

#include <PietteTech_DHT.h>
#include <math.h>

#define DHTPIN D3
#define DHTTYPE DHT22
void dht_wrapper();

PietteTech_DHT DHT(DHTPIN, DHTTYPE, dht_wrapper);

double humidity = 0.0;
double temperature = 0.0;
double dewPoint = 0.0;
double dewPointSlow = 0.0;
int count = 0;
String myName = "/";
String myPureName = "";


void callback(char* topic, byte* payload, unsigned int length);

/**
 * if want to use IP address,
 * byte server[] = { XXX,XXX,XXX,XXX };
 * MQTT client(server, 1883, callback);
 * want to use domain name,
 * MQTT client("www.sample.com", 1883, callback);
 **/
byte server[] = { 192, 168, 178, 1 };
MQTT client(server, 1883, callback);

// for QoS2 MQTTPUBREL message.
// this messageid maybe have store list or array structure.
uint16_t qos2messageid = 0;

// recieve message
void callback(char* topic, byte* payload, unsigned int length) {
    char p[length + 1];
    memcpy(p, payload, length);
    p[length] = NULL;

    if (!strcmp(p, "RED"))
        RGB.color(255, 0, 0);
    else if (!strcmp(p, "GREEN"))
        RGB.color(0, 255, 0);
    else if (!strcmp(p, "BLUE"))
        RGB.color(0, 0, 255);
    else
        RGB.color(255, 255, 255);
    delay(1000);
}

// QOS ack callback.
// if application use QOS1 or QOS2, MQTT server sendback ack message id.
void qoscallback(unsigned int messageid) {
    Serial.print("Ack Message Id:");
    Serial.println(messageid);

    if (messageid == qos2messageid) {
        Serial.println("Release QoS2 Message");
        client.publishRelease(qos2messageid);
    }
}


void dht_wrapper() {
    DHT.isrCallback();
}

void setup() {
    Serial.begin(9600);

    Particle.variable("temperature", temperature);
    Particle.variable("humidity", humidity);
    Particle.variable("dewPoint", dewPoint);
    
    RGB.control(true);
    RGB.brightness(0);
    
    Particle.subscribe("spark/device/name", nameHandler);
    Particle.publish("spark/device/name");
    
    // connect to the server
    client.connect("sparkclient");

    // add qos callback. If don't add qoscallback, ACK message from MQTT server is ignored.
    client.addQosCallback(qoscallback);

    // publish/subscribe
    if (client.isConnected()) {
        // it can use messageid parameter at 4.
        uint16_t messageid;
        client.publish("outTopic/message", "hello world QOS1", MQTT::QOS1, &messageid);
        Serial.println(messageid);

        // if 4th parameter don't set or NULL, application can not check the message id to the ACK message from MQTT server. 
        client.publish("outTopic/message", "hello world QOS1(message is NULL)", MQTT::QOS1);
        
        // QOS=2
        client.publish("outTopic/message", "hello world QOS2", MQTT::QOS2, &messageid);
        Serial.println(messageid);

        // save QoS2 message id as global parameter.
        qos2messageid = messageid;

        client.subscribe("inTopic/message");
    }
}

float roundReading(float reading) {
    return (floor(reading * 100)) / 100.0;
}

void loop() {
    if (count == 0) {
        int result = 0;
        while ((result = DHT.acquireAndWait()) != DHTLIB_OK) {
            Serial.println("not ready to read yet, waiting...");
            delay(500);
        }
        
        humidity = roundReading( DHT.getHumidity() );
        temperature = roundReading( DHT.getCelsius() );
        dewPoint = roundReading( DHT.getDewPoint() );
        dewPointSlow = roundReading( DHT.getDewPointSlow() );
    
        client.publish(myPureName + "/temperature", String(temperature));
        client.publish(myPureName + "/humidity", String(humidity));
        client.publish(myPureName + "/dewPoint", String(dewPoint));
        client.publish(myPureName + "/dewPointSlow", String(dewPointSlow));
    }
    
    Serial.println(temperature, 2);
    Serial.println(humidity, 2);
    Serial.println(dewPoint, 2);
    
    //Particle.publish(myName + "temperature", String(temperature), 60, PRIVATE);
    //Particle.publish(myName + "humidity", String(humidity), 60, PRIVATE);
    
    String json_condition = "{ \"temperature\": " + String(temperature) + ", \"humidity\": " + String(humidity) + ", \"dewPoint\":" + String(dewPoint) + ", \"dewPointSlow\":" + String(dewPointSlow) + " }";
    Particle.publish(myName + "json_condition", json_condition, 60, PRIVATE);
    
    count = (count + 1) % 2;
    
    if (client.isConnected())
        client.loop();
    
    delay(30000);
}

void nameHandler(const char *topic, const char *data) {
    myPureName = String(data);
    myName = "/" + myPureName + "/";
    Particle.publish("my name is: " + String(topic), myName);
}
