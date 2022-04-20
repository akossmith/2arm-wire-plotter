#pragma once

#include "stepper.h"
#include "utils.h"
#include "nvm_manager.h"

struct ActuatorsNVMData{
  // nvm data:
  double calibrationOffsetLDeg = 9.0;
  double calibrationOffsetRDeg = 14.49;

  double currAlhpaL = 0.0;
  double currAlhpaR = 0.0;
};

class Actuators: public WithNVMData<ActuatorsNVMData>{
 private:
  MyStepper motorL;
  MyStepper motorR;

  uint8_t calibrationPinLeft;
  uint8_t calibrationPinRight;

 public:
  Actuators(const uint8_t motorLPins[4], const uint8_t motorRPins[4], 
            uint8_t calibrationPinLeft,
            uint8_t calibrationPinRight) 
      : motorL(motorLPins[0], motorLPins[1], motorLPins[2], motorLPins[3]),
        motorR(motorRPins[0], motorRPins[1], motorRPins[2], motorRPins[3]),
        calibrationPinLeft(calibrationPinLeft),
        calibrationPinRight(calibrationPinRight) {
    pinMode(calibrationPinLeft, INPUT_PULLUP);
    pinMode(calibrationPinRight, INPUT_PULLUP);
  }

  virtual void afterLoadFromNVM() override{
    motorL.setCurrentAngleDeg(nvmData.currAlhpaL);
    motorR.setCurrentAngleDeg(nvmData.currAlhpaR);
  };

  virtual void beforeSaveToNVM() const override{
    nvmData.currAlhpaL = motorL.getCurrentAngleDeg();
    nvmData.currAlhpaR = motorR.getCurrentAngleDeg();
  };

  void setSpeed(double rpm) {
    motorL.setSpeed(200);
    motorR.setSpeed(200);
  }

  void zeroStepState() {
    motorL.zeroStepState();
    motorR.zeroStepState();
  }

  pair<double, double> getCurrAngles() {
    return make_pair(motorL.getCurrentAngleDeg(), motorR.getCurrentAngleDeg());
  }

  void moveToDegs(double lDeg, double rDeg) {
    const int lStepsTemp = motorL.degreesToSteps(lDeg);
    const int lStepsDelta = lStepsTemp - motorL.getCurrentStepState();

    const int rStepsTemp = motorR.degreesToSteps(rDeg);
    const int rStepsDelta = rStepsTemp - motorR.getCurrentStepState();

    moveMotorsBySteps(lStepsDelta, rStepsDelta);
  }

  void moveMotorsByAngles(double lDeg, double rDeg) {
    moveMotorsBySteps(motorL.degreesToSteps(lDeg), motorR.degreesToSteps(rDeg));
  }

  void moveMotorsBySteps(int lSteps, int rSteps) {
    // interleaving steps, so that left and right change simultaneously
    MyStepper* motors[] = {&motorL, &motorR};
    const int steps[] = {lSteps, rSteps};
    const size_t smallerIndex = (abs(lSteps) <= abs(rSteps)) ? 0 : 1;
    const size_t biggerIndex = 1 - smallerIndex;
    const double minStepRate =
        abs(steps[biggerIndex]) * 1.0 / abs(steps[smallerIndex]);

    int minStepsTaken = 0;
    for (int i = 0; i < abs(steps[biggerIndex]); i++) {
      motors[biggerIndex]->step(sgn(steps[biggerIndex]));
      if (i > minStepsTaken * minStepRate) {
        motors[smallerIndex]->step(sgn(steps[smallerIndex]));
        minStepsTaken++;
      }
    }
    motors[smallerIndex]->step(steps[smallerIndex] -
                               sgn(steps[smallerIndex]) * minStepsTaken);
  }


  /////////////////////////////////////////////////////////////////////////////////////
  // Calibration methods
  /////////////////////////////////////////////////////////////////////////////////////

  pair<double, double> downUntilSwitchesClose(double upSpeed,
                                              double downSpeed) {
    const double currLSpeed = motorL.getSpeed();  // save curr speeds
    const double currRSpeed = motorR.getSpeed();

    // move up until switch opens
    motorL.setSpeed(upSpeed);
    motorR.setSpeed(upSpeed);
    bool armLeftHigh = digitalRead(calibrationPinLeft);
    bool armRightHigh = digitalRead(calibrationPinRight);
    int armLOffsetSteps = 0;
    int armROffsetSteps = 0;
    while (!armLeftHigh || !armRightHigh) {
      armLOffsetSteps -= 1 - armLeftHigh;
      armROffsetSteps -= 1 - armRightHigh;
      motorL.step(1 - armLeftHigh);
      motorR.step(1 - armRightHigh);
      armLeftHigh = digitalRead(calibrationPinLeft);
      armRightHigh = digitalRead(calibrationPinRight);
    }

    // move down until switch closes
    motorL.setSpeed(downSpeed);
    motorR.setSpeed(downSpeed);
    while (armLeftHigh || armRightHigh) {
      armLOffsetSteps += armLeftHigh;
      armROffsetSteps += armRightHigh;
      motorL.step(-armLeftHigh);
      motorR.step(-armRightHigh);
      armLeftHigh = digitalRead(calibrationPinLeft);
      armRightHigh = digitalRead(calibrationPinRight);
    }

    motorL.setSpeed(currLSpeed);  // restore speeds
    motorR.setSpeed(currRSpeed);

    return make_pair(motorL.stepsToDegrees(armLOffsetSteps),
                     motorR.stepsToDegrees(armROffsetSteps));
  }

  void setAutoCalibrateOffsets(double calibrationOffsetLDeg,
                               double calibrationOffsetRDeg) {
    nvmData.calibrationOffsetLDeg = calibrationOffsetLDeg;
    nvmData.calibrationOffsetRDeg = calibrationOffsetRDeg;
  }

  void autoCalibrate() {
    int16_t calibrationOffsetL =
        motorL.degreesToSteps(nvmData.calibrationOffsetLDeg);
    int16_t calibrationOffsetR =
        motorR.degreesToSteps(nvmData.calibrationOffsetRDeg);

    const double downSpeeds[]{250, 100};
    const double upSpeeds[]{250, 250};
    const uint8_t cycles = 2;

    const double currLSpeed = motorL.getSpeed();  // save curr speeds
    const double currRSpeed = motorR.getSpeed();

    for (int i = 0; i < cycles; i++) {
      downUntilSwitchesClose(upSpeeds[i], downSpeeds[i]);
      // move up by predetermined offset
      motorL.setSpeed(upSpeeds[i]);
      motorR.setSpeed(upSpeeds[i]);
      moveMotorsBySteps(calibrationOffsetL, calibrationOffsetR);
    }

    zeroStepState();

    motorL.setSpeed(currLSpeed);  // restore speeds
    motorR.setSpeed(currRSpeed);
  }

  /// arms must be perfectly horizontal before running this method
  pair<double, double> findCalibrationOffsetsFromHorizontal() {
    return downUntilSwitchesClose(200, 20);
  }

  pair<double, double> getAutoCalOffsets() {
    return make_pair(nvmData.calibrationOffsetLDeg,
                     nvmData.calibrationOffsetRDeg);
  }

  pair<double, double> calibrateCalibrate() {
    auto result = findCalibrationOffsetsFromHorizontal();
    nvmData.calibrationOffsetLDeg = result.first;
    nvmData.calibrationOffsetRDeg = result.second;
    autoCalibrate();
    return result;
  }
};