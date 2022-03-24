#include <math.h>
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
    if (patternIndex == -1){
      return defaultResult;
    }
    const int endOfPatternIndex = str.indexOf(' ', patternIndex); // (unsigned)(-1 is ok)
    const double result = toNumber<T>(str.substring(patternIndex + pattern.length(), endOfPatternIndex));
    return result;
}

class MyStepper: public Stepper{
  public:
    static constexpr int STEPS_PER_REV = 32;
    static constexpr double GEAR_RED = 63.68395;
    static constexpr double STEPS_PER_OUT_REV = STEPS_PER_REV * GEAR_RED;
  private:
    long currStepState = 0;
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

    double getCurrentAngleDeg() const {
      return stepsToDegrees(currStepState);
    }
};

// wires on pins 2-blue 3-yellow 4-orange 5-pink, same order for right motor
MyStepper motorRight(5, 4, 3, 2);
MyStepper motorLeft(6, 7, 8, 9);

void setup() {
  Serial.begin(115200);
  Serial.setTimeout(10);
  motorLeft.setSpeed(200);
  motorRight.setSpeed(200);
  Serial.println("Plotter ready");
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
  Serial.print("dl");
  Serial.print(String(motorLeft.getCurrentAngleDeg(), 8));
  Serial.print(" dr");
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
      
      Serial.print("alphaL");
      Serial.print(String(motorLeft.getCurrentAngleDeg(), 8));
      Serial.print(" alphaR");
      Serial.println(String(motorRight.getCurrentAngleDeg(), 8));

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
      
    }else if(incomingString.startsWith("bur")){
      for(int i = 0; i < 5 ; i++){
        const double ldegrees = incomingString.substring(3 + i * 12, 3 + i * 12 + 6).toDouble();
        const double rdegrees = incomingString.substring(3 + i * 12 + 6, 3 + i * 12 + 12).toDouble();

        const int lStepsTemp = motorLeft.degreesToSteps(ldegrees);
        const int lStepsRelative = lStepsTemp - motorLeft.getCurrentStepState();

        const int rStepsTemp = motorRight.degreesToSteps(rdegrees);
        const int rStepsRelative = rStepsTemp - motorRight.getCurrentStepState();

        moveMotorsBySteps(lStepsRelative, rStepsRelative);
      }
      printCurrentAngles();
    }else if(incomingString.startsWith("move")){
      const bool relative = incomingString.startsWith("move delta");

      const double ldegrees = getCommandParam<double>(incomingString, "l", 
        relative ? 0.0 : motorLeft.getCurrentAngleDeg());
      const int lStepsTemp = motorLeft.degreesToSteps(ldegrees);
      const int lStepsRelative = 
        relative ? lStepsTemp : lStepsTemp - motorLeft.getCurrentStepState();

      const double rdegrees = getCommandParam<double>(incomingString, "r",
        relative ? 0.0 : motorRight.getCurrentAngleDeg());
      const int rStepsTemp = motorRight.degreesToSteps(rdegrees);
      const int rStepsRelative = 
        relative ? rStepsTemp : rStepsTemp - motorRight.getCurrentStepState();

      moveMotorsBySteps(lStepsRelative, rStepsRelative);

      //return actual angles in degrees (!= requested due to finite stepper resolution)
      printCurrentAngles();
    }else{
      Serial.print("Invalid command: ");
      Serial.println(incomingString);
    }

  }

}
