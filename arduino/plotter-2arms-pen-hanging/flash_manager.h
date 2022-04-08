#ifdef __LGT8FX8P__

namespace flash{

  uint32_t read32(uint16_t address)
  {
    uint32_t dwTmp;

    EEARL = address & 0xff;
    EEARH = (address >> 8) & 0x3;
    
    EECR |= (1 << EERE);
    
    __asm__ __volatile__ ("nop" ::);
    __asm__ __volatile__ ("nop" ::);
    
    dwTmp = E2PD0;
    dwTmp |= ((uint32_t)E2PD1 << 8);
    dwTmp |= ((uint32_t)E2PD2 << 16);
    dwTmp |= ((uint32_t)E2PD3 << 24);

    // return data from data register
    return dwTmp; 
  }

  void write32(uint16_t address, uint32_t value)
  {
    uint8_t __bk_sreg = SREG;

    EEARL = 1;
    EEDR = value >> 8;
    EEARL = 2;
    EEDR = value >> 16;
    EEARL = 3;
    EEDR = value >> 24;
    EEARL = 0;
    EEDR = value;
    
    EEARH = address >> 8;
    EEARL = address & 0xfc;

    // Program Mode
    cli();
    EECR = 0x44;
    EECR |= 0x42;
    
    SREG = __bk_sreg; 
  }

  template<typename T>
  void write(const T& data, uint16_t address)
  {
    static_assert(sizeof(T) % 4 == 0, "");
    const uint32_t *p = reinterpret_cast<const uint32_t*>(&data);

    uint8_t len = sizeof(T) / 4;

    for(uint8_t i = 0; i < len; i++) {
        write32(address + 4*i, *p++);
    }
  }

  template<typename T>
  void read(T& data, uint16_t address)
  {
    static_assert(sizeof(T) % 4 == 0, "");
    uint8_t len = sizeof(T) / 4;
    uint32_t *p = reinterpret_cast<uint32_t*>(&data);

    for(uint8_t i = 0; i < len; i++) {
        *p++ = read32(address + 4*i);
    }
  }

  template<typename T>
  void rawRead(T& data, uint16_t address){
    static_assert(sizeof(T) % 4 == 0, "");
    uint8_t len = sizeof(T) / 4;
    uint32_t *source = reinterpret_cast<uint32_t*>(address);
    uint32_t *target = reinterpret_cast<uint32_t*>(&data);

    for(uint8_t i = 0; i < len; i++) {
        *target++ =  *source++;
    }
  }

  void erase(){
    EECR = 0x84;
    bitSet(EECR, EEPE);
    EECR = 0X40;
  }
}

template<typename data_type>
class FlashManager{
  struct FlashDataType{
    data_type data;  // todo: look into how to pack
    uint8_t _padding[(4 - sizeof(data_type) % 4) % 4];
    uint32_t isLast {0xffffffff};

    FlashDataType(){}
    FlashDataType(const data_type& val):data(val){}
  };

  static constexpr uint16_t highestAddress = 1019;
  static constexpr uint16_t blocksPerPage = (highestAddress + 1 - sizeof(uint32_t)) / sizeof(FlashDataType);

  FlashDataType currRecord;

  uint16_t currBlockNum{0};

  uint16_t addressOfBlock(uint16_t blockNumber){
    return sizeof(uint32_t) + sizeof(FlashDataType)*blockNumber;
  }

public:
  void putFirst(const data_type& firstRecord){
    flash::erase();
    currBlockNum = 0;
    flash::write(FlashDataType(firstRecord), addressOfBlock(currBlockNum));
    flash::write32(0, 0);
    currRecord.data = firstRecord;
  }

 public:

  FlashManager(){
    ECCR = 0x80; ECCR = 0x4C;

    uint32_t lowerPageStart = 47104;
    uint32_t upperPageStart = lowerPageStart + 1024;

    uint8_t lFlag =(*((volatile unsigned char *)upperPageStart - 2));
    uint8_t rFlag = (*((volatile unsigned char *)upperPageStart + 1022));
    uint16_t activePageBegin; 
    uint32_t swapPageBegin;
    if((lFlag == 1 && rFlag == 0) || (lFlag == 3 && rFlag == 2)){
      activePageBegin = lowerPageStart;  
      swapPageBegin = upperPageStart;
    }else{
      activePageBegin = upperPageStart;
      swapPageBegin = lowerPageStart;
    }

    if((*((volatile unsigned char *)activePageBegin)) != 0x00){ // last page swap was interrupted
      if((*((volatile unsigned char *)swapPageBegin)) != 0x00){ // whole eprom is empty
        putFirst(data_type());
      }else{  // recover data from the end of the swap page
        currBlockNum = 0;
        flash::rawRead(currRecord, swapPageBegin + addressOfBlock(blocksPerPage - 1));
        // or... but this may be bad -> currRecord = (*((FlashDataType *)(swapPageBegin + addressOfBlock(blocksPerPage - 1)))); // load data from the end of the swap page
        flash::write(FlashDataType(currRecord.data), addressOfBlock(currBlockNum));
        flash::write32(0, 0);
      }
    }else{ // seek last record
      currBlockNum = 0;
      flash::read(currRecord, addressOfBlock(0));
      while(!(bool)currRecord.isLast && currBlockNum < blocksPerPage){
        currBlockNum ++;
        flash::read(currRecord, addressOfBlock(currBlockNum));
      }
      if(currBlockNum == blocksPerPage){
        //todo: flash is corrupt
      }
    }
  }

  FlashManager(const data_type& initialRecord){
    ECCR = 0x80; ECCR = 0x4C; 

    putFirst(initialRecord);
  }

  void put(const data_type& newData){
    currBlockNum ++;
    if(currBlockNum >= blocksPerPage){
      putFirst(newData);
    }else{
      flash::write(FlashDataType(newData), addressOfBlock(currBlockNum));
      flash::write32(addressOfBlock(currBlockNum) - 4, 0);
      currRecord.data = newData;
    }
  }

  void get(data_type& dataOut) const {
    dataOut = currRecord.data;
  }

  const data_type& get() const {
    return currRecord.data;
  }
};

#else

#include <EEPROM.h>

template<typename data_type>
class FlashManager{
  data_type cache;
 public:
  FlashManager(){
    EEPROM.get(0, &cache);
    // todo: what if eeprom empty... (first use)
  }

  void put(const data_type& newData){
    EEPROM.put(0, &newData);
    cache = newData;
  }

  void get(data_type& dataOut) const {
    dataOut = cache;
  }

  const data_type& get() const {
    return cache;
  }
};
#endif 