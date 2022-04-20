#ifndef _TEST_MAIN_H
#define _TEST_MAIN_H

#line 5 "tests_main.h"

#include <AUnit.h>

#include "nvm_manager.h"

using namespace aunit;

class MyTestOnce : public TestOnce {
 protected:
  
  // mock components ---------------------------------------
  struct AData {
    int val;
    AData() {}
    AData(int val) : val(val) {}
  };

  struct BData {
    int val;
    BData() {}
    BData(int val) : val(val) {}
  };

  class A : public WithNVMData<AData> {
   public:
    using WithNVMData<AData>::WithNVMData;
    using WithNVMData<AData>::nvmData;

    virtual void afterLoadFromNVM() {}
  } a1{1}, a2{3};

  class B : public WithNVMData<BData> {
   public:
    using WithNVMData<BData>::nvmData;
    using WithNVMData<BData>::WithNVMData;

    virtual void afterLoadFromNVM() {}
  } b{2};

  // flash manager mock ---------------------------------------

  template <typename data_type>
  class FlashManagerMock {
   public:
   data_type cache;

    FlashManagerMock() {}

    void put(const data_type& newData) {
      cache = newData;
      // flashManagerPutCalls++;
      // flashManagerCache = newData;
    }

    void get(data_type& dataOut) const { dataOut = cache; }

    const data_type& get() const { return cache; }
  };

  NVMManager<FlashManagerMock, A, B, A> nvmManager{a1, b, a2};

};

testF(MyTestOnce, ctor_init0) {
  a1.nvmData.val = 10;
  nvmManager.restore<0>(a1);
  assertEqual(a1.nvmData.val, 1);
}

testF(MyTestOnce, ctor_init1) {
  b.nvmData.val = 10;
  nvmManager.restore<1>(b);
  assertEqual(b.nvmData.val, 2);
}

testF(MyTestOnce, ctor_init2) {
  a2.nvmData.val = 10;
  nvmManager.restore<2>(a2);
  assertEqual(a2.nvmData.val, 3);
}

testF(MyTestOnce, persist_restore_0) {
  a1.nvmData.val = 10;
  nvmManager.persist<0>(a1);

  A res;
  nvmManager.restore<0>(res);
  assertEqual(res.nvmData.val, 10);

  // others left alone:
  nvmManager.restore<1>(b);
  assertEqual(b.nvmData.val, 2);

  nvmManager.restore<2>(a2);
  assertEqual(a2.nvmData.val, 3);
}

testF(MyTestOnce, persist_restore_1) {
  b.nvmData.val = 10;
  nvmManager.persist<1>(b);
  b.nvmData.val = 0;

  nvmManager.restore<1>(b);
  assertEqual(b.nvmData.val, 10);

  // others left alone:
  nvmManager.restore<0>(a1);
  assertEqual(a1.nvmData.val, 1);

  nvmManager.restore<2>(a2);
  assertEqual(a2.nvmData.val, 3);
}

testF(MyTestOnce, persist_burst) {
  a2.nvmData.val = 12;
  b.nvmData.val = 11;
  nvmManager.persistBurst().persist<2>(a2).persist<1>(b);

  a2.nvmData.val = 0;
  b.nvmData.val = 0;

  nvmManager.restore<1>(b);
  assertEqual(b.nvmData.val, 11);

  nvmManager.restore<2>(a2);
  assertEqual(a2.nvmData.val, 12);
}

//----------------------------------------------------------------------------
// setup() and loop()
//----------------------------------------------------------------------------

void setup() {
  delay(1000);  // wait for stability on some boards to prevent garbage Serial
  Serial.begin(::baudRate);  // ESP8266 default of 74880 not supported on Linux
  while (!Serial)
    ;  // for the Arduino Leonardo/Micro only

  // TestRunner::setVerbosity(Verbosity::kAll);
}

void loop() { TestRunner::run(); }

#endif
