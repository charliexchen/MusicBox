

#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>
#include <Adafruit_SSD1306.h>
QueueHandle_t stepperQueue;

uint16_t servo_reset[30] = {1818, 898, 1843, 1033, 1754, 1052, 1763, 1003, 1759, 1001, 1719, 1093, 1838, 967, 1793, 1038, 1795, 853, 1780, 962, 1827, 979, 1786, 1070, 1884, 1061, 1820, 1008, 1687, 1052};
uint16_t servo_idle_tight[30] = {1459, 1338, 1534, 1413, 1441, 1456, 1438, 1431, 1408, 1401, 1395, 1457, 1495, 1377, 1486, 1415, 1493, 1248, 1473, 1301, 1477, 1317, 1488, 1390, 1592, 1415, 1541, 1337, 1385, 1376};
//uint16_t servo_idle[30] = {1541, 1258, 1594, 1370, 1527, 1383, 1498, 1299, 1518, 1337, 1468, 1385, 1559, 1315, 1571, 1322, 1538, 1191, 1555, 1200, 1552, 1269, 1580, 1317, 1674, 1369, 1635, 1255, 1466, 1361};
uint16_t servo_idle[30] = {1489, 1241, 1541, 1374, 1472, 1342, 1484, 1383, 1434, 1351, 1374, 1429, 1496, 1342, 1448, 1402, 1537, 1248, 1500, 1306, 1520, 1276, 1459, 1379, 1562, 1422, 1498, 1335, 1377, 1386};
//uint16_t servo_play[30] = {1322, 1500, 1321, 1537, 1283, 1564, 1239, 1548, 1271, 1509, 1191, 1635, 1328, 1507, 1265, 1585, 1349, 1397, 1315, 1456, 1299, 1461, 1321, 1564, 1397, 1568, 1370, 1466, 1226, 1521};

uint16_t servo_play[30] = {1214, 1488, 1218, 1608, 1187, 1582, 1145, 1623, 1219, 1576, 1066, 1722, 1251, 1555, 1164, 1651, 1280, 1438, 1191, 1534, 1244, 1512, 1157, 1628, 1308, 1603, 1175, 1587, 1066, 1591};

uint8_t reset_counter[30];
int8_t full_note_counter[30];
uint8_t note_on[30];

// params for playing notes
uint8_t reset_count_max = 5;
uint8_t relay_pin = 25;
uint8_t full_note_sustain = 3;

// pins for the RGB LED
uint8_t r = 15;
uint8_t g = 26;
uint8_t b = 4;

// pins for the shift register LEDs
uint8_t latchPin = 13;
uint8_t clockPin = 19;
uint8_t dataPin = 23;
int noteBarCount = 0;
int noteCooldown = 0;
int maxNoteCooldown = 250;

#define LED_BUILTIN  2

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

// pins and params for controlling the stepper motor
const int stepPin = 33;
const int dirPin = 32;
const float rpm = 45;
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
const int16_t disable_power_frames = 60 * 20;
int16_t disable_power_count;
bool shut_down = false;

// pins and params for the ServoControllers
#define USMIN  600 // This is the rounded 'minimum' microsecond length based on the minimum pulse of 150
#define USMAX  2300 // This is the rounded 'maximum' microsecond length based on the maximum pulse of 600
#define SERVO_FREQ 50 // Analog servos run at ~50 Hz updates
// Servo i2c pins
#define servoSda  22
#define servoScl 21
TwoWire servoWire = TwoWire(1);

// our servo # counter
uint8_t servo_num = 30;
uint8_t incomingByte = 0;
Adafruit_PWMServoDriver pwm[2];
uint16_t servo_pos[30];
uint16_t short_pulse = 1000;
uint16_t long_pulse = 1800;
uint16_t servo_speed = 200;

float h = 0.4;

int send_data = 0;
void setup() {
  
  delay(500);
  
  servoWire.begin(servoSda, servoScl);
  pwm[0] = Adafruit_PWMServoDriver(0x40, servoWire);
  pwm[1] = Adafruit_PWMServoDriver(0x41, servoWire);
  displayWire.begin(displaySda, displayScl);
  displayLeft.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS_LEFT);
  displayRight.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS_RIGHT);
  displayLeft.display();
  delay(2000);
  displayLeft.clearDisplay();
  displayLeft.drawPixel(10, 10, SSD1306_WHITE);
  displayLeft.display();
  displayRight.clearDisplay();
  displayRight.drawPixel(10, 10, SSD1306_WHITE);
  displayRight.display();
  delay(2000);

  
  pinMode(r, OUTPUT);
  pinMode(g, OUTPUT);
  pinMode(b, OUTPUT);

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
  step_delay_time = 60 * 1000000.0 / (steps_per_rot * rpm * microstepping);
  digitalWrite(dirPin, HIGH);

  for (int i = 0 ; i < 2; i++) {
    pwm[i].begin();
    pwm[i].setOscillatorFrequency(27000000);
    pwm[i].setPWMFreq(SERVO_FREQ);
    delay(1000);
  }
  digitalWrite(relay_pin, HIGH);
  delay(500);
  for (int i = 0 ; i < servo_num; i++) {
    reset_counter[i] = 0;
    full_note_counter[i] = -1;
    note_on[i] = false;
  }
  delay(100);
  //for (int i = 0 ; i < servo_num; i++) {
  //  setServoPosByID(i, servo_play[i]);
  //  delay(30);
  //}
  // Send all servos to the "reset" position
  for (int i = 0 ; i < servo_num; i++) {
    setServoPosByID(i, servo_reset[i]);
    delay(30);
  }

  for (int i = 0; i < steps_per_rot * microstepping / 2; i++) {
    digitalWrite(stepPin, HIGH);
    delayMicroseconds(step_delay_time);
    digitalWrite(stepPin, LOW);
    delayMicroseconds(step_delay_time);
  }
  // Send all servos to the "idle" position
  for (int i = 0 ; i < servo_num; i++) {
    servo_pos[i] = servo_idle[i];
    setServoPosByID(i, servo_pos[i]);
    delay(30);
  }
  Serial.println("Startup Complete!");

  stepperQueue = xQueueCreate(8, sizeof(int));
  xTaskCreatePinnedToCore(serial_servos_i2c, "servo_task", 10000, NULL, 1, NULL,  0);
  // xTaskCreatePinnedToCore(reset_steppers, "stepper_task", 10000, NULL, 1, NULL,  1);


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
            send_data += 2;
          }
        }
        else {
          note_on[incomingByte - 65] = false;
          if (send_data%2==0){
            send_data+=1;
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
    if (send_data) {
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
void reset_steppers( void * pvParameters ) {
  for (;;) {

  }


}
void loop() {
  if (shut_down) return;
  int enable_stepper = 0;
  xQueueReceive(stepperQueue, &enable_stepper, 0);
  if (enable_stepper > 0) {
    start_time = myTime;
    if (noteBarCount==0){noteCooldown = maxNoteCooldown;}
    noteBarCount += (enable_stepper-enable_stepper%2)/2;
    if (noteBarCount==1){noteBarCount+=1;}
    
    if (noteBarCount > 10) {
      noteBarCount = 10;
    }
    digitalWrite(latchPin, LOW);
    shiftOut(dataPin, clockPin, MSBFIRST, pow(2, min(noteBarCount,8)) - 1);
    digitalWrite(latchPin, HIGH);
  }
  myTime = millis();
  if (myTime - rotation_time_ms < start_time) {


    noteCooldown -= 1;
    if (noteCooldown == 0) {
      noteBarCount -= 1;
      if (noteBarCount < 0) {
        noteBarCount = 0;
      }
      digitalWrite(latchPin, LOW);
      shiftOut(dataPin, clockPin, MSBFIRST, pow(2, min(noteBarCount,8)) - 1);
      digitalWrite(latchPin, HIGH); 

    }
    if (noteCooldown < 0) {
      noteCooldown = maxNoteCooldown;
    }

    h += 0.0002;
    if (h > 1.0) {
      h = 0;
    }
    digitalWrite(stepPin, HIGH);
    delayMicroseconds(step_delay_time);
    digitalWrite(stepPin, LOW);
    delayMicroseconds(step_delay_time);

    analogWrite(r, 128 * constrain(abs(fract(h + 1.0) * 6.0 - 3.0) - 1.0, 0.0, 1.0));
    analogWrite(g, 128 * constrain(abs(fract(h + 0.66666) * 6.0 - 3.0) - 1.0, 0.0, 1.0));
    analogWrite(b, 128 * constrain(abs(fract(h + 0.33333) * 6.0 - 3.0) - 1.0, 0.0, 1.0));
  }
}


void testdrawrect(void) {
  displayLeft.clearDisplay();
  for(int16_t i=0; i<displayLeft.height()/2; i+=2) {
    displayLeft.drawRect(i, i, displayLeft.width()-2*i, displayLeft.height()-2*i, SSD1306_WHITE);
    displayLeft.display(); // Update screen with each newly-drawn rectangle
    delay(1);
  }
  delay(2000);
}
