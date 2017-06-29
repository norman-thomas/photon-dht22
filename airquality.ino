#include <MQTT.h>
#include <PietteTech_DHT.h>
#include <math.h>

#define ONE_DAY_MILLIS (24 * 60 * 60 * 1000)
#define COOLING (5 * 60)
#define DEFAULT_DELAY 5000

#define PPD_PIN D5
#define DHTPIN D3
#define DHTTYPE DHT22

void dht_wrapper();
PietteTech_DHT DHT(DHTPIN, DHTTYPE, dht_wrapper);

uint16_t qos2messageid = 0;
void callback(char* topic, byte* payload, unsigned int length);
MQTT client("pluto.fritz.box", 1883, 120, callback);

String myName = "";

#define SAMPLE_TIME_MS 30000

struct Environment {
    double temperature;
    double humidity;
    double dewPoint;
    double dewPointSlow;
    double dust_ratio;
    double dust_concentration;
};

bool scheduled = false;
int scheduled_delay = DEFAULT_DELAY;
time_t last_measurement_timestamp = 0;
Environment last_measurement = {};

void measure_air_quality(Environment& env) {
    const int starttime = millis();
    int lowpulseoccupancy = 0;
    while ((millis()-starttime) < SAMPLE_TIME_MS) {
        int duration = pulseIn(PPD_PIN, LOW);
        lowpulseoccupancy += duration;
    }
    
    env.dust_ratio = lowpulseoccupancy / (SAMPLE_TIME_MS * 10.0);
    env.dust_concentration = 1.1 * pow(env.dust_ratio, 3) - 3.8 * pow(env.dust_ratio, 2) + 520 * env.dust_ratio + 0.62;
}

const Environment measure() {
    int result;
    while ((result = DHT.acquireAndWait()) != DHTLIB_OK) {
        Serial.println("not ready to read yet, waiting...");
        delay(500);
    }

    Environment env = {};
    env.temperature = roundReading( DHT.getCelsius() );
    env.humidity = roundReading( DHT.getHumidity() );
    env.dewPoint = roundReading( DHT.getDewPoint() );
    env.dewPointSlow = roundReading( DHT.getDewPointSlow() );

    measure_air_quality(env);

    return env;
}

double roundReading(double reading) {
    return (floor(reading * 100.0)) / 100.0;
}

void publish(const Environment& env) {
    client.publish(myName + "/temperature", String(env.temperature));
    client.publish(myName + "/humidity", String(env.humidity));
    client.publish(myName + "/dewPoint", String(env.dewPoint));
    client.publish(myName + "/dewPointSlow", String(env.dewPointSlow));
    client.publish(myName + "/dustRatio", String(env.dust_ratio));
    client.publish(myName + "/dustConcentration", String(env.dust_concentration));

    String json_condition = "{ \"temperature\": " + String(env.temperature)
                          + ", \"humidity\": " + String(env.humidity)
                          + ", \"dewPoint\":" + String(env.dewPoint)
                          + ", \"dewPointSlow\":" + String(env.dewPointSlow)
                          + ", \"dustRation\":" + String(env.dust_ratio)
                          + ", \"dustConcentration\":" + String(env.dust_concentration)
                          + " }";
    Particle.publish("/" + myName + "/json_condition", json_condition, 60, PRIVATE);

    Serial.println(env.temperature, 2);
    Serial.println(env.humidity, 2);
    Serial.println(env.dewPoint, 2);
    Serial.println(env.dewPointSlow, 2);
}

void do_all() {
    if (abs(millis() - last_measurement_timestamp) > COOLING) {
        const Environment env = measure();
        publish(env);
        last_measurement = env;
        last_measurement_timestamp = millis();
    }
    else {
        publish(last_measurement);
    }
}

int func_measure(String args) {
    do_all();
    return 0;
}

int func_schedule(String args) {
    int seconds = args.toInt();
    return schedule(seconds) ? 0 : 1;
}

bool schedule(int seconds) {
    if (seconds > 0) {
        scheduled_delay = 1000 * (seconds < 30 ? 30 : seconds); // mininum of 30 secs
        scheduled = true;
        Particle.publish("/" + myName + "/debug", "enabled scheduling", 60, PRIVATE);
    }
    else {
        scheduled = false;
        scheduled_delay = DEFAULT_DELAY;
        Particle.publish("/" + myName + "/debug", "disabled scheduling", 60, PRIVATE);
    }
    return scheduled;
}

void callback(char* topic, byte* payload, unsigned int length) {
    char p[length + 1];
    memcpy(p, payload, length);
    p[length] = NULL;

    if (strcmp(topic, myName + "/schedule") == 0) {
        int seconds = String(p).toInt();
        schedule(seconds);
    }
    else if (strcmp(topic, myName + "/measure") == 0) {
        do_all();
    }
    delay(500);
}

void work() {
    if (scheduled) {
        do_all();
    }
}


void setup() {
    Serial.begin(9600);

    Particle.variable("temperature", last_measurement.temperature);
    Particle.variable("humidity", last_measurement.humidity);
    Particle.variable("dewPoint", last_measurement.dewPoint);

    Particle.function("measure", func_measure);
    Particle.function("schedule", func_schedule);

    RGB.control(true);
    RGB.brightness(0);

    Particle.subscribe("spark/device/name", nameHandler);
    Particle.publish("spark/device/name");

    maintain_connections();
}

void maintain_connections() {
    while (!WiFi.ready() && !WiFi.connecting()) {
        delay(500);
    }

    if (myName.length() > 0) {
        if (!client.isConnected())
        {
            Particle.publish("/" + myName + "/mqtt", "connecting to MQTT server", 60, PRIVATE);
            client.connect(System.deviceID());
            client.addQosCallback(qoscallback);
            client.subscribe(myName + "/measure");
            client.subscribe(myName + "/schedule");
        }
    }

    syncTime();
}

void syncTime() {
    unsigned long lastTimeSync = Particle.timeSyncedLast();
    if (abs(millis() - lastTimeSync) > ONE_DAY_MILLIS) {
        Particle.syncTime();
    }
}


void loop() {
    maintain_connections();

    if (client.isConnected())
        client.loop();
        
    work();

    delay(scheduled_delay);
}

void nameHandler(const char *topic, const char *data) {
    myName = String(data);
    Particle.publish("my name is:", myName);
}

void dht_wrapper() {
    DHT.isrCallback();
}

void qoscallback(unsigned int messageid) {
    Serial.print("Ack Message Id:");
    Serial.println(messageid);

    if (messageid == qos2messageid) {
        Serial.println("Release QoS2 Message");
        client.publishRelease(qos2messageid);
    }
}
