/*
 *      
 * Modified from Simple Stepper Motor Control Example Code by Dejan Nedelkovski, www.HowToMechatronics.com
 *  
 */
// defines pins numbers
const int stepPin = 3; 
const int dirPin = 4; 
const float rpm = 45;
const int microstepping = 16;
const int steps_per_rot = 200;
const int rotation_time_ms = 1000;

const int disable_stepper_ms = 3000;
int step_delay_time;
unsigned long start_time = 0;
unsigned long myTime;
int datapin = 12; 
int enablepin = 11; 

void setup() {
  // Sets the two pins as Outputs
  pinMode(stepPin,OUTPUT); 
  pinMode(dirPin,OUTPUT);
  pinMode(enablepin,OUTPUT);
  step_delay_time = 60*1000000.0/(steps_per_rot*rpm*microstepping);
  digitalWrite(dirPin,HIGH); // Enables the motor to move in a particular direction

  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(datapin, INPUT); 
}
void loop() {
  digitalWrite(enablepin,LOW);
  int val = digitalRead(datapin);  
  if (val == HIGH){
    start_time = myTime;
  }
  myTime = millis();
   if (myTime - disable_stepper_ms < start_time) {
    digitalWrite(enablepin,LOW);
    }
    else {digitalWrite(enablepin,HIGH);}
  if (myTime - rotation_time_ms < start_time) {
      digitalWrite(stepPin,HIGH);
      delayMicroseconds(step_delay_time);
      digitalWrite(stepPin,LOW);
      delayMicroseconds(step_delay_time);
      digitalWrite(LED_BUILTIN, HIGH); 
  } else {
      digitalWrite(LED_BUILTIN, LOW); 
  }
}
