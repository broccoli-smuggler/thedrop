#include <Arduino.h>
#include <DacESP32.h>
#include <ezButton.h>

// put function declarations here:
void update_ripems();
float revsPerMinute();
void RPMLED(float);

// RPM switch
const int input_RPM_pin = 14;
ezButton rpm_button(input_RPM_pin);

const int idle_time = 2500;
unsigned long last_falling_edge_time;
bool was_pressed = false;
float rpm = 0;
float last_rpm = 0.0;

// RPM light
const int output_RPM_led = 15;
unsigned long last_led_time;

// DAC
DacESP32 dac1(GPIO_NUM_25),
         dac2(GPIO_NUM_26);

float RPM;

// RPM interupt
void IRAM_ATTR isr() {

}

void setup() {
  // put your setup code here, to run once:
	Serial.begin(115200);
	pinMode(input_RPM_pin, INPUT_PULLUP);
  pinMode(output_RPM_led, OUTPUT);

	// attachInterrupt(input_RPM_pin, isr, RISING);
  rpm_button.setDebounceTime(1);
  rpm_button.resetCount();

  last_falling_edge_time = millis();
  last_led_time = millis();
  digitalWrite(output_RPM_led, HIGH);
  RPM = 0;

  dac1.outputCW(200);
  dac2.outputCW(200);
}

void loop() {
  update_ripems();

  if (! fabs(rpm - last_rpm) < 0.00001){
    Serial.printf("RPM: %f\n",rpm);
    last_rpm = rpm;
  }
  unsigned int sensor = analogRead(GPIO_NUM_36);
  if (sensor < 1700 or sensor  > 2000)
      Serial.printf("HB %u\n",sensor);
}

void update_ripems() {
  unsigned long revtime;
  unsigned long now = millis();
  rpm_button.loop(); // MUST call the loop() function first

  // if there is no button press in the idle time assume it is not rotating
  if (now - last_falling_edge_time > idle_time) {
    rpm = 0.0;
  }

  if (rpm_button.isPressed() && ! was_pressed) {
    was_pressed = true;
    digitalWrite(output_RPM_led, HIGH);
  }

  // on down edge.
  if (rpm_button.isReleased() && was_pressed){
    // Serial.printf("Off at %lu ms\n",now);
    revtime = now - last_falling_edge_time;
    // Serial.printf("revtime=%lu ms\n",revtime);
    rpm = 60000.0 / float(revtime) ;
    was_pressed = false;
    last_falling_edge_time = now;
    digitalWrite(output_RPM_led, LOW);
  }
}

// void RPMLED(float RPM) {
//   if (RPM == 0) return;

//   // If it's been long enough, turn off the LED
//   if (millis() - last_led_time > switch_diff / 2.0) {
//     digitalWrite(output_RPM_led, LOW);
//   }
// }