#ifndef _APP_MAIN_H
#define _APP_MAIN_H

#ifndef TESTING

#include <math.h>

#include "actuators.h"
#include "stepper.h"
#include "utils.h"
#include "nvm_manager.h"
#include "flash_manager.h"

// wires on pins 2-blue 3-yellow 4-orange 5-pink, same order for right motor
constexpr uint8_t motorLPins[] {8, 9, 10, 11};
constexpr uint8_t motorRPins[] {5, 4, 3, 2};

constexpr uint8_t penServoPin = 6;
constexpr uint8_t motorLCalibrationSwitchPin = A0;
constexpr uint8_t motorRCalibrationSwitchPin = A1;

Actuators actuators{motorLPins, motorRPins, penServoPin, motorLCalibrationSwitchPin, motorRCalibrationSwitchPin};

NVMManager<FlashManager, Actuators> nvmManager;

void printCurrentAngles() {
  auto angles = actuators.getCurrAngles();
  Serial.print("ok ");
  Serial.print(String(angles.first, 8));
  Serial.print(" ");
  Serial.println(String(angles.second, 8));
}

void setup() {
  Serial.begin(::baudRate);
  Serial.setTimeout(10);

  nvmManager.restore<0>(actuators);
  actuators.init();
  actuators.setSpeed(200);


  Serial.println("Plotter ready");
}

void loop() {
  if (Serial.available() > 0) {
    String incomingString = Serial.readStringUntil('\n');
    if (incomingString.startsWith("setspeed")) {
      int speed =
          int(incomingString.substring(String("setSpeed").length() + 1)
                  .toDouble());  // expecting double for future improvements
      actuators.setSpeed(speed);
      Serial.print("s");
      Serial.println(double(speed), 8);

    }    
    ////////////////////////////////////////////////////////////////////////////////////////
    // calibration stuff
    ////////////////////////////////////////////////////////////////////////////////////////
    
    else if (incomingString.startsWith("zeroangles")) {
      actuators.zeroStepState();
      Serial.println("Position zeroed");

    } else if (incomingString.startsWith("calautocal")) {
      // arms need to be level to begin with
      auto result = actuators.calibrateCalibrate();
      nvmManager.persist<0>(actuators);
      Serial.print(result.first, 4);
      Serial.print(" ");
      Serial.println(result.second, 4);

    } else if (incomingString.startsWith("getautocaloffs")) {
      auto offss = actuators.getAutoCalOffsets();
      Serial.print(String(offss.first, 8));
      Serial.print(" ");
      Serial.println(String(offss.second, 8));

    } else if (incomingString.startsWith("autocal")) {
      auto offss = actuators.getAutoCalOffsets();
      const double lOffset =
          getCommandParam<double>(incomingString, "l", offss.first);
      const double rOffset =
          getCommandParam<double>(incomingString, "r", offss.second);
      actuators.setAutoCalibrateOffsets(lOffset, rOffset);
      actuators.autoCalibrate();

      printCurrentAngles();

    }     
    // else if(incomingString.startsWith("setcurrangles ")){

    //   const double leftAngle = getCommandParam<double>(incomingString, "l",
    //   motorLeft.getCurrentAngleDeg());
    //   motorLeft.setCurrentAngleDeg(leftAngle);
    //   const double rightAngle = getCommandParam<double>(incomingString, "r",
    //   motorRight.getCurrentAngleDeg());
    //   motorRight.setCurrentAngleDeg(rightAngle);
    //   Serial.println("Calibration done");

    // }
    
    ////////////////////////////////////////////////////////////////////////////////////////
    // arm movement stuff
    ////////////////////////////////////////////////////////////////////////////////////////

    else if (incomingString.startsWith("getcurrangles")) {
      printCurrentAngles();

    } else if (incomingString.startsWith("saveangles")) {
      nvmManager.persist<0>(actuators);
      printCurrentAngles();

    } else if (incomingString.startsWith("burst")) {
      Serial.println("entered burst mode");
      const uint16_t size = getCommandParam<long>(incomingString, "s", 15);

      uint8_t buffer[size * 4 + 4];
      Serial.readBytes(buffer, size * 4 + 4);

      uint32_t expectedChecksum = static_cast<uint32_t>(buffer[size * 4 + 0]) << 8;
      expectedChecksum = (expectedChecksum + buffer[size * 4 + 1]) << 8;
      expectedChecksum = (expectedChecksum + buffer[size * 4 + 2]) << 8;
      expectedChecksum = expectedChecksum + buffer[size * 4 + 3];

      uint32_t actualChecksum = 0;
      for (uint16_t i = 0; i < size; i++) {
        uint32_t temp = static_cast<uint32_t>(buffer[(i + 1) * 4 - 4]) << 8;
        temp = (temp + buffer[(i + 1) * 4 - 3]) << 8;
        temp = (temp + buffer[(i + 1) * 4 - 2]) << 8;
        temp = temp + buffer[(i + 1) * 4 - 1];
        actualChecksum += temp;
      }

      if (expectedChecksum != actualChecksum) {
        Serial.print("checksum error");
        return;
      }

      for (uint16_t i = 0; i < size; i++) {
        const double ldegrees = buffer[i * 4] + buffer[i * 4 + 1] / 255.0;
        const double rdegrees = buffer[i * 4 + 2] + buffer[i * 4 + 3] / 255.0;

        actuators.moveToDegs(ldegrees, rdegrees);
      }

      printCurrentAngles();

    } else if (incomingString.startsWith("move ") || incomingString.startsWith("moveto")) {

      auto currAngles = actuators.getCurrAngles();
      const double ldegrees =
          getCommandParam<double>(incomingString, "l", currAngles.first);
      const double rdegrees =
          getCommandParam<double>(incomingString, "r", currAngles.second);

      actuators.moveToDegs(ldegrees, rdegrees);

      // return actual angles in degrees (!= requested due to finite stepper
      // resolution)
      printCurrentAngles();

    } else if (incomingString.startsWith("moveby")) {
      const double ldegreesDelta =
          getCommandParam<double>(incomingString, "l", 0.0);
      const double rdegreesDelta =
          getCommandParam<double>(incomingString, "r", 0.0);

      auto currAngles = actuators.getCurrAngles();    

      actuators.moveToDegs(currAngles.first + ldegreesDelta, currAngles.second + rdegreesDelta);

      // return actual angles in degrees (!= requested due to finite stepper
      // resolution)
      printCurrentAngles();

    } 

    ////////////////////////////////////////////////////////////////////////////////////////
    // Pen servo stuff
    ////////////////////////////////////////////////////////////////////////////////////////

    else if (incomingString.startsWith("penup")) {
      actuators.penUp();
      Serial.println("pen is up");

    } else if (incomingString.startsWith("pendown")) {
      actuators.penDown();
      Serial.println("pen is down");

    }else if (incomingString.startsWith("penset")) {
      const int16_t angleDeg = getCommandParam<long>(incomingString, "a", -1);
      if (angleDeg == -1){
        Serial.println("invalid angle");
        return;
      }
      actuators.setPenServoAngle(angleDeg);
      Serial.print("servo angle is ");
      Serial.println(angleDeg);

    } else if (incomingString.startsWith("pensaveasdown")) {
      actuators.penSetAsDown();
      nvmManager.persist<0>(actuators);
      Serial.println("saved");

    } else if (incomingString.startsWith("pengetservoangles")) {
      Serial.print("up ");
      Serial.print(actuators.getPenUpAngle());
      Serial.print(" down ");
      Serial.println(actuators.getPenDownAngle());

    } else if (incomingString.startsWith("pensaveasup")) {
      actuators.penSetAsUp();
      nvmManager.persist<0>(actuators);
      Serial.println("saved");

    } else {
      Serial.print("Invalid command: ");
      Serial.println(incomingString);
    }
  }
}

#endif

#endif