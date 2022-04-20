#pragma once

#include <Stepper.h>

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