#include <math.h>
#include <Stepper.h>

const int16_t autocalibration_offset_left = 64;
const int16_t autocalibration_offset_right = 34;

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
MyStepper motorLeft(6, 7, 8, 9);
//
//const int16_t autocalibration_offset_left = 64;
//const int16_t autocalibration_offset_right = 34;


void autoCalibrate(MyStepper& motor, uint8_t switchPin, int16_t calibrationOffset);
void autoCalibrate(MyStepper& motor, uint8_t switchPin, int16_t calibrationOffset){
  const double currSpeed = motor.getSpeed();
  motor.setSpeed(250);
  bool pinHigh = digitalRead(switchPin);
  while(! pinHigh){  // move up
    motor.step(1);
    pinHigh = digitalRead(switchPin);
  }
  motor.setSpeed(100);
  while(pinHigh){  // move down
    motor.step(-1);
    delay(10);
    pinHigh = digitalRead(switchPin);
  }
  motor.setSpeed(250);
  motor.step(calibrationOffset); //move up
  motor.zeroStepState();
  motor.setSpeed(currSpeed);
}


void setup() {
  pinMode(A0, INPUT_PULLUP);
  pinMode(A1, INPUT_PULLUP);
  Serial.begin(115200);
  Serial.setTimeout(10);
  motorLeft.setSpeed(200);
  motorRight.setSpeed(200);
  Serial.println("Plotter ready");
//
//  delay(5000);
//  motorLeft.step(100);
//  motorRight.step(100);
//  
//  bool pinHigh = digitalRead(A0);
//  int numSteps = 0;
//  while(pinHigh){
//    motorLeft.step(-1);
//    numSteps++;
//    delay(100);
//    pinHigh = digitalRead(A0);
//  }
//  Serial.println(numSteps);
//  
//  pinHigh = true;
//  numSteps =digitalRead(A1);
//  while(pinHigh){
//   motorRight.step(-1);
//   numSteps++;
//   delay(100);
//   pinHigh = digitalRead(A1);
//  }
//   Serial.println(numSteps);
}

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

void printCurrentAngles(){
  Serial.print("ok ");
  Serial.print(String(motorLeft.getCurrentAngleDeg(), 8));
  Serial.print(" ");
  Serial.println(String(motorRight.getCurrentAngleDeg(), 8));
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
      
    }else if(incomingString.startsWith("calibrate")){
      
      const double leftAngle = getCommandParam<double>(incomingString, "l", motorLeft.getCurrentAngleDeg());
      motorLeft.calibrate(leftAngle);
      const double rightAngle = getCommandParam<double>(incomingString, "r", motorRight.getCurrentAngleDeg());
      motorRight.calibrate(rightAngle);
      Serial.println("Calibration done");
      
    }else if(incomingString.startsWith("autocalibrate")){
      
      const int16_t lstepsOffset = getCommandParam<long>(incomingString, "l", autocalibration_offset_left);
      const int16_t rstepsOffset = getCommandParam<long>(incomingString, "r", autocalibration_offset_right);
      autoCalibrate(motorLeft, A0, lstepsOffset);
      autoCalibrate(motorRight, A1, rstepsOffset);
      autoCalibrate(motorLeft, A0, lstepsOffset); // has to be done twice
      autoCalibrate(motorRight, A1, rstepsOffset);
      
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

}
