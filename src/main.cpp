#include <Arduino.h>
#include <Audio.h>
#include <DacESP32.h>
#include <FS.h>
#include <SPIFFS.h>
#include <ezButton.h>
#define FORMAT_SPIFFS_IF_FAILED true
#define I2S_DOUT 25
#define I2S_BCLK 27
#define I2S_LRC 26
// put function declarations here:

void update_flywheel_ripems();
float revsPerMinute();
void RPMLED(float);

Audio audio;

const int idle_time = 2500;
const int sleep_timer = 10000;  // time to go to sleep after this timer expires
const int input_ir_sensor_pin = 14;
const gpio_num_t magnetic_switch = GPIO_NUM_2;

// using the IR sensor on flywheel
unsigned long flywheel_falling_edge_time;
bool flywheel_seen = false;
float flywheel_rpm = 0.0;
float last_flywheel_rpm = 0.0;
int last_flywheel, flywheel = 0;

unsigned int sensor_prev = 0;

// RPM light
const int output_RPM_led = 2;
unsigned long last_led_time;

unsigned long action_seen = 0; 

void setup()
{
    
    // put your setup code here, to run once:
    Serial.begin(115200);
    pinMode(input_ir_sensor_pin, INPUT_PULLUP);
    pinMode(output_RPM_led, OUTPUT);
    esp_sleep_enable_ext0_wakeup(magnetic_switch,1);
    Serial.flush();                                                // Waits for the transmission of outgoing serial data to complete.
    esp_deep_sleep_start();

    last_led_time = millis();
    digitalWrite(output_RPM_led, HIGH);
}

void loop()
{
    unsigned long now = millis();
    update_flywheel_ripems(now);
    audio.loop();
    if (!fabs(flywheel_rpm - last_flywheel_rpm) < 0.00001)
        Serial.printf("Flywheel RPM: %f \n", last_flywheel_rpm = flywheel_rpm);
    if(flywheel_rpm > 0.0)
        action_seen = now;
    if(now - action_seen > sleep_timer){
        Serial.flush();                                                // Waits for the transmission of outgoing serial data to complete.
        esp_deep_sleep_start();
    }
}


void update_flywheel_ripems(unsigned long now )
{
    unsigned long revtime;
    int flywheel = digitalRead(input_ir_sensor_pin);

    if (now - flywheel_falling_edge_time > idle_time)
    {
        flywheel_rpm = 0.0;
    }
    if (flywheel && !flywheel_seen)
    {
        flywheel_seen = true;
    }
    // on down edge.
    if (!flywheel && flywheel_seen)
    {
        revtime = now - flywheel_falling_edge_time;
        // Serial.printf("revtime=%lu ms\n",revtime);
        flywheel_rpm = 60000.0 / float(revtime);
        flywheel_seen = false;
        flywheel_falling_edge_time = now;
    }
}

