#include <math.h>
#include <EEPROM.h>
#include <Stepper.h>

template <typename T> 
int sgn(T val) {
   return (T(0) < val) - (val < T(0));
}

template <typename T> 
T toNumber(const String& str){
  return static_cast<T>(str);
}

template <> 
double toNumber<double>(const String& str){
  return str.toDouble();
}

template <> 
long toNumber<long>(const String& str){
  return str.toInt();
}

/// command format: "<commmand> <param1 name><param1value> <param2 name><param2 value> ..."
template <typename T> 
T getCommandParam(const String& str,const String& pattern, T defaultResult = T()) {
    const int endOfCommandIndex = str.indexOf(' ');
    const int patternIndex = str.indexOf(pattern, endOfCommandIndex + 1);
    if (endOfCommandIndex == -1 || patternIndex == -1){
      return defaultResult;
    }
    const int endOfPatternIndex = str.indexOf(' ', patternIndex); // (unsigned)(-1 is ok)
    const double result = toNumber<T>(str.substring(patternIndex + pattern.length(), endOfPatternIndex));
    return result;
}

class PowerLowHandler{
  const int capacitorChargeLevel;
  const uint8_t capacitorChargePin;
  const uint8_t capacitorSensePin;
  bool ready = false;
  volatile bool lowVoltageFlag = false;
  void  (*handlerCallback)();

  void discharge()const{
    pinMode(capacitorChargePin, OUTPUT);
    digitalWrite(capacitorChargePin, 0);
    int res = analogRead(capacitorSensePin);
    while(res > capacitorChargeLevel){
      res = analogRead(capacitorSensePin);
    }
    pinMode(capacitorChargePin, INPUT);
  }
  
  void charge()const{

    pinMode(capacitorChargePin, OUTPUT);
    digitalWrite(capacitorChargePin, 1);
    int res = analogRead(capacitorSensePin);
    while(res <= capacitorChargeLevel){
      res = analogRead(capacitorSensePin);
    }
    pinMode(capacitorChargePin, INPUT);
  }
  
 public:
  PowerLowHandler(
    float capacitorChargeLevelPercent, 
    uint8_t capacitorChargePin,
    uint8_t capacitorSensePin
   )
    :capacitorChargeLevel(int(capacitorChargeLevelPercent * 1023))
    ,capacitorChargePin(capacitorChargePin)
    ,capacitorSensePin(capacitorSensePin)
  {
  }

  void init(){ //ADC only initialized in main()
    pinMode(capacitorChargePin, INPUT);
    pinMode(capacitorSensePin, INPUT);
    discharge();
    charge();
    ready = true;
  }

  void initIfNotReady(){
    if(!ready){
      init();
    }
  }
  
  void maintain(){
    initIfNotReady();
    
    int res = analogRead(capacitorSensePin);
    if(res < capacitorChargeLevel){
      charge();
    }
  }

  bool isReady()const{
    return ready;
  }

  bool lowVoltage()const{
    return lowVoltageFlag;
  }

  void setHandlerCallback(void (*callback)()){
    this->handlerCallback = callback;
  }

  void handlePowerLoss(){
    handlerCallback();
    lowVoltageFlag = true;
  }
} powerLossHandler(0.9, A5, A7);

class MyStepper;
class MyStepper: public Stepper{
  public:
    static constexpr int STEPS_PER_REV = 32;
    static constexpr double GEAR_RED = 63.68395;
    static constexpr double STEPS_PER_OUT_REV = STEPS_PER_REV * GEAR_RED;
  private:
    long currStepState = 0;
    double rpm;
  public:
    MyStepper(int pin1, int pin2, int pin3, int pin4)
      :Stepper(STEPS_PER_REV, pin1, pin2, pin3, pin4){       

    }

    int degreesToSteps(double degrees) const {
      const double stepsPerDegree = STEPS_PER_OUT_REV / 360.0;
      return round(degrees * stepsPerDegree);
    }

    double stepsToDegrees(int steps) const {
      return double(steps) / STEPS_PER_OUT_REV * 360.0;
    }

    void zeroStepState(){
      currStepState = 0; //calibrate(0.0);
    }

    void calibrate(double angle){
      currStepState = degreesToSteps(angle);
      
    }

    long getCurrentStepState() const {
      return currStepState;
    }

    void step(int numberOfSteps){
      currStepState += numberOfSteps;
      Stepper::step(numberOfSteps);
    }

    void setSpeed(long rpm){
      this->rpm = rpm;
      Stepper::setSpeed(static_cast<long>(rpm + 0.5));
    }

    double getSpeed()const{
      return rpm;
    }

    double getCurrentAngleDeg() const {
      return stepsToDegrees(currStepState);
    }
};

// wires on pins 2-blue 3-yellow 4-orange 5-pink, same order for right motor
MyStepper motorRight(5, 4, 3, 2);
MyStepper motorLeft(8, 9, 10, 11);



void moveMotorsBySteps(int lSteps, int rSteps){
  //interleaving steps, so that left and right change simultaneously
  MyStepper *motors[] = {&motorLeft, &motorRight};
  const int steps[] = {lSteps, rSteps};
  const size_t smallerIndex = (abs(lSteps) <= abs(rSteps)) ? 0 : 1;
  const size_t biggerIndex = 1 - smallerIndex;
  const double minStepRate = abs(steps[biggerIndex]) * 1.0 / abs(steps[smallerIndex]);
  
  int minStepsTaken = 0;
  for(int i = 0; i < abs(steps[biggerIndex]); i++){
    motors[biggerIndex]->step(sgn(steps[biggerIndex]));
    if(i > minStepsTaken * minStepRate){
      motors[smallerIndex]->step(sgn(steps[smallerIndex]));
      minStepsTaken ++;
    }
  } 
  motors[smallerIndex]->step(steps[smallerIndex] - sgn(steps[smallerIndex]) * minStepsTaken);
}


static struct EEPROMData{
  double autocalibrationOffsetLeftDeg = 9.0;
  double autocalibrationOffsetRightDeg = 14.49;

  double currAlhpaL;
  double currAlhpaR;

 void load(){
   EEPROM.get(0, *this);
 }

 void save(){
   EEPROM.put(0, *this);
 }
} eepromData;

void autoCalibrate(double calibrationOffsetLDeg, double calibrationOffsetRDeg);
void autoCalibrate(double calibrationOffsetLDeg, double calibrationOffsetRDeg){
  int16_t calibrationOffsetL = motorLeft.degreesToSteps(calibrationOffsetLDeg);
  int16_t calibrationOffsetR = motorRight.degreesToSteps(calibrationOffsetRDeg);

  const double downSpeeds[]{250, 100};
  const double upSpeeds[]{250, 250};
  const uint8_t cycles = 2;
  
  const double currLSpeed = motorLeft.getSpeed();
  const double currRSpeed = motorRight.getSpeed();

  for (int i = 0; i < cycles; i++){
    // move up until switch opens
    motorLeft.setSpeed(upSpeeds[i]);
    motorRight.setSpeed(upSpeeds[i]);
    bool armLeftHigh = digitalRead(A0);
    bool armRightHigh = digitalRead(A1);
    while(! armLeftHigh || ! armRightHigh){
      motorLeft.step(1 - armLeftHigh);
      motorRight.step(1 - armRightHigh);
      armLeftHigh = digitalRead(A0);
      armRightHigh = digitalRead(A1);
    }
    
    // move down until switch closes
    motorLeft.setSpeed(downSpeeds[i]);
    motorRight.setSpeed(downSpeeds[i]);
    while(armLeftHigh || armRightHigh){
      motorLeft.step(-armLeftHigh);
      motorRight.step(-armRightHigh);
      armLeftHigh = digitalRead(A0);
      armRightHigh = digitalRead(A1);
    }
    
    // move up by predetermined offset
    motorLeft.setSpeed(upSpeeds[i]);
    motorRight.setSpeed(upSpeeds[i]);
    moveMotorsBySteps(calibrationOffsetL, calibrationOffsetR);
  }
  motorLeft.zeroStepState();
  motorRight.zeroStepState();
  eepromData.autocalibrationOffsetLeftDeg = calibrationOffsetLDeg;
  eepromData.autocalibrationOffsetRightDeg = calibrationOffsetRDeg;
  eepromData.save();
  motorLeft.setSpeed(currLSpeed);
  motorRight.setSpeed(currRSpeed);
}

void printCurrentAngles(){
  Serial.print("ok ");
  Serial.print(String(motorLeft.getCurrentAngleDeg(), 8));
  Serial.print(" ");
  Serial.println(String(motorRight.getCurrentAngleDeg(), 8));
}

void setup() {
  pinMode(A0, INPUT_PULLUP);
  pinMode(A1, INPUT_PULLUP);

  ADCSRB = 0;           // (Disable) ACME: Analog Comparator Multiplexer Enable
  ACSR =  bit (ACI)     // (Clear) Analog Comparator Interrupt Flag
        | bit (ACIE)    // Analog Comparator Interrupt Enable
        | bit (ACIS1);  // ACIS1, ACIS0: Analog Comparator Interrupt Mode Select (trigger on falling edge)

  Serial.begin(115200);
  Serial.setTimeout(10);
  motorLeft.setSpeed(200);
  motorRight.setSpeed(200);
  powerLossHandler.init();
  powerLossHandler.setHandlerCallback([](){
    ::eepromData.currAlhpaL = motorLeft.getCurrentAngleDeg();
    ::eepromData.currAlhpaR = motorRight.getCurrentAngleDeg();
    ::eepromData.save();
    digitalWrite(LED_BUILTIN, 1);
  });
  eepromData.load();
  motorLeft.calibrate(eepromData.currAlhpaL);
  motorRight.calibrate(eepromData.currAlhpaR);
  Serial.println("Plotter ready");

//  delay(5000);
//  motorLeft.zeroStepState();
//  motorRight.zeroStepState();
//  motorLeft.step(100);
//  motorRight.step(100);
//  
//  bool pinHigh = digitalRead(A0);
//  int numSteps = 0;
//  while(pinHigh){
//    motorLeft.step(-1);
//    numSteps++;
//    delay(80);
//    pinHigh = digitalRead(A0);
//  }
//  Serial.println(motorLeft.stepsToDegrees(numSteps - 100));
//  
//  pinHigh = true;
//  numSteps =digitalRead(A1);
//  while(pinHigh){
//   motorRight.step(-1);
//   numSteps++;
//   delay(80);
//   pinHigh = digitalRead(A1);
//  }
//   Serial.println(motorRight.stepsToDegrees(numSteps - 100));
}

void loop() {
  if (Serial.available() > 0) {
    String incomingString = Serial.readStringUntil('\n');
    if(incomingString.startsWith("setSpeed")){
      
      int speed = int(incomingString.substring(String("setSpeed").length() + 1).toDouble());// expecting double for future improvements
      motorLeft.setSpeed(speed);
      motorRight.setSpeed(speed);
      Serial.print("s"); Serial.println(double(speed), 8);

    }else if(incomingString.startsWith("getAlphas")){
      
      printCurrentAngles();

    }else if(incomingString.startsWith("zeroAngles")){
      
      motorLeft.zeroStepState();
      motorRight.zeroStepState();
      Serial.println("Position zeroed");
      
    }else if(incomingString.startsWith("calibrate ")){
      
      const double leftAngle = getCommandParam<double>(incomingString, "l", motorLeft.getCurrentAngleDeg());
      motorLeft.calibrate(leftAngle);
      const double rightAngle = getCommandParam<double>(incomingString, "r", motorRight.getCurrentAngleDeg());
      motorRight.calibrate(rightAngle);
      Serial.println("Calibration done");
      
    }else if(incomingString.startsWith("autocalibrate")){
      
      const double lstepsOffset = getCommandParam<double>(
        incomingString,
        "l", 
        eepromData.autocalibrationOffsetLeftDeg
      );
      const double rstepsOffset = getCommandParam<double>(
        incomingString,
        "r",
        eepromData.autocalibrationOffsetRightDeg
      );
      autoCalibrate(lstepsOffset, rstepsOffset);
      
      printCurrentAngles();
      
    }else if(incomingString.startsWith("burst")){

      Serial.println("entered burst mode");
      const uint16_t size = getCommandParam<long>(incomingString, "s", 15);

      uint8_t buffer[size * 4 + 4];
      Serial.readBytes(buffer, size * 4 + 4);

      uint32_t expectedChecksum = static_cast<uint32_t>(buffer[size * 4 + 0]) << 8;
      expectedChecksum = (expectedChecksum + buffer[size * 4 + 1]) << 8;
      expectedChecksum = (expectedChecksum + buffer[size * 4 + 2]) << 8;
      expectedChecksum = expectedChecksum + buffer[size * 4 + 3];

      uint32_t actualChecksum = 0;
      for(uint16_t i = 0; i < size; i++){
        uint32_t temp = static_cast<uint32_t>(buffer[(i + 1) * 4 - 4]) << 8;
        temp = (temp + buffer[(i + 1) * 4 - 3]) << 8;
        temp = (temp + buffer[(i + 1) * 4 - 2]) << 8;
        temp =  temp + buffer[(i + 1) * 4 - 1];
        actualChecksum += temp;
      }

      if (expectedChecksum != actualChecksum){
        Serial.print("checksum error");
        return;
      }

      for(uint16_t i = 0; i < size; i++ ){
        const double ldegrees = buffer[i * 4] + buffer[i * 4 + 1] / 255.0;
        const double rdegrees = buffer[i * 4 + 2] + buffer[i * 4 + 3] / 255.0;

        const int lStepsTemp = motorLeft.degreesToSteps(ldegrees);
        const int lStepsDelta = lStepsTemp - motorLeft.getCurrentStepState();

        const int rStepsTemp = motorRight.degreesToSteps(rdegrees);
        const int rStepsDelta = rStepsTemp - motorRight.getCurrentStepState();

        moveMotorsBySteps(lStepsDelta, rStepsDelta);
      }
      printCurrentAngles();

    }else if(incomingString.startsWith("move")){

      const bool relative = incomingString.startsWith("moved");

      const double ldegrees = getCommandParam<double>(incomingString, "l", 
        relative ? 0.0 : motorLeft.getCurrentAngleDeg());
      const int lStepsTemp = motorLeft.degreesToSteps(ldegrees);
      const int lStepsDelta = 
        relative ? lStepsTemp : lStepsTemp - motorLeft.getCurrentStepState();

      const double rdegrees = getCommandParam<double>(incomingString, "r",
        relative ? 0.0 : motorRight.getCurrentAngleDeg());
      const int rStepsTemp = motorRight.degreesToSteps(rdegrees);
      const int rStepsDelta = 
        relative ? rStepsTemp : rStepsTemp - motorRight.getCurrentStepState();

      moveMotorsBySteps(lStepsDelta, rStepsDelta);

      //return actual angles in degrees (!= requested due to finite stepper resolution)
      printCurrentAngles();
      
    }else{
      Serial.print("Invalid command: ");
      Serial.println(incomingString);
    }

  }
  powerLossHandler.maintain();
}

ISR (ANALOG_COMP_vect)
{
  noInterrupts();
  if(! powerLossHandler.isReady()){
    return;
  }
  powerLossHandler.handlePowerLoss();
  interrupts();
}
