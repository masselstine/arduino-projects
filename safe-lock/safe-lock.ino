/**
 * A fun 'safe'/'lock' sketch that was used to secure a box
 * containing a clue for a Christmas Gift Hunt.
 *
 * What you need:
 *   - arduino or ESP32 or similar
 *   - servo
 *   - piezo buzzer
 *   - IR receiver
 *   - Old TV remote or other IR source
 *   - A box
 *
 * Wiring:
 *   - Buzzer, -'ve to ground, +'ve to pin 9
 *   - IR receiver, Gnd to Gnd, VCC to pin 7, Signal to pin 2
 *   - Servo, Gnd to Gnd, VCC to 5V, Signal to pin 10
 *
 * Setup:
 *   Use a program such as the one found here:
 *   https://learn.adafruit.com/ir-sensor/using-an-ir-sensor
 *   to capture the keys on your particular remote. The
 *   resulting values should be used to replace 'numstrs'
 *   which I have here.
 *
 * Find a box that will fit the conponents. Tape the servo
 * to the side of the box and use the arm of the servo to
 * hold the box closed until the proper code has been
 * entered. Ensure the IR sensor has a hole to the outside
 * of the box.
 */

#include <Servo.h>

#define IRpin_PIN      PIND
#define IRpin          2
#define IRPower        7
const int buzzer = 9; //buzzer to arduino pin 9

// the maximum pulse we'll listen for - 65 milliseconds is a long time
#define MAXPULSE 65000
 
// what our timing resolution should be, larger is better
// as its more 'precise' - but too large and you wont get
// accurate timing
#define RESOLUTION 20 
 
// we will store up to 100 pulse pairs (this is -a lot-)
uint16_t pulses[100][2];  // pair is high and low pulse 
uint8_t currentpulse = 0; // index for pulses we're storing

Servo myservo;
int servopos = 0; 

char *numstrs[] = {
"1101000100101110101010000101011111",
"1101000100101110110000000011111111",
"1101000100101110101000000101111111",
"1101000100101110111000000001111111",
"1101000100101110100100000110111111",
"1101000100101110110100000010111111",
"1101000100101110101100000100111111",
"1101000100101110111100000000111111",
"1101000100101110100010000111011111",
"1101000100101110110010000011011111"
};

int code[] = {5, 5, 6, 8, 7, 2, 8, 0};

void setup(void) {
  Serial.begin(115200);
  Serial.println("Ready to decode IR!");
  
  pinMode(buzzer, OUTPUT); // Set buzzer - pin 9 as an output
  pinMode(IRPower, OUTPUT);
  digitalWrite(IRPower, HIGH);

  myservo.attach(10);
  myservo.write(servopos);
}

void buzz(bool good)
{
  if(!good)
  {
    tone(buzzer, 250); // Send 1KHz sound signal...
    delay(300);        // ...for 1 sec
  } else {
    tone(buzzer, 1000); // Send 1KHz sound signal...
    delay(300);        // ...for 1 sec    
  }
  noTone(buzzer);     // Stop sound...  
}

void togglelock(void)
{
  Serial.println("Congratulations!");
  if(servopos == 0) {
    servopos = 180;
  } else {
    servopos = 0;
  }
  myservo.write(servopos);
}

void checkcode()
{
  int num;
  static int pos = 0;

  num = getvalue();
  if(num == -1) {
    buzz(false);
    pos = 0;
    return;
  }

  if(num == code[pos]) {
    buzz(true);
    pos++;
  } else {
    buzz(false);
    pos = 0;    
  }

  if(pos == 8) {
    pos = 0;
    togglelock();
  }
}
 
void loop(void) {
  uint16_t highpulse, lowpulse;  // temporary storage timing
  highpulse = lowpulse = 0; // start out with no pulse length
  
  
//  while (digitalRead(IRpin)) { // this is too slow!
    while (IRpin_PIN & (1 << IRpin)) {
     // pin is still HIGH
 
     // count off another few microseconds
     highpulse++;
     delayMicroseconds(RESOLUTION);
 
     // If the pulse is too long, we 'timed out' - either nothing
     // was received or the code is finished, so print what
     // we've grabbed so far, and then reset
     if ((highpulse >= MAXPULSE) && (currentpulse != 0)) {
       checkcode();
       currentpulse=0;
       return;
     }
  }
  // we didn't time out so lets stash the reading
  pulses[currentpulse][0] = highpulse;
  
  // same as above
  while (! (IRpin_PIN & _BV(IRpin))) {
     // pin is still LOW
     lowpulse++;
     delayMicroseconds(RESOLUTION);
     if ((lowpulse >= MAXPULSE)  && (currentpulse != 0)) {
       checkcode();
       currentpulse=0;
       return;
     }
  }
  pulses[currentpulse][1] = lowpulse;
 
  // we read one high-low pulse successfully, continue!
  currentpulse++;
}
 
int getvalue(void) {
  int ret = -1;
  char str[34];
  int charpos = 0;
  
  for (uint8_t i = 0; i < currentpulse && charpos < 34; i++) {
    if((pulses[i][1] * RESOLUTION) > 1000)
      continue;

    if((pulses[i][0] * RESOLUTION) > 1000) {
      str[charpos] = '1';
      Serial.print("1");
    } else {
      str[charpos] = '0';
      Serial.print("0");
    }
    charpos++;
  }
  Serial.print("\n");
  Serial.println(str);
  
  for (uint8_t i = 0; i < 10; i++) {
    if(!strncmp(str, numstrs[i], 34)) {
      Serial.println(i);
      ret = i;
      break;
    }
  }
  return ret;
}
