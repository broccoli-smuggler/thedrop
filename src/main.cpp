#include <Arduino.h>
#include <Audio.h>
#include <FS.h>
#include <SD.h>
#include <SPI.h>
#include <SPIFFS.h>
#include <ezButton.h>

// Our files
#include "sd_card.h"

#define FORMAT_SPIFFS_IF_FAILED true
#define I2S_DOUT 14
#define I2S_BCLK 25
#define I2S_LRC 26

// put function declarations here:

void update_flywheel_ripems(unsigned long);
void interupt_update_flywheel_ripems();
void calc_flywheel_ripems();
void update_audio();
void update_audio_speed();
void update_audio_volume();
void fire();
void hold_fire();

// Fire
const int output_RPM_led = 2;

// Audio
Audio audio;
unsigned long last_speed_adjustment;
const char *audio_sample = "/got.wav";
const char *audio_sample_2 = "/got_roar.wav";
const char *current_audio_sample = audio_sample;

long max_rpm = 600;
int diff = 10;
unsigned long last_pulse_test;
bool update_on_loop = true;
bool update_on_pulse = false;
bool test_pulse = false;
const int audio_loop_timer = 1000;
unsigned long next_loop_update;


//RPM
const unsigned int threshold = 100;

const int idle_time = 2500;
const int sleep_timer = 10000; // time to go to sleep after this timer expires
const int input_ir_sensor_pin = 13;
const int output_fire_pin = 15;
const gpio_num_t magnetic_switch = GPIO_NUM_5;

// using the IR sensor on flywheel
unsigned long flywheel_falling_edge_time, prev_flywheel_falling_edge_time;
bool flywheel_seen = false;
float flywheel_rpm = 0.0;
float last_flywheel_rpm = 0.0;
int last_flywheel, flywheel = 0;

unsigned int sensor_prev = 0;

// RPM light
// const int output_fire = 2;

int last_pulse, pulse = 0;

unsigned long last_led_time;
unsigned long action_seen = 0;

unsigned long start_firing = 0;
const unsigned long fire_time = 2000; 

void setup()
{
    // put your setup code here, to run once:
    Serial.begin(115200);
    pinMode(input_ir_sensor_pin, INPUT_PULLUP);
    attachInterrupt(input_ir_sensor_pin, interupt_update_flywheel_ripems, FALLING);
    pinMode(output_RPM_led, OUTPUT);
    pinMode(output_fire_pin, OUTPUT);
    digitalWrite(output_fire_pin, LOW);

    setup_sd();

    last_pulse_test = millis();
    next_loop_update = millis();

    esp_sleep_enable_ext0_wakeup(magnetic_switch, 1);

    last_led_time = millis();
    digitalWrite(output_RPM_led, HIGH);

    if (!SPIFFS.begin(FORMAT_SPIFFS_IF_FAILED))
    {
        Serial.println("SPIFFS Mount Failed");
        return;
    }

    audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
    audio.connecttoFS(SD, current_audio_sample);
    audio.setFileLoop(true);
}

void loop()
{
    audio.loop();
    unsigned long revtime;
    unsigned long now = millis();
    // update_flywheel_ripems(now);
    calc_flywheel_ripems();
    if (!fabs(flywheel_rpm - last_flywheel_rpm) < 0.00001)
        Serial.printf("Flywheel RPM: %f \n", last_flywheel_rpm = flywheel_rpm);
    if (flywheel_rpm > 0.0)
        action_seen = now;
    // if(now - action_seen > sleep_timer){
    //     Serial.flush();  // Waits for the transmission of outgoing serial data to complete.
    //     esp_deep_sleep_start();
    // }

    if (test_pulse && now - last_pulse_test > 1000)
    {
        if (flywheel_rpm > max_rpm)
            diff = -15;
        if (flywheel_rpm <= 0)
            diff = 15;

        flywheel_rpm += diff;
        last_pulse_test = now;
        if (update_on_pulse)
        {
            audio.setFilePos(0);
            update_audio_speed();
        }
    }
    update_audio();
    hold_fire();
}

void fire()
{
    Serial.println("Firing!");
    digitalWrite(output_fire_pin, HIGH);
    start_firing = millis();
    Serial.println("Fire started!");
    Serial.println(start_firing);
}

void hold_fire()
{
    if (millis() - start_firing > fire_time && start_firing != 0)
    {
        Serial.println("Stopping fire!");
        Serial.println(millis());
        digitalWrite(output_fire_pin, LOW);
        start_firing = 0;
    }
}

void audio_info(const char *info)
{
    if (millis() > next_loop_update)
    {
        unsigned long now = millis();
        next_loop_update = now + audio_loop_timer;

        const char *prior_sample = current_audio_sample;

        // Update audio sample based on flywheel RPM
        if (flywheel_rpm > 400)
        {
            current_audio_sample = audio_sample_2;
        } else 
        {
            current_audio_sample = audio_sample;
        }

        // On fast enough and ending
        Serial.println(prior_sample);
        Serial.println(flywheel_rpm);
        if (prior_sample == audio_sample_2 && flywheel_rpm >= 400)
        {
            fire();
        }

        if (current_audio_sample != prior_sample)
        {
            audio.connecttoFS(SD, current_audio_sample);
            audio.setFileLoop(true);
        }

        if (update_on_loop)
            update_audio_speed();
    }
}

void update_audio()
{
    unsigned long now = millis();

    audio.loop();
    update_audio_volume();
    last_speed_adjustment = now;
}


void update_audio_volume()
{
    long result_volume = map(flywheel_rpm, 0, 600, 0, 21);

    audio.setVolume(uint8_t(result_volume));
}

void update_audio_speed()
{
    static long min_speed = 0.8 * 100;
    static long max_speed = 1.3 * 100;
    long result_speed = map(flywheel_rpm, 0, 600, min_speed, max_speed);
    audio.audioFileSeek(float(result_speed / 100.0));
}

void update_flywheel_ripems(unsigned long now)
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

void interupt_update_flywheel_ripems()
{
    flywheel_falling_edge_time = millis();
}

void calc_flywheel_ripems()
{
    if (flywheel_falling_edge_time > prev_flywheel_falling_edge_time)
    {
        flywheel_rpm = 60000.0 / (flywheel_falling_edge_time - prev_flywheel_falling_edge_time);
        prev_flywheel_falling_edge_time = flywheel_falling_edge_time;
    }
}