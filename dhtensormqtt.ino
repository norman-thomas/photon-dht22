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
String myName = "";


void callback(char* topic, byte* payload, unsigned int length);

MQTT client("SERVER", 1883, 120, callback);

// for QoS2 MQTTPUBREL message.
// this messageid maybe have store list or array structure.
uint16_t qos2messageid = 0;

void publish() {
    client.publish(myName + "/temperature", String(temperature));
    client.publish(myName + "/humidity", String(humidity));
    client.publish(myName + "/dewPoint", String(dewPoint));
    client.publish(myName + "/dewPointSlow", String(dewPointSlow));
    
    String json_condition = "{ \"temperature\": " + String(temperature) + ", \"humidity\": " + String(humidity) + ", \"dewPoint\":" + String(dewPoint) + ", \"dewPointSlow\":" + String(dewPointSlow) + " }";
    Particle.publish("/" + myName + "/json_condition", json_condition, 60, PRIVATE);
}

// receive message
void callback(char* topic, byte* payload, unsigned int length) {
    publish();
    delay(500);
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
    client.connect(System.deviceID());

    // add qos callback. If don't add qoscallback, ACK message from MQTT server is ignored.
    client.addQosCallback(qoscallback);
    
    client.subscribe(myName + "/refresh");
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
    }
    
    Serial.println(temperature, 2);
    Serial.println(humidity, 2);
    Serial.println(dewPoint, 2);
    
    if (myName.length() > 0)
    {
        if (!client.isConnected())
        {
            Particle.publish("/" + myName + "/mqtt", "reconnecting to MQTT server", 60, PRIVATE);
            client.connect(System.deviceID());
        }
        
        publish();
    }
    
    count = (count + 1) % 2;
    
    if (client.isConnected())
        client.loop();
    
    delay(30000);
}

void nameHandler(const char *topic, const char *data) {
    myName = String(data);
    Particle.publish("my name is:", myName);
}
