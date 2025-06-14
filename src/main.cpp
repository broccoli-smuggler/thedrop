#include <Arduino.h>
#include <Audio.h>
#include <FS.h>
#include <SPIFFS.h>
#include <ezButton.h>

#define FORMAT_SPIFFS_IF_FAILED true
#define I2S_DOUT 14
#define I2S_BCLK 25
#define I2S_LRC 26

// put function declarations here:
void update_ripems();
void update_pulse_ripems();
void update_audio();
void update_audio_speed();

// Audio
Audio audio;
unsigned long last_speed_adjustment;
const char* audio_sample = "heartbeat1.wav";
long max_rpm = 600;
int diff = 10;
unsigned long last_pulse_test;
bool update_on_loop = true;
bool update_on_pulse = false;
bool test_pulse = false;

// RPM switch
const int input_RPM_pin = 14;
const int pulse_pin = 4;
const unsigned int threshold = 100;
ezButton rpm_button(input_RPM_pin);

const int idle_time = 2500;

unsigned long last_falling_edge_time;
bool was_pressed = false;
float rpm = 0;
float last_rpm = 0.0;

// using the IR sensor on flywheel
unsigned long pulse_falling_edge_time;
bool pulse_seen = false;
float pulse_rpm = 0.0;
float last_pulse_rpm = 0.0;

unsigned int sensor_prev = 0;

// RPM light
const int output_RPM_led = 2;

int last_pulse, pulse = 0;

void listDir(fs::FS &fs, const char *dirname, uint8_t levels)
{

    Serial.printf("Listing directory: %s\r\n", dirname);

    File root = fs.open(dirname);

    if (!root)
    {

        Serial.println("- failed to open directory");

        return;
    }

    if (!root.isDirectory())
    {

        Serial.println(" - not a directory");

        return;
    }

    File file = root.openNextFile();

    while (file)
    {
        if (file.isDirectory())
        {
            Serial.print("  DIR : ");
            Serial.println(file.name());
            if (levels)
            {
                listDir(fs, file.path(), levels - 1);
            }
        }
        else
        {
            Serial.print("  FILE: ");
            Serial.print(file.name());
            Serial.print("\tSIZE: ");
            Serial.println(file.size());
        }
        file = root.openNextFile();
    }
}

void setup()
{
    // put your setup code here, to run once:
    Serial.begin(115200);
    pinMode(input_RPM_pin, INPUT_PULLUP);
    pinMode(pulse_pin, INPUT_PULLUP);
    pinMode(output_RPM_led, OUTPUT);

    rpm_button.setDebounceTime(1);
    rpm_button.resetCount();

    last_falling_edge_time = millis();
    last_pulse_test = millis();
    digitalWrite(output_RPM_led, HIGH);

    if (!SPIFFS.begin(FORMAT_SPIFFS_IF_FAILED))
    {
        Serial.println("SPIFFS Mount Failed");
        return;
    }

    listDir(SPIFFS, "/", 0);

    audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
    audio.connecttoFS(SPIFFS, audio_sample);
    audio.setFileLoop(true);
}

void loop()
{
    update_ripems();
    update_pulse_ripems();
    unsigned long now = millis();

    if (test_pulse && now - last_pulse_test > 1000)
    {
      if (pulse_rpm > max_rpm)
        diff = -10;
      if (pulse_rpm <= 0)
        diff = 10;
      
      pulse_rpm += diff;
      last_pulse_test = now;
      if (update_on_pulse)
      {
        audio.setFilePos(0);
        update_audio_speed();
      }
    }

    update_audio();

    if (!fabs(rpm - last_rpm) < 0.00001)
        Serial.printf("RPM: %f\n", last_rpm = rpm);
    if (!fabs(pulse_rpm - last_pulse_rpm) < 0.00001)
        Serial.printf("Flywheel RPM: %f (%f)\n", last_pulse_rpm = pulse_rpm, (rpm) ? pulse_rpm / rpm : 0.0);
}

void audio_info(const char* info) {
    Serial.println(info);
    if (update_on_loop)    
        update_audio_speed();
}

void update_audio()
{
    unsigned long now = millis();

    audio.loop();
    last_speed_adjustment = now;
}

void update_audio_speed()
{
    static long min_speed = 0.4*100;
    static long max_speed = 2.0*100;
    long result_speed = map(pulse_rpm, 0, 600, min_speed, max_speed);
    
    audio.audioFileSeek(float(result_speed/100.0));
}

void update_ripems()
{
    unsigned long revtime;
    unsigned long now = millis();
    rpm_button.loop(); // MUST call the loop() function first
    // if there is no button press in the idle time assume it is not rotating
    if (now - last_falling_edge_time > idle_time)
    {
        rpm = 0.0;
    }
    if (rpm_button.isPressed() && !was_pressed)
    {
        was_pressed = true;
        digitalWrite(output_RPM_led, HIGH);
    }
    // on down edge.
    if (rpm_button.isReleased() && was_pressed)
    {
        // Serial.printf("Off at %lu ms\n",now);
        revtime = now - last_falling_edge_time;
        // Serial.printf("revtime=%lu ms\n",revtime);
        rpm = 60000.0 / float(revtime);
        was_pressed = false;
        last_falling_edge_time = now;
        digitalWrite(output_RPM_led, LOW);
    }
}

void update_pulse_ripems()
{
    unsigned long revtime;
    unsigned long now = millis();
    int pulse = digitalRead(pulse_pin);

    if (now - pulse_falling_edge_time > idle_time)
    {
        pulse_rpm = 0.0;
    }
    if (pulse && !pulse_seen)
    {
        pulse_seen = true;
    }
    // on down edge.
    if (!pulse && pulse_seen)
    {
        revtime = now - pulse_falling_edge_time;
        // Serial.printf("revtime=%lu ms\n",revtime);
        pulse_rpm = 60000.0 / float(revtime);
        pulse_seen = false;
        pulse_falling_edge_time = now;
    }
}
