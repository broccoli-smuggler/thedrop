#include <Arduino.h>
#include <Audio.h>
#include <FS.h>
#include <SD.h>
#include <SPI.h>
#include <SPIFFS.h>
#include <ezButton.h>
#include <SimpleKalmanFilter.h>

// Our files
#include "sd_card.h"

#define FORMAT_SPIFFS_IF_FAILED true
#define I2S_DOUT 14
#define I2S_BCLK 25
#define I2S_LRC 26

// put function declarations here:
void interupt_update_flywheel_ripems();
void calc_flywheel_rpms();
void update_leds();
void update_trigger_switch();
void update_audio();
void update_audio_speed();
void update_audio_volume();
void test_pulse_rpms();

// Serial
bool debug = false;

// test rpm
long max_rpm = 2500;
int diff = 50;
unsigned long last_pulse_test;
bool test_pulse = false;

// Audio
bool update_on_loop = false;
bool update_on_pulse = false;

Audio audio;
bool audio_playing = false;
unsigned long last_speed_adjustment;
const char *audio_sample = "/drop_reso.wav";
const char *audio_sample_2 = "/drop_punk_met.wav";
const char *current_audio_sample = audio_sample;
const int audio_loop_timer = 1000;
unsigned long next_loop_update;
unsigned long audio_start_time = 0;


// RPM calulation via ir sensor
SimpleKalmanFilter rpm_filter(0.1, 1, 0.1);
const int idle_time = 2700;
const int sleep_timer = 10000; // time to go to sleep after this timer expires
const int input_ir_sensor_pin = 13;

const gpio_num_t magnetic_switch = GPIO_NUM_5;
unsigned long flywheel_falling_edge_time, prev_flywheel_falling_edge_time;
bool flywheel_seen = false;
float flywheel_rpm = 0.0;
float last_flywheel_rpm = 0.0;
int last_flywheel, flywheel = 0;

// Trigger switch
const int output_trigger_pin = 15;
float trigger_switch_rpm = 700.0; // RPM to trigger the switch
bool triggered = false;
int triggered_count = 0;

// RPM lights
unsigned int pwm_leds = 4;
unsigned long pulse_length = 3000;       // Timer duration for each pulse in seconds
unsigned long last_pulse = 0;
int last_intensity = 255;
bool up = true; // If the lights are going up or down

void setup()
{
    // put your setup code here, to run once:
    Serial.begin(115200);
    pinMode(input_ir_sensor_pin, INPUT_PULLUP);
    attachInterrupt(input_ir_sensor_pin, interupt_update_flywheel_ripems, FALLING);
    pinMode(pwm_leds, OUTPUT);
    pinMode(output_trigger_pin, OUTPUT);
    digitalWrite(output_trigger_pin, LOW);
    analogWrite(pwm_leds, last_intensity);

    setup_sd();

    unsigned long now = millis();
    last_pulse_test = now;
    next_loop_update = now;

    if (!SPIFFS.begin(FORMAT_SPIFFS_IF_FAILED))
    {
        Serial.println("SPIFFS Mount Failed");
        return;
    }

    audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
}

void loop()
{
    test_pulse_rpms();

    calc_flywheel_rpms();
    update_leds();
    update_trigger_switch();
    update_audio();
}

void calc_flywheel_rpms()
{
    if (debug && !fabs(flywheel_rpm - last_flywheel_rpm) < 0.00001)
        Serial.printf("Flywheel RPM: %f \n", last_flywheel_rpm = flywheel_rpm);

    if (millis() - prev_flywheel_falling_edge_time > 1800 && !test_pulse)
        flywheel_rpm = 0.0;

    if (flywheel_falling_edge_time > prev_flywheel_falling_edge_time)
    {   
        float new_rpm = 60000.0 / (flywheel_falling_edge_time - prev_flywheel_falling_edge_time);
        if (new_rpm < max_rpm * 1.1)
        {
            flywheel_rpm = rpm_filter.updateEstimate(new_rpm);
            prev_flywheel_falling_edge_time = flywheel_falling_edge_time;
        }
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
        if (flywheel_rpm > trigger_switch_rpm * 1.7)
        {
            current_audio_sample = audio_sample_2;
        } else 
        {
            current_audio_sample = audio_sample;
        }

        // On fast enough and ending
        Serial.println(current_audio_sample);

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

    last_speed_adjustment = now;

    if (triggered)
    {
        if (!audio_playing)
        {
            audio.connecttoFS(SD, current_audio_sample);
            audio.setFileLoop(true);
            audio_playing = true;
            audio_start_time = now;
            Serial.println("Playing");
        }
        audio.loop();
        update_audio_volume();
    }
    else if (!triggered && audio_playing)
    {
        Serial.println("Stopped");
        digitalWrite(output_trigger_pin, LOW);
        current_audio_sample = audio_sample;
        audio_playing = false;
        audio_start_time = 0;
    }
}

void update_audio_volume()
{
    unsigned long now = millis();
    long result_volume = map(flywheel_rpm, 0, max_rpm, 1, 18);
    audio.setVolume(uint8_t(result_volume));
}

void update_audio_speed()
{
    static long min_speed = 0.8 * 100;
    static long max_speed = 1.3 * 100;
    long result_speed = map(flywheel_rpm, 0, 1500, min_speed, max_speed);
    audio.audioFileSeek(float(result_speed / 100.0));
}

void update_trigger_switch()
{
    unsigned long now = millis();

    if (flywheel_rpm > trigger_switch_rpm)
    {   
        // Sometimes we get a false fast reading
        if (triggered_count <= 3)
        {
            triggered_count++;
        } 
        else 
        {
            triggered = true;
            digitalWrite(output_trigger_pin, HIGH);
        }
    }
    else if (flywheel_rpm <= trigger_switch_rpm * 0.2)
    {
        digitalWrite(output_trigger_pin, LOW);
        triggered = false;
        triggered_count = 0;
    }
}

void update_leds()
{
    long intensity;

    if (triggered)
    {   
        intensity = map(flywheel_rpm, 0, max_rpm, 20, 255);
    } 
    else
    {
        unsigned long now = millis();
        unsigned long time_since_last_pulse = now - last_pulse;
        if (time_since_last_pulse > pulse_length)
        {
            last_pulse = now;
            up = !up; // Toggle the direction of the pulse
        }

        // Set the lights based on how far through the cycle we are
        if (up)
            intensity = map(time_since_last_pulse, 0, pulse_length, 0, 255);
        else
            intensity = map(time_since_last_pulse, 0, pulse_length, 255, 0);
    }
    
    // We can't write the same value over and over
    if (int(intensity) != last_intensity)
    {
        analogWrite(pwm_leds, intensity);
        last_intensity = intensity;
    }
}

void test_pulse_rpms()
{
    unsigned long now = millis();
    if (test_pulse && now - last_pulse_test > 1000)
    {
        if (flywheel_rpm > max_rpm)
            diff = -50;
        if (flywheel_rpm <= 0)
            diff = 50;

        flywheel_rpm += diff;
        last_pulse_test = now;
        if (update_on_pulse)
        {
            audio.setFilePos(0);
            update_audio_speed();
        }
    }
}

void interupt_update_flywheel_ripems()
{
    flywheel_falling_edge_time = millis();
}
