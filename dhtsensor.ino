#include <PietteTech_DHT.h>

#define DHTPIN D3
#define DHTTYPE DHT22
void dht_wrapper();

PietteTech_DHT DHT(DHTPIN, DHTTYPE, dht_wrapper);

double humidity = 0.0;
double temperature = 0.0;
double dewPoint = 0.0;
int count = 0;
String myName = "/";

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
}

float roundReading(float reading) {
    return ((int) (reading * 10)) / 10.0;
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
    }
    
    Serial.println(temperature, 2);
    Serial.println(humidity, 2);

    Particle.publish("temperature", String(temperature), PRIVATE);
    Particle.publish("humidity", String(humidity), PRIVATE);
    
    Particle.publish(myName + "temperature", String(temperature), PRIVATE);
    Particle.publish(myName + "humidity", String(humidity), PRIVATE);
    
    String condition = "temperature=" + String(temperature) + ",humidity=" + String(humidity);
    String json_condition = "{ \"temperature\": " + String(temperature) + ", \"humidity\": " + String(humidity) + " }";
    
    Particle.publish("condition", condition, PRIVATE);
    Particle.publish("json_condition", json_condition, PRIVATE);
    
    count = (count + 1) % 4;

    delay(30000);
}

void nameHandler(const char *topic, const char *data) {
    myName = "/" + String(data) + "/";
    Particle.publish("my name is: " + String(topic), myName);
}
