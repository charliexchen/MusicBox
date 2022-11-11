

#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>
#include <Adafruit_SSD1306.h>

#define LED_BUILTIN  2

uint16_t servo_reset[30] = {1793, 855, 1829, 1061, 1774, 1040, 1761, 1036, 1733, 988, 1681, 1100, 1798, 1015, 1713, 1106, 1759, 907, 1758, 1013, 1775, 972, 1738, 1107, 1848, 1114, 1795, 1068, 1617, 1091};
uint16_t servo_idle[30] = {1464, 1273, 1479, 1374, 1472, 1342, 1436, 1383, 1434, 1351, 1374, 1429, 1496, 1342, 1448, 1399, 1456, 1248, 1500, 1306, 1464, 1329, 1456, 1397, 1504, 1411, 1512, 1360, 1363, 1393};
uint16_t servo_play[30] = {1214, 1488, 1269, 1587, 1214, 1584, 1184, 1619, 1177, 1564, 1145, 1690, 1260, 1555, 1230, 1646, 1294, 1452, 1264, 1537, 1262, 1500, 1230, 1599, 1301, 1603, 1292, 1562, 1138, 1569};

uint8_t servo_mapping_rev[30] = {26, 22, 18, 14, 10, 6, 2, 28, 24, 20, 16, 12, 8, 4, 0, 27, 23, 19, 15, 11, 7, 3, 29, 25, 21, 17, 13, 9, 5, 1};
uint8_t servo_mapping[30] = {14, 29, 6, 21, 13, 28, 5, 20, 12, 27, 4, 19, 11, 26, 3, 18, 10, 25, 2, 17, 9, 24, 1, 16, 8, 23, 0, 15, 7, 22};

uint8_t reset_counter[30];
int8_t full_note_counter[30];
uint8_t note_on[30];

// params for playing notes
uint8_t reset_count_max = 5; //5
uint8_t relay_pin = 25;
uint8_t full_note_sustain = 1; //3

// pins for the RGB LED
uint8_t r = 15;
uint8_t g = 26;
uint8_t b = 4;
float hue = 0.4;


// pins for the shift register LEDs
uint8_t latchPin = 13;
uint8_t clockPin = 19;
uint8_t dataPin = 23;
float noteBarCount = 0;
float noteBarDecay = 0.93;


// pins and params for the OLED displays
#define OLED_RESET     4
uint8_t displayScl = 5;
uint8_t displaySda = 18;
#define SCREEN_ADDRESS_LEFT 0x3D
#define SCREEN_ADDRESS_RIGHT 0x3C
TwoWire displayWire = TwoWire(0);

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
Adafruit_SSD1306 displayLeft(SCREEN_WIDTH, SCREEN_HEIGHT, &displayWire, OLED_RESET);
Adafruit_SSD1306 displayRight(SCREEN_WIDTH, SCREEN_HEIGHT, &displayWire, OLED_RESET);
float displayBars[30];
float displayBarsDecay = 0.93;
float d = 0.2;

// pins and params for controlling the stepper motor
QueueHandle_t stepperQueue;
const int stepPin = 33;
const int dirPin = 32;
const float rpm = 50;
const int microstepping = 32;
const int steps_per_rot = 200;
const int rotation_time_ms = 1000;
const int disable_stepper_ms = 3000;
int step_delay_time;
unsigned long start_time = 0;
unsigned long myTime;
int enablepin = 12;
int stepper_mode_pin_1 = 14;
int stepper_mode_pin_2 =  27;
const int16_t disable_power_frames = 100 * 20;
int16_t disable_power_count;
bool shut_down = false;
bool turn_stepper = false;
hw_timer_t *stepper_timer = NULL; // We use a hardware timer interrupt in order to ensure consistent timings.

// pins and params for the ServoControllers
#define USMIN  600 // This is the rounded 'minimum' microsecond length based on the minimum pulse of 150
#define USMAX  2300 // This is the rounded 'maximum' microsecond length based on the maximum pulse of 600
#define SERVO_FREQ 50 // Analog servos run at ~50 Hz updates
// Servo i2c pins
#define servoSda  22
#define servoScl 21
TwoWire servoWire = TwoWire(1);

// our servo # counter
Adafruit_PWMServoDriver pwm[2];
uint8_t servo_num = 30;
uint8_t incomingByte = 0;
uint16_t servo_pos[30];
uint16_t short_pulse = 1000;
uint16_t long_pulse = 1800;
uint16_t servo_speed = 200;



unsigned int send_data = 0; // 32 bit int on esp32


void IRAM_ATTR tick_stepper() {
  if (turn_stepper) {
    digitalWrite(stepPin, !digitalRead(stepPin));
  }
}

void setup() {

  step_delay_time = 60 * 1000000.0 / (steps_per_rot * rpm * microstepping);
  stepper_timer = timerBegin(0, 80, true);
  timerAttachInterrupt(stepper_timer, &tick_stepper, true); // attaches to core 1
  timerAlarmWrite(stepper_timer, step_delay_time, true);
  timerAlarmEnable(stepper_timer); //Just Enable
  servoWire.begin(servoSda, servoScl);
  pwm[0] = Adafruit_PWMServoDriver(0x40, servoWire);
  pwm[1] = Adafruit_PWMServoDriver(0x41, servoWire);
  displayWire.begin(displaySda, displayScl);
  displayLeft.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS_LEFT);
  displayRight.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS_RIGHT);
  displayLeft.clearDisplay();
  displayLeft.display();
  displayRight.clearDisplay();
  displayRight.display();


  pinMode(r, OUTPUT);
  pinMode(g, OUTPUT);
  pinMode(b, OUTPUT);
  analogWrite(r, 255);
  analogWrite(g, 0);
  analogWrite(b, 0);
  pinMode(latchPin, OUTPUT);
  pinMode(clockPin, OUTPUT);
  pinMode(dataPin, OUTPUT);

  digitalWrite(latchPin, LOW);
  shiftOut(dataPin, clockPin, MSBFIRST, 0);
  digitalWrite(latchPin, HIGH);

  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(relay_pin, OUTPUT);
  disable_power_count = disable_power_frames;
  Serial.begin(115200);
  Serial.println("Initalising Music Box...");
  digitalWrite(relay_pin, LOW);

  pinMode(stepper_mode_pin_1, OUTPUT);
  pinMode(stepper_mode_pin_2, OUTPUT);

  digitalWrite(stepper_mode_pin_1, HIGH);
  digitalWrite(stepper_mode_pin_2, HIGH);

  pinMode(stepPin, OUTPUT);
  pinMode(dirPin, OUTPUT);
  pinMode(enablepin, OUTPUT);
  digitalWrite(dirPin, HIGH);

  for (int i = 0 ; i < 2; i++) {
    pwm[i].begin();
    pwm[i].setOscillatorFrequency(27000000);
    pwm[i].setPWMFreq(SERVO_FREQ);
    
  }
  delay(500);
  digitalWrite(relay_pin, HIGH);
  for (int i = 0 ; i < servo_num; i++) {
    reset_counter[i] = 0;
    full_note_counter[i] = -1;
    note_on[i] = false;
  }
  // Send all servos to the "reset" position
  for (int i = 0 ; i < servo_num; i++) {
    setServoPosByID(i, servo_reset[i]);
    delay(10);
  }

  for (int i = 0; i < steps_per_rot * microstepping / 8; i++) {
    digitalWrite(stepPin, HIGH);
    delayMicroseconds(step_delay_time);
    digitalWrite(stepPin, LOW);
    delayMicroseconds(step_delay_time);
  }
  // Send all servos to the "idle" position
  for (int i = 0 ; i < servo_num; i++) {
    servo_pos[i] = servo_idle[i];
    setServoPosByID(i, servo_pos[i]);
    delay(10);
  }
  Serial.println("Startup Complete!");

  stepperQueue = xQueueCreate(32, sizeof(int));
  // We care about latency, so we attach the servo controller code to a dedicated core 0
  xTaskCreatePinnedToCore(serial_servos_i2c, "servo_task", 10000, NULL, 1, NULL,  0);
}

void setServoPosByID(uint8_t id, uint16_t microsec) {
  uint8_t pwm_id = (id - id % 16) / 16;
  pwm[pwm_id].writeMicroseconds(id % 16, microsec);
}

void serial_servos_i2c( void * pvParameters ) {
  for (;;) {
    if (shut_down) return;
    send_data = 0;
    if (Serial.available() > 0) {
      while (Serial.available() > 0) {
        incomingByte = Serial.read();
        disable_power_count = disable_power_frames;
        digitalWrite(relay_pin, HIGH);
        if (incomingByte >= 97) {
          if (servo_pos[incomingByte - 97] != servo_reset[incomingByte - 97]) {
            note_on[incomingByte - 97] = true;
            full_note_counter[incomingByte - 97] = full_note_sustain;
            bitSet(send_data, incomingByte - 97);
          }
        }
        else {
          note_on[incomingByte - 65] = false;
          if (send_data % 2 == 0) {
            bitSet(send_data, 31);
          }
        }
      }
    }
    for (int i = 0 ; i < servo_num; i++) {
      if (servo_pos[i] == servo_idle[i] &&  note_on[i] == false && full_note_counter[i] < 0) {
        setServoPosByID(i, servo_pos[i]);
        continue;
      }
      if (full_note_counter[i] >= 0 || note_on[i]) {
        if (full_note_counter[i] >= 0)
        {
          full_note_counter[i] -= 1;
        }
        servo_pos[i] = servo_play[i];
      }
      else {
        servo_pos[i] = servo_reset[i];
        reset_counter[i] += 1;
        if (reset_counter[i] == reset_count_max) {
          servo_pos[i] = servo_idle[i];
          reset_counter[i] = 0;
        }
      }
      setServoPosByID(i, servo_pos[i]);
    }
    if (send_data > 0) {
      digitalWrite(LED_BUILTIN, HIGH);
      int k = send_data;
      xQueueSend(stepperQueue, &k, 0);
    }
    else {
      digitalWrite(LED_BUILTIN, LOW);


    }

    if (disable_power_count == 0) {
      digitalWrite(relay_pin, LOW);
      analogWrite(r, 0);
      analogWrite(g, 0);
      analogWrite(b, 0);
      disable_power_count--;
    }
    else if (disable_power_count > 0) {
      disable_power_count--;
    }

  }
}
float fract(float x) {
  return x - int(x);
}

void updateDisplays() {
  displayLeft.clearDisplay();
  displayRight.clearDisplay();
  for (int16_t i = 0; i < 15; i ++) {
    displayBars[i + 15] = displayBars[i + 15] * displayBarsDecay;
    displayBars[i] = displayBars[i] * 0.9;
    int level_left = int(displayBars[i]) + 1;
    int level_right = int(displayBars[i + 15]) + 1;
    displayLeft.fillRect(8 * i + 9, 63 - level_left, 7, level_left, SSD1306_WHITE);
    displayRight.fillRect(8 * i, 63 - level_right, 7, level_right, SSD1306_WHITE);
  }
  displayLeft.display(); // Update screen with each newly-drawn rectangle
  displayRight.display(); // Update screen with each newly-drawn rectangle
}

void updateDisplaysDiffuse() {
  displayLeft.clearDisplay();
  displayRight.clearDisplay();
  float newDisplayBars[30];
  newDisplayBars[0] = displayBars[0] + d * ((-2 * displayBars[0] + displayBars[1]));
  for (int16_t i = 1; i < 29; i ++) {
    newDisplayBars[i] = displayBars[i] + d * ((-2 * displayBars[i] + displayBars[i - 1] + displayBars[i + 1]));
  }
  newDisplayBars[29] = displayBars[29] + d * ((-2 * displayBars[29] + displayBars[28]));
  for (int16_t i = 0; i < 15; i ++) {
    displayBars[i + 15] = newDisplayBars[i+15]*displayBarsDecay;
    displayBars[i] = newDisplayBars[i]*displayBarsDecay;
    int level_left = int(displayBars[i]) + 1;
    int level_right = int(displayBars[i + 15]) + 1;
    displayLeft.fillRect(8 * i + 9, 63 - level_left, 7, level_left, SSD1306_WHITE);
    displayRight.fillRect(8 * i, 63 - level_right, 7, level_right, SSD1306_WHITE);
  }
  displayLeft.display(); // Update screen with each newly-drawn rectangle
  displayRight.display(); // Update screen with each newly-drawn rectangle
}

void updateDisplaysWave() {
  displayLeft.clearDisplay();
  displayRight.clearDisplay();
  float newDisplayBars[30];
  newDisplayBars[0] = displayBars[0] + d * ((-2 * displayBars[0] + displayBars[1]));
  for (int16_t i = 1; i < 29; i ++) {
    newDisplayBars[i] = displayBars[i] + d * ((-2 * displayBars[i] + displayBars[i - 1] + displayBars[i + 1]));
  }
  newDisplayBars[29] = displayBars[29] + d * ((-2 * displayBars[29] + displayBars[28]));
  for (int16_t i = 0; i < 15; i ++) {
    displayBars[i + 15] = newDisplayBars[i+15]*displayBarsDecay;
    displayBars[i] = newDisplayBars[i]*displayBarsDecay;
    int level_left = int(displayBars[i]) + 1;
    int level_right = int(displayBars[i + 15]) + 1;
    displayLeft.fillRect(8 * i + 9, 63 - level_left, 7, level_left, SSD1306_WHITE);
    displayRight.fillRect(8 * i, 63 - level_right, 7, level_right, SSD1306_WHITE);
  }
  displayLeft.display(); // Update screen with each newly-drawn rectangle
  displayRight.display(); // Update screen with each newly-drawn rectangle
}

void loop() {
  if (shut_down) return;
  unsigned int note_data = 0;
  xQueueReceive(stepperQueue, &note_data, 1);
  if (note_data > 0) {
    for (int i = 0; i < 30; i++) {
      if (bitRead(note_data, i) == 1) {
        displayBars[servo_mapping[i]] = 64;
        noteBarCount += 1;
      }
    }
    start_time = myTime;
    if (noteBarCount > 9) {
      noteBarCount = 9;
    }
    digitalWrite(latchPin, LOW);
    shiftOut(dataPin, clockPin, MSBFIRST, pow(2, min(int(round(noteBarCount)), 8)) - 1);
    digitalWrite(latchPin, HIGH);
  }
  noteBarCount = noteBarCount * noteBarDecay;
  digitalWrite(latchPin, LOW);
  shiftOut(dataPin, clockPin, MSBFIRST, pow(2, min(int(round(noteBarCount)), 8)) - 1);
  digitalWrite(latchPin, HIGH);
  updateDisplaysDiffuse();
  myTime = millis();
  if (myTime - rotation_time_ms < start_time) {
    turn_stepper = true;
    hue += 0.02;
    if (hue > 1.0) {
      hue = 0;
    }
    analogWrite(r, 128 * constrain(abs(fract(hue + 1.0) * 6.0 - 3.0) - 1.0, 0.0, 1.0));
    analogWrite(g, 128 * constrain(abs(fract(hue + 0.66666) * 6.0 - 3.0) - 1.0, 0.0, 1.0));
    analogWrite(b, 128 * constrain(abs(fract(hue + 0.33333) * 6.0 - 3.0) - 1.0, 0.0, 1.0));
  }
  else {
    turn_stepper = false;
  }
}
