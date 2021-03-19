/*
 * Arbitrary Analog To Midi Converter
 * 
 * Note: This is Teensy code and uses libs 
 * that don't work on Arduino. 
 * 
 * Takes an analog reading and normalizes it into 
 * the range of accepable MIDI control change 
 * values (0 -> 127). 
 * 
 * Since the Teensy has a 10 bit ADC we will 
 * get an inital digital reading of about 0 -> 1023.
 * 
 * Getting this value into the MIDI control range
 * is just:
 * v = a*(127/1023)
 * 
 * Many analog sensors can only give a full range 
 * of readings in ideal conditions. 
 * 
 * To ensure you can always get a full MIDI control 
 * range of 0-127 you can use
 * a button on (digital) pin 14 to set the minimum 
 * expected value (floor) on the first press. 
 * The second press will set the maximum 
 * expected value (celing). 
 * 
 * The analog reading from the ADC limited
 * by the max value and negatively offset
 * by the minimum. No matter what is given 
 * by the ADC, the value passed is between 0
 * and the length of the range.
 * 
 * The range between the two values is used to 
 * calculate the factor by witch the analog 
 * reading should be scaled in order to become 
 * midi values.  
 *
 * So the conversion scaling factor is: 
 * 127/range
 * 
 * With a range of 508 we would get a scaling 
 * factor of 0.25.
 * 
 * The full conversion is a formula something like:
 * v = a(127/r)
 * or:
 * midi_cc_val = limited_analog_reading*(127/range)
 * 
 * If we have a range of 400->600 (200) and 
 * a limited analog reading of 100
 * could say:
 * 63.5 = 100(127/200)
 * 
 * Which of course is rounded up to 64.
 * As expected this is exactly half of the full
 * range of midi control values.
 * 
 * The rest is just the Teensy midi library. 
 * 
 * Note: The reason I used 127 as the constant 
 * for scaling to midi values and not 128
 * is because 0 is a valid MIDI CC value and 128
 * is not. 
 * 
 * Serial Commands
 * none - stop logging
 * adc - log raw ADC
 * cc - log cc output value
 * dif - log the differnce between the last two ADC reads
 * dif% - log the % differnce between the last two ADC reads
 * diffac - log the differnce factor between the last two ADC reads
 * difmidi - log the differnce between the last two midi sends
 * range - show information about range and scaling values
 * 1hz - set a sample rate of 1hz (1 second between samples)
 * 20hz - set a sample rate of 20hz (50ms between samples)
 * 200hz - set a sample rate of 200hz (5ms between samples, default)
 * real - set the sample rate to "realtime".
 * notemode - toggle "note mode" where analog inputs >1 can play a note.
 * ccmode - toggle "cc mode" where the analog input controls MIDI CC (default on).
 * https://www.pjrc.com/teensy/td_midi.html
 * 
 */

// Analog pins are indexed differently than digital
// Analog pin 1 = Digital 15
int APIN = 1;// Arbitrary Analog Sensor
int BPIN = 14;// BUTTON
int BPIN2 = 5;// BUTTON 2

// State
int CCVAL = 0;//CC Value
int AFLOOR = 0;//Calibration floor
int ACEIL = 1023;//Calibration ceiling
bool SETSTATE = 0;//Min || Max
bool BTN = 0;//Button state
bool BTN2 = 0;//Button2 state
int LASTAREAD = 0;
int LTYP = 0;//Log Type
bool NOTEON = false;
bool NOTEMODE = false;
bool CCMODE = true;
//Increase these for noisy sensors
int DELAYT = 5;//Miliseconds to delay  after val change
int NOISEGATE = 0;



void setup() {
  pinMode(BPIN, INPUT);
  pinMode(BPIN2, INPUT);
  Serial.begin(9600);
  Serial.println("STARTING MIDI CONTROLLER");
}

void sDebug(){
  if (Serial.available() > 0) {
    String CMD = Serial.readString();
    CMD = CMD.trim();
    Serial.print(">>");
    Serial.println(CMD);
    if(CMD == "none"){
      LTYP = 0;
    }
    if(CMD == "adc"){
      LTYP = 1;
    }
    if(CMD == "cc"){
      LTYP = 2;
    }
    if(CMD == "dif"){
      LTYP = 3;
    }
    if(CMD == "dif%"){
      LTYP = 4;
    }
    if(CMD == "diffac"){
      LTYP = 5;
    }
    if(CMD == "difmidi"){
      LTYP = 6;
    }
    if(CMD == "1hz"){
      Serial.println("SAMPLE RATE SET TO 1hz");
      DELAYT = 1000;
    }
    if(CMD == "20hz"){
      Serial.println("SAMPLE RATE SET TO 20hz");
      DELAYT = 50;
    }
    if(CMD == "200hz"){
      Serial.println("SAMPLE RATE SET TO 200hz");
      DELAYT = 5;
    }
    if(CMD == "real"){
      Serial.println("SAMPLE RATE SET TO SOMETHING VERY FAST");
      DELAYT = 0;
    }
    if(CMD == "range"){
      LTYP = 0;// Turn off other logging
      Serial.print("MIN:");
      Serial.println(AFLOOR);
      Serial.print("MAX:");
      Serial.println(ACEIL);
      int RANGE = ACEIL - AFLOOR;
      float FACTOR = 127.00 / float(RANGE);
      Serial.print("RANGE:");
      Serial.println(RANGE);
      Serial.print("FACTOR:");
      Serial.println(FACTOR);
    }
    if(CMD == "autonoise"){
      autoNoiseGate();
    }
    if(CMD == "denoisebig"){
      NOISEGATE = 25;
    }
    if(CMD == "denoise"){
      NOISEGATE =5;
    }
    if(CMD == "allnoise"){
      NOISEGATE =0;
    }
    if(CMD == "notemode"){
      NOTEMODE = !NOTEMODE;
    }
    if(CMD == "ccmode"){
      CCMODE = !CCMODE;
    }
  }
}

void autoNoiseGate(){
  // Very poor. Needs more samples.
  int READINGA = analogRead(APIN);
  int READINGB = analogRead(APIN);
  int NOISEGATE = abs(READINGA - READINGB);
  Serial.println(READINGA);
  Serial.println(READINGB);
  Serial.println("SET NOISE GATE TO");
  Serial.println(NOISEGATE);
}

bool passNoiseGate(int READING){
  if(abs(READING - LASTAREAD) > NOISEGATE){
    return true;
  }
  return false;
}

void setState(int AREAD){
  // AREAD for easy expansion
  int AIN = analogRead(AREAD);
  if(SETSTATE == 0){
    // set min
    AFLOOR = AIN;
    Serial.print("SET MIN: ");
    Serial.println(AFLOOR);
  }else{
    // set max
    ACEIL = AIN;
    Serial.print("SET MAX: ");
    Serial.println(ACEIL);
  }
  SETSTATE = !SETSTATE;
  delay(300);// quick debounce help
}

int analogToMIDI(int AREAD, int MINV = 0, int MAXV = 1023){
  // AREAD for easy expansion
  int READING = analogRead(AREAD);
  if(READING < MINV){
    READING = MINV;
  }
  if(READING > MAXV){
    READING = MAXV;
  }
  READING = READING - MINV;
  int RANGE = MAXV - MINV;
  float FACTOR = 127.00 / float(RANGE);
  int VAL = READING *  FACTOR;
  if(passNoiseGate(READING)==false){
    return LASTAREAD *  FACTOR;
  }
  if(LTYP == 1){//Debug ADC
    Serial.println(READING);
  }
  if(LTYP == 2){//Debug conversion
    Serial.println(VAL);
  }
  if(LTYP == 3){// Debug sensor noise
    Serial.println( READING - LASTAREAD);
  }
  if(LTYP == 4){// Debug sensor noise (%)
    Serial.print( abs((READING - LASTAREAD)/1023.00)*100 );
    Serial.println("%");
  }
  if(LTYP == 5){// Debug sensor noise (factor)
    Serial.println( (READING - LASTAREAD)/1023.00);
  }
  if(LTYP == 6){// Debug midi out noise
    int VALD = VAL - (LASTAREAD * FACTOR);
    if(passNoiseGate(READING)){
      Serial.println(VALD);
    }
  }
  LASTAREAD = READING;
  return VAL;
}

void loop() {
  int ARANGE = ACEIL - AFLOOR;
  int CCVALR = analogToMIDI(APIN, AFLOOR, ACEIL);
  if(CCVAL != CCVALR){
    CCVAL = CCVALR;
    if(CCMODE){
      usbMIDI.sendControlChange(1, CCVAL, 1);
    }
    if(NOTEMODE == true && CCVAL > 1 && NOTEON == false){
      Serial.println("SENDING NOTE (MODE)");
      usbMIDI.sendNoteOn(60, 100, 1);
      NOTEON = true;
    }else if(NOTEMODE == true && NOTEON == true && CCVAL < 1){
      Serial.println("STOPPING NOTE (MODE)");
      usbMIDI.sendNoteOff(60, 0, 1);
      NOTEON = false;
    }
    delay(DELAYT);//Avoid overload, hacky
  }
  int BTNR = digitalRead(BPIN);
  if(BTN == 1 && BTNR != 1){
    setState(APIN);
  }
  BTN = BTNR;
  //
  BTNR = digitalRead(BPIN2);
  if(BTN2 == 1 && BTNR != 1){
    NOTEON = !NOTEON;
    if(NOTEON){
      Serial.println("SENDING NOTE");
      usbMIDI.sendNoteOn(60, 100, 1);
    }else{
      usbMIDI.sendNoteOff(60, 0, 1);
      Serial.println("STOPPING NOTE");
    }
    delay(300);// quick debounce help
  }
  BTN2 = BTNR;
  sDebug();
}
