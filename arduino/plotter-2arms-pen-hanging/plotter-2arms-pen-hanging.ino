#include <math.h>
#include <Stepper.h>

#include "flash_manager.h"

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

template<typename first_type, typename second_type>
struct pair{
  first_type first;
  second_type second;
};

template<typename first_type, typename second_type>
pair<first_type, second_type> make_pair(first_type first, second_type second);

template<typename first_type, typename second_type>
pair<first_type, second_type> make_pair(first_type first, second_type second){
  return pair<first_type, second_type>{first, second};
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

    void (*angleChangeCallback)(double currAngle) = [](double){};
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
      setCurrentAngleDeg(0.0);
    }

    void setCurrentAngleDeg(double angle){
      currStepState = degreesToSteps(angle);
      if(angleChangeCallback){
        angleChangeCallback(getCurrentAngleDeg());
      }
    }

    long getCurrentStepState() const {
      return currStepState;
    }

    void step(int numberOfSteps){
      currStepState += numberOfSteps;
      Stepper::step(numberOfSteps);

      angleChangeCallback(getCurrentAngleDeg());
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

    void setAngleChangeCallback(void (*callback)(double currAngle)){
      this->angleChangeCallback = callback;
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

struct EEPROMDataRaw{
  double autocalibrationOffsetLDeg = 9.0;
  double autocalibrationOffsetRDeg = 14.49;

  double currAlhpaL = 0.0;
  double currAlhpaR = 0.0;
};

FlashManager<EEPROMDataRaw> flashManager;//{EEPROMDataRaw()};

class EEPROMData: public EEPROMDataRaw{
public:
  void setAlphaLDeg(double angleDeg){
    currAlhpaL = angleDeg;
  };

  void setAlphaRDeg(double angleDeg){
    currAlhpaR = angleDeg;
  };

  void load(){
    flashManager.get(*this);
  }

  void save() const{
    flashManager.put(*this);
  }
} eepromData;

pair<double, double> downUntilSwitchesClose(double upSpeed, double downSpeed);
pair<double, double> downUntilSwitchesClose(double upSpeed, double downSpeed){
    const double currLSpeed = motorLeft.getSpeed(); // save curr speeds
    const double currRSpeed = motorRight.getSpeed();

    // move up until switch opens
    motorLeft.setSpeed(upSpeed);
    motorRight.setSpeed(upSpeed);
    bool armLeftHigh = digitalRead(A0);
    bool armRightHigh = digitalRead(A1);
    int armLOffsetSteps = 0;
    int armROffsetSteps = 0;
    while(! armLeftHigh || ! armRightHigh){
      armLOffsetSteps -= 1 - armLeftHigh;
      armROffsetSteps -= 1 - armRightHigh;
      motorLeft.step(1 - armLeftHigh);
      motorRight.step(1 - armRightHigh);
      armLeftHigh = digitalRead(A0);
      armRightHigh = digitalRead(A1);
    }
    
    // move down until switch closes
    motorLeft.setSpeed(downSpeed);
    motorRight.setSpeed(downSpeed);
    while(armLeftHigh || armRightHigh){
      armLOffsetSteps += armLeftHigh;
      armROffsetSteps += armRightHigh;
      motorLeft.step(-armLeftHigh);
      motorRight.step(-armRightHigh);
      armLeftHigh = digitalRead(A0);
      armRightHigh = digitalRead(A1);
    }

    motorLeft.setSpeed(currLSpeed); // restore speeds
    motorRight.setSpeed(currRSpeed);
  
    return make_pair(
      motorLeft.stepsToDegrees(armLOffsetSteps),
      motorRight.stepsToDegrees(armROffsetSteps)
    );
}

void autoCalibrate(double calibrationOffsetLDeg, double calibrationOffsetRDeg);
void autoCalibrate(double calibrationOffsetLDeg, double calibrationOffsetRDeg){
  int16_t calibrationOffsetL = motorLeft.degreesToSteps(calibrationOffsetLDeg);
  int16_t calibrationOffsetR = motorRight.degreesToSteps(calibrationOffsetRDeg);

  const double downSpeeds[]{250, 100};
  const double upSpeeds[]{250, 250};
  const uint8_t cycles = 2;

  const double currLSpeed = motorLeft.getSpeed(); // save curr speeds
  const double currRSpeed = motorRight.getSpeed();

  for (int i = 0; i < cycles; i++){
    downUntilSwitchesClose(upSpeeds[i], downSpeeds[i]);
    // move up by predetermined offset
    motorLeft.setSpeed(upSpeeds[i]);
    motorRight.setSpeed(upSpeeds[i]);
    moveMotorsBySteps(calibrationOffsetL, calibrationOffsetR);
  }
  motorLeft.zeroStepState();
  motorRight.zeroStepState();
  eepromData.autocalibrationOffsetLDeg = calibrationOffsetLDeg;
  eepromData.autocalibrationOffsetRDeg = calibrationOffsetRDeg;
  eepromData.save();

  motorLeft.setSpeed(currLSpeed); // restore speeds
  motorRight.setSpeed(currRSpeed);
}

pair<double, double> calibrateCalibrate(){
  auto result = downUntilSwitchesClose(200, 20);
  eepromData.autocalibrationOffsetLDeg = result.first;
  eepromData.autocalibrationOffsetRDeg = result.second;
  eepromData.save();
  autoCalibrate(eepromData.autocalibrationOffsetLDeg, eepromData.autocalibrationOffsetRDeg);
  return result;
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

  Serial.begin(115200);
  Serial.setTimeout(10);

  motorLeft.setSpeed(200);
  motorRight.setSpeed(200);

  eepromData.load();
  motorLeft.setCurrentAngleDeg(eepromData.currAlhpaL);
  motorRight.setCurrentAngleDeg(eepromData.currAlhpaR);

  motorLeft.setAngleChangeCallback([](double currAngle){
    ::eepromData.currAlhpaL = currAngle;
  });
  motorRight.setAngleChangeCallback([](double currAngle){
    ::eepromData.currAlhpaR = currAngle;
  });
  Serial.println("Plotter ready");
}

void loop() {
  if (Serial.available() > 0) {
    String incomingString = Serial.readStringUntil('\n');
    if(incomingString.startsWith("setspeed")){
      
      int speed = int(incomingString.substring(String("setSpeed").length() + 1).toDouble());// expecting double for future improvements
      motorLeft.setSpeed(speed);
      motorRight.setSpeed(speed);
      Serial.print("s"); Serial.println(double(speed), 8);

    }else if(incomingString.startsWith("getcurrangles")){
      
      printCurrentAngles();

    }else if(incomingString.startsWith("zeroangles")){
      
      motorLeft.zeroStepState();
      motorRight.zeroStepState();
      Serial.println("Position zeroed");
      
    }else if(incomingString.startsWith("calautocal")){
      // arms need to be level to begin with
      auto result = calibrateCalibrate();
      Serial.print(result.first, 4);
      Serial.print(" ");
      Serial.println(result.second, 4);
      
    }else if(incomingString.startsWith("getautocaloffs")){
      Serial.print(String(eepromData.autocalibrationOffsetLDeg, 8));
      Serial.print(" ");
      Serial.println(String(eepromData.autocalibrationOffsetRDeg, 8));
      
    }else if(incomingString.startsWith("autocal")){
      
      const double lstepsOffset = getCommandParam<double>(
        incomingString,
        "l", 
        eepromData.autocalibrationOffsetLDeg
      );
      const double rstepsOffset = getCommandParam<double>(
        incomingString,
        "r",
        eepromData.autocalibrationOffsetRDeg
      );
      autoCalibrate(lstepsOffset, rstepsOffset);
      
      printCurrentAngles();
      
    }else if(incomingString.startsWith("saveangles")){
      
      eepromData.save();

      printCurrentAngles();

    }else if(incomingString.startsWith("setcurrangles ")){
      
      const double leftAngle = getCommandParam<double>(incomingString, "l", motorLeft.getCurrentAngleDeg());
      motorLeft.setCurrentAngleDeg(leftAngle);
      const double rightAngle = getCommandParam<double>(incomingString, "r", motorRight.getCurrentAngleDeg());
      motorRight.setCurrentAngleDeg(rightAngle);
      Serial.println("Calibration done");

      
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

      const bool relative = incomingString.startsWith("moveby"); // vs moveto

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
