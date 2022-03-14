#include <math.h>
#include <Stepper.h>

template <typename T> int sgn(T val) {
   return (T(0) < val) - (val < T(0));
}

const double STEPS_PER_REV = 32.0;
const double GEAR_RED = 64.0;
const double STEPS_PER_OUT_REV = STEPS_PER_REV * GEAR_RED;


// wires on pins 2-blue 3-yellow 4-orange 5-pink, same order for right motor
Stepper motorLeft(STEPS_PER_REV, 2, 3, 4, 5);
Stepper motorRight(STEPS_PER_REV, 9, 8, 7, 6);

int degreesToSteps(double degrees) {
  const double stepsPerDegree = STEPS_PER_OUT_REV / 360.0;
  return round(degrees * stepsPerDegree);
}

double stepsToDegrees(int steps){
  return double(steps) / STEPS_PER_OUT_REV * 360.0;
}

void setup() {
  Serial.begin(115200);
  Serial.setTimeout(10);
  motorLeft.setSpeed(200);
  motorRight.setSpeed(200);
  Serial.println("ready");
}

void loop() {
  if (Serial.available() > 0) {
    String incomingString = Serial.readString();
    if(incomingString[0] == 's'){
        int speed = int(incomingString.substring(1).toDouble()); // expected double for future improvements
        motorLeft.setSpeed(speed);
        motorRight.setSpeed(speed);
        Serial.print("s"); Serial.println(double(speed), 8);
        return;
    }
    const int rInd = incomingString.indexOf('r');
    const double ldegrees = incomingString.substring(1, rInd).toDouble();
    const int lsteps = degreesToSteps(ldegrees);

    const double rdegrees = incomingString.substring(rInd + 1).toDouble();
    const int rsteps = degreesToSteps(rdegrees);

    //interleaving steps, so that left and right change simultaneously
    Stepper *motors[] = {&motorLeft, &motorRight};
    int steps[] = {lsteps, rsteps};
    size_t smallerIndex = (abs(lsteps) < abs(rsteps)) ? 0 : 1;
    size_t biggerIndex = 1 - smallerIndex;
    double minStepRate = abs(steps[biggerIndex]) * 1.0 / abs(steps[smallerIndex]);
    
    int minStepsTaken = 0;
    for(int i = 0; i < abs(steps[biggerIndex]); i++){
      motors[biggerIndex]->step(sgn(steps[biggerIndex]));
      if(i > minStepsTaken * minStepRate){
        motors[smallerIndex]->step(sgn(steps[smallerIndex]));
        minStepsTaken ++;
      }
    } 
    motors[smallerIndex]->step(steps[smallerIndex] - sgn(steps[smallerIndex]) * minStepsTaken);

    //return actual angle deltas in degrees (!= requested due to 
    Serial.print("dl");
    Serial.print(String(stepsToDegrees(lsteps), 8));
    Serial.print(" dr");
    Serial.println(String(stepsToDegrees(rsteps), 8));
  }

}
