/*
 *	Arduino BitBuffer
 *	a library to storing a large number of values within a certain range by keeping footprint in memory as small as possible.
 *	Memory footprint reduction is achieved by defining a range for values and by only using up the space required for the defined range,
 *	by performing internal bit shifting operations. BitBuffer API gives you a FIFO-based interface for which you don't have to care about
 *	internal representation. Only thing you have to do is to define the range and the number of values kept for FIFO. Once this is done,
 *	you can push values and pop them at a later point in time. Also index based access is possible with value remaining in store.
 *
 *	Version 0.1
 *	Open agenda items:
 *		- reflect instance parameters in buffer itself and use dynamic range for those (e.g. bitIndex instead of predefined long) to
 *			reduce memory footprint
 *		- also represent local variables in buffer to avoid Arduino memory clustering (Bjoern)
 *		- define DEBUG statements via macro and separate into different debug level (Bjoern)
 *		- minimize number of local variables (e.g. already identified marked with TODO)
 *		- decide on license for publishing library
 *		- remove DEBUG code sections for final release
 *		- remove two line debug lines with one lines, e.g. printf("Label: %s\n\r", string)
 *		- switch to cross-platform type due to different type representation for UNO and DUE, e.g. use (u)int8_t instead of byte
 *
 *	Temporary license until newer version of the library with updated license is released:
 *		You might use this coding and redistribute it. It's currently considered an alpha version - be aware of that even though I tried already
 *		to run a couple of test cases. If you come across a bug or proposing any feature you are obliged to share it with the community by sending
 *		your proposal to the owner michab-ma<HERE_COMES_THE_AT>web<HERE_COMES_THE_PERIOD>de. If your project turns into something productive I would
 *		be eager to hear from you. Not wanting to hear the details of your secret project but would love to get some feedback. If you don't do, you
 *		owe me a beer with some full-flegded dinner in a restaurant of my choice. I would recommend you to avoid it, it will be rather costly... :)
 *
 *	Thanks to Sigurður Örn Aðalgeirsson (siggi@media.mit.edu) for his ByteBuffer project and the reference it gave.
 */

#if defined(ARDUINO) && ARDUINO >= 100
#include "Arduino.h"
#else
#include "WProgram.h"
#endif

#include "BitBuffer.h"
#include "math.h"

// constant for ranges defining the maximum number of distinct values to be stored
const byte BitBuffer::RANGE2 = 0x01;
const byte BitBuffer::RANGE4 = 0x03;
const byte BitBuffer::RANGE8 = 0x07;
const byte BitBuffer::RANGE16 = 0x0F;
const byte BitBuffer::RANGE32 = 0x1F;
const byte BitBuffer::RANGE64 = 0x3F;
const byte BitBuffer::RANGE128 = 0x7F;
const byte BitBuffer::RANGE256 = 0xFF;
const byte BitBuffer::RANGE512 = 0x02;
const byte BitBuffer::RANGE1024 = 0x06;
const byte BitBuffer::RANGE2048 = 0x0e;
const byte BitBuffer::RANGE4096 = 0x1E;
const byte BitBuffer::RANGE8192 = 0x3E;
const byte BitBuffer::RANGE16384 = 0x7E;
const byte BitBuffer::RANGE32768 = 0xFE;

// constants for overflow state
const byte BitBuffer::OVERFLOW_MAX = 0x01;
const byte BitBuffer::OVERFLOW_MIN = 0x02;
const byte BitBuffer::OVERFLOW_SKIP = 0x03;

byte s_range; //value range
byte s_overflow; //overflow behaviour
byte* s_data; //dataset array
unsigned long s_bitIndex; //location index for next write on bitlevel
unsigned int s_popCount; //number of values currently retrieved from buffer
unsigned int s_size; //capacity of values that can be stored in buffer for defined range
unsigned int s_bitSize; //number of bits per value for defined range
boolean s_full; //keep state whether first overrun of FIFO happened already

/*
 * Constructor
 * p_range - defines the max values to be stored in the buffer, use the public constants
 * p_size - defines the number of entries in this FIFO store before data will be overwritten
 */
BitBuffer::BitBuffer(byte p_range, unsigned int p_size) {
  //DEBUG
  /*
  Serial.begin(9600);
  */
  
  //initialize members
  s_overflow = BitBuffer::OVERFLOW_SKIP;
  s_bitSize = 0;
  s_bitIndex = 0;
  s_popCount = 0;
  s_range = p_range;
  s_size = p_size;
  s_full = false;
  
  s_data = (byte*)malloc(getArraySize());
  
  //DEBUG
  /*
  Serial.print("Array size: ");
  Serial.println(getArraySize());
  */
}

/*
 * Resets buffer instance and frees memory
 */
void BitBuffer::flush() {
  free(s_data);
}

/*
 * Overflow handling
 * Defines the behavior in case the defined values is beyond the defined range
 * OVERFLOW_MAX - the maximum value for this range will be written
 * OVERFLOW_MIN - zero will be written
 * OVERFLOW_SKIP - value will not be stored
 */
byte BitBuffer::getOverflowState() {
  return s_overflow;
}

void BitBuffer::setOverflowState(byte p_overflow) {
  s_overflow = p_overflow;
}

// returns capacity of values that can be stored in buffer for defined range
unsigned int BitBuffer::getSize() {
	return s_size;
}

// returns the number of values currently stored in buffer
unsigned int BitBuffer::getValueCount() {
	return (s_full ? s_size : s_bitIndex / getBitSize()) - s_popCount;
}

boolean BitBuffer::push(unsigned int p_value) {
  // check if value is within defined range
  if(p_value > getMaxRangeValue())
  {
    if(s_overflow == OVERFLOW_MAX)
      p_value = getMaxRangeValue();
    else if(s_overflow == OVERFLOW_MIN)
      p_value = 0;
    else
      return false;
  }
  
  //check if we reached end of capacity and overwrite old values
  //as we have a defined size it might happen that there are still bits in the array left that will not be used
  if(s_bitIndex + getBitSize() > getSize() * getBitSize())
  {
	  s_bitIndex = 0;
	  s_full = true;
  }
  
  //calculate position in byte array
  int index = (int) (s_bitIndex / 8);
  int offset = s_bitIndex % 8;
  
  //DEBUG
  /*
  Serial.print("Index for push: ");
  Serial.println(index);
  */
  
  byte *arrayValue = &s_data[index];
  byte mask = 0xFF;
  byte valueLow = ((byte *)&p_value)[0];
  
  if(getMaxRangeValue() <= RANGE256)
  {    
    //check if we are still in the same array index if we add the value
    if(offset + getBitSize() <= 8)
    {	
      // bits new value to be added need to be shifted to the corresponding place, incorporating value already in dataset and size of value 
	  valueLow = valueLow << (8 - offset - getBitSize());
	  //create a mask that corresponds with the fields taken by value filled with 1
	  mask = mask << (8 - getBitSize());
	  mask = mask >> offset;
	  
      //DEBUG
	  /*
	  Serial.print("Mask non-inverted: ");
	  Serial.println(mask);
	  Serial.print("Mask inverted: ");
	  Serial.println(0xFF - mask);	  
	  Serial.print("Array value before: ");
	  Serial.println(*arrayValue);
	  */
	  
      // value stored before: 11000000, value new: 00001111, value shifted: 00111100, after storage: 11111100
      // ensure that bits at location that value will be put to are reverted beforehand by using inverted mask
	  *arrayValue = (*arrayValue & (0xFF - mask)) | valueLow;
	  
      //DEBUG
	  /*
	  Serial.print("Array value after: ");
	  Serial.println(*arrayValue);
	  */
    }
    else //<256 but beyond current byte for storing at current position
    {
		//TODO instead of two byte (v1, v2) overwrite v1
      // value stored before: 11111000, value new: 00001111, value shifted1: 00000111, after storage1: 11111111, value shifted2/after storage2: 10000000
      int overlength = offset + getBitSize() - 8;
      byte v1 = valueLow >> overlength;
	  mask = mask << (8 - getBitSize());
	  mask = mask >> (8 - getBitSize() + overlength);
	  
	  //DEBUG
	  /*
	  Serial.print("Mask non-inverted: ");
	  Serial.println(mask);
	  Serial.print("Mask inverted: ");
	  Serial.println(0xFF - mask);	  
	  Serial.print("Value1: ");
	  Serial.println(v1);
	  */
	  
      *arrayValue = (*arrayValue & (0xFF - mask)) | v1;
	  
	  byte v2 = valueLow << (8 - overlength);
      arrayValue = &s_data[index + 1];
	  mask = 0xFF << (8 - overlength);
	  
	  //DEBUG
	  /*
	  Serial.print("Mask non-inverted: ");
	  Serial.println(mask);
	  Serial.print("Mask inverted: ");
	  Serial.println(0xFF - mask);	  
	  Serial.print("Value2: ");
	  Serial.println(v2);
	  */
	  
      *arrayValue = (*arrayValue & (0xFF - mask)) | v2;
    }
  }
  else //>255
  {
	  // check whether value fits into current byte and next one
	  if(offset + getBitSize() <= 16)
	  {
		  //TODO instead of two byte (v1, v2) overwrite v1
		  // value stored before: 11100000|00000000, value new: 00000111|11111111, value shifted(v1|v2): 00011111|11111100, after storage: 11111111|11111111
		  int overlength = offset + getBitSize() - 8;
		  unsigned int valueIntShifted = p_value << (8 - overlength);
		  byte v1 = ((byte *)&valueIntShifted)[1];
		  
		  mask = mask << 8 - (getBitSize() - 8) - (8 - overlength); //TODO to be replaced by 8 - offset???
		  mask = mask >> 8 - (getBitSize() - 8) - (8 - overlength);
		  
		  //DEBUG
		  /*
		  Serial.print("Mask non-inverted: ");
		  Serial.println(mask);
		  Serial.print("Value1: ");
		  Serial.println(v1);
		  */
		  
		  *arrayValue = (*arrayValue & (0xFF - mask)) | v1;
		  
		  byte v2 = ((byte *)&valueIntShifted)[0];
		  arrayValue = &s_data[index + 1];
		  mask = 0xFF << (8 - overlength);
		  
		  //DEBUG
		  /*
		  Serial.print("Mask non-inverted: ");
		  Serial.println(mask);	  
		  Serial.print("Value2: ");
		  Serial.println(v2);
		  */
		  
		  *arrayValue = (*arrayValue & (0xFF - mask)) | v2;
	  }
	  else //>255 and value needs more space than the current and next byte
	  {
		  //TODO instead of two byte (v1, v2) overwrite v1
		  int overlength = offset + getBitSize() - 16;
		  
		  //TODO TEST		  
		  byte v1 = p_value >> getBitSize() - (8 - offset);
		  
		  mask = mask << offset;
		  mask = mask >> offset;

		  //DEBUG
		  /*
		  Serial.print("Mask non-inverted: ");
		  Serial.println(mask);
		  Serial.print("Value1: ");
		  Serial.println(v1);
		  */
		  
		  *arrayValue = (*arrayValue & (0xFF - mask)) | v1;
		  
		  unsigned int valueIntShifted = p_value << (8 - offset) + (16 - getBitSize());
		  byte v2 = ((byte *)&valueIntShifted)[1];
		  mask = 0xFF;

		  //DEBUG
		  /*
		  Serial.print("Value shifted (midByte, lowByte): ");
		  Serial.println(valueIntShifted);
		  Serial.print("Mask non-inverted: ");
		  Serial.println(mask);	  
		  Serial.print("Value2: ");
		  Serial.println(v2);
		  */
		  
		  arrayValue = &s_data[index + 1];
		  *arrayValue = v2;
		  
		  byte v3 = ((byte *)&valueIntShifted)[0];
		  mask = 0xFF << 8 - overlength;
		  
		  //DEBUG
		  /*
		  Serial.print("Mask non-inverted: ");
		  Serial.println(mask);	  
		  Serial.print("Value3: ");
		  Serial.println(v3);
		  */
		  
		  arrayValue = &s_data[index + 2];
		  *arrayValue = (*arrayValue & (0xFF - mask)) | v3;
	  }	
  }  
  
  s_bitIndex = s_bitIndex + getBitSize();  
  
  if(s_popCount > 0) {
	  s_popCount--;
	  
	  //DEBUG
	  /*
	  Serial.print("Decreased popCounter: ");
	  Serial.println(s_popCount);
	  */
  }
  
  return true;
} //END push

unsigned int BitBuffer::pop() {
	unsigned int ret;
	
	//check whether any values in buffer left, if not we do not have a sufficient criteria to return error so we return 0
	if(getValueCount() <= 0)
		return 0;
	
	ret = getValue(1);
	
	s_popCount++;
	
	//DEBUG
	/*
	Serial.print("Increased popCounter: ");
	Serial.println(s_popCount);
	*/
	
	return ret;
} //END pop

/*
 * Returns the specified index in the buffer without deleting it.
 * The index does not correspond with the internal bit-Index nor the array-Index but represents the
 * index in a FIFO. If for example one more value was added than the capacity of the buffer and the first
 * index is requested this would be the second one added to the buffer, with the first value being already 
 * overwritten.
 *
 * p_index: index in FIFO starting with 1
 * returns: value at specified index or 0 in case of invalid index
 */
unsigned int BitBuffer::getValue(unsigned p_index) {
	//check whether index is currently filled in buffer
	if(getValueCount() < p_index || p_index < 1)
		return 0;
	
	unsigned long bitDelta = (getValueCount() - p_index + 1) * getBitSize();
	unsigned long bitIndex;
	
	// check if we have to take the value from the end of the buffer
	if(bitDelta <= s_bitIndex)
		bitIndex = s_bitIndex - bitDelta;
	else
		bitIndex = getSize() * getBitSize() - bitDelta + s_bitIndex;
	
	//DEBUG
	/*
	Serial.print("Value count: ");
	Serial.println(getValueCount());
	Serial.print("BitDelta: ");
	Serial.println(bitDelta);
	Serial.print("Current BitIndex: ");
	Serial.println(s_bitIndex);
	Serial.print("Value BitIndex: ");
	Serial.println(bitIndex);
	*/	
	
	return getValueInternal(bitIndex);	
} //END getValue

//DEBUG
//TODO how to setup static method??
void BitBuffer::runTest() {
	//TODO do this for all ranges
	for(int k = 0; k < 15; k++)
	{
		byte range;
		unsigned int maxVal;
		
		switch(k) {
			case 0:
				range = BitBuffer::RANGE2;
				maxVal = 1;
				break;
			case 1:
				range = BitBuffer::RANGE4;
				maxVal = 3;
				break;
			case 2:
				range = BitBuffer::RANGE8;
				maxVal = 7;
				break;
			case 3:
				range = BitBuffer::RANGE16;
				maxVal = 15;
				break;
			case 4:
				range = BitBuffer::RANGE32;
				maxVal = 31;
				break;
			case 5:
				range = BitBuffer::RANGE64;
				maxVal = 63;
				break;
			case 6:
				range = BitBuffer::RANGE128;
				maxVal = 127;
				break;
			case 7:
				range = BitBuffer::RANGE256;
				maxVal = 255;
				break;
			case 8:
				range = BitBuffer::RANGE512;
				maxVal = 511;
				break;
			case 9:
				range = BitBuffer::RANGE1024;
				maxVal = 1023;
				break;
			case 10:
				range = BitBuffer::RANGE2048;
				maxVal = 2047;
				break;
			case 11:
				range = BitBuffer::RANGE4096;
				maxVal = 4095;
				break;
			case 12:
				range = BitBuffer::RANGE8192;
				maxVal = 8191;
				break;
			case 13:
				range = BitBuffer::RANGE16384;
				maxVal = 16383;
				break;
			case 14:
				range = BitBuffer::RANGE32768;
				maxVal = 32767;
				break;
		}
		
		int capacity = random(18);
		unsigned int value;
		int rand;
		
		BitBuffer buffer(range, capacity);
		buffer.setOverflowState(BitBuffer::OVERFLOW_SKIP);
		Serial.println("\n--------------------------------------");
		Serial.print("New buffer created (maxValue, capacityBuffer): ");
		Serial.print(maxVal);
		Serial.print(", ");
		Serial.println(capacity);
		buffer.printContent2Serial();
		//test filling values within range and capacity
		Serial.print("Filling array: ");
		rand = (int) capacity + random(capacity);
		Serial.print(rand);
		Serial.print(" -");
		for(int i = 0; i < rand; i++)
		{
			Serial.print(" ");
			Serial.print(maxVal - i % (maxVal + 1));
			buffer.push(maxVal - i % (maxVal + 1));
		}
		Serial.println("");
		buffer.printContent2Serial();
		Serial.print("Pop value: ");
		rand = random(capacity);
		Serial.print(rand);
		Serial.print(" -");
		for(int i = 0; i < rand; i++)
		{
			value = buffer.pop();
			Serial.print(" ");
			Serial.print(value);
		}
		Serial.println("");
		buffer.printContent2Serial();
		Serial.print("Adding: ");
		rand = random(capacity);
		Serial.print(rand);
		Serial.print(" -");
		for(int i = 0; i < rand; i++)
		{
			Serial.print(" ");
			Serial.print(i % (maxVal + 1));
			buffer.push(i % (maxVal + 1));
		}
		Serial.println("");
		buffer.printContent2Serial();
	}
}


//DEBUG
void BitBuffer::printContent2Serial() {  
	//get bitIndex of first existing entry to calculate range of not-popped entries
	unsigned long bitDelta = (getValueCount()) * getBitSize();
	unsigned long initialBitIndex;

	// check if we have to take the value from the end of the buffer
	if(bitDelta < s_bitIndex)
		initialBitIndex = s_bitIndex - bitDelta;
	else
		initialBitIndex = getSize() * getBitSize() - bitDelta + s_bitIndex;
	
	//DEBUG
	/*
	Serial.print("BitSize: ");
	Serial.println(getBitSize());
	Serial.print("BitIndex: ");
	Serial.println(s_bitIndex);
	Serial.print("Max BitIndex: ");
	Serial.println(getArraySize() * 8);
	Serial.print("Filled: ");
	Serial.println(s_full);
	Serial.print("First index excl. popped: ");
	Serial.println(initialBitIndex);
	*/
	
	Serial.print("[");
  
	for(unsigned long bitIndexCounter = 0; bitIndexCounter + getBitSize() - 1 < (long) (s_full ? getSize() * getBitSize() : s_bitIndex); bitIndexCounter += getBitSize())
	{
		Serial.print(" ");
		  
		Serial.print(getValueInternal(bitIndexCounter));

		Serial.print("{");
		Serial.print(bitIndexCounter);
		
		if(initialBitIndex == s_bitIndex && s_popCount > 0)
		{
			Serial.print("P");
		}
		else if(initialBitIndex < s_bitIndex)
		{
			if(bitIndexCounter < initialBitIndex || bitIndexCounter >= s_bitIndex)
				Serial.print("P");
		}
		else
		{
			if(bitIndexCounter >= s_bitIndex && bitIndexCounter < initialBitIndex)
				Serial.print("P");
		}
		
		if(bitIndexCounter == s_bitIndex || (s_bitIndex == getBitSize() * getSize() && bitIndexCounter == 0))
			Serial.print("N");

		Serial.print("}");
	}

	Serial.println("]");
} //END printContent2Serial



/*####################################
 *      INTERNAL PROCESSING METHODS
 *####################################
 */
unsigned int BitBuffer::getMaxRangeValue() {
  return getMaxRangeValue(s_range);
}
 
unsigned int BitBuffer::getMaxRangeValue(byte p_range) {
  unsigned int ret;
  byte *p = (byte *)&ret;
  
  //for all values <255 byte representation represent integer value
  //we use a trick for value >255 by using index 0 as an indicator, if 0 values to the left represent the second byte
  if(bitRead(p_range, 0) == 1)
  {
    p[0] = p_range;
    p[1] = 0x00;
  }
  else
  {
    p_range = p_range >> 1;
    p[0] = 0xFF;
    p[1] = p_range;
  }
  
  return ret;
}

/*
 * returns size of values in bits for defined range
 * value range defines the number of bits required, as range defines the maximum value we have to calclate the exponent
 * example range = 0x07 that corresponds to 2^X=8; the exponent X is what we are looking for
 * math.h does not define a logarithm to the base of two, http://stackoverflow.com/questions/758001/log2-not-found-in-my-math-h
 */
unsigned int BitBuffer::getBitSize() {
  if(s_bitSize < 1) {
	  if(getMaxRangeValue() > 1)
		  s_bitSize = round(log(getMaxRangeValue()) / M_LN2);
	  else
		  s_bitSize = 1;
  }
  return s_bitSize;
}

// returns the size of the byte array for internal calculation
unsigned int BitBuffer::getArraySize() {
  //size of the array is calculated by value range multiplied by requested number of values
  return ((int) (getBitSize() * getSize() / 8)) + 1;
}

/*
 * Returns the value for the defined bitIndex
 */
unsigned int BitBuffer::getValueInternal(unsigned long p_bitIndex) {
	int offset;
	byte mask;
    byte *arrayValue = &s_data[((int) p_bitIndex / 8)];
    offset = p_bitIndex % 8;
    
	//check if value can be retrieved from within the current array value
    if(offset + getBitSize() <= 8)
    {
      mask = 0xFF << (8 - getBitSize());
      mask = mask >> offset;
	  
	  //DEBUG
	  /*
	  Serial.println("");
	  Serial.print("mask: ");
	  Serial.println(mask);
	  Serial.print("Array value: ");
	  Serial.println(*arrayValue);
      */
	  
      byte v1 = *arrayValue & mask;
	  v1 = v1 >> (8 - offset - getBitSize());
	  
	  return getIntegerValue(0x00, v1); 
    }
	//values that are distributed over current and next byte but not more
    else if(offset + getBitSize() <= 16)
    {
      //value stored1/mask1: 00000011, value stored2/mask2: 11000000
	  int overlength = offset + getBitSize() - 8;
      mask = 0xFF << (8 - getBitSize() + overlength);
      mask = mask >> offset;
      byte valueHigh = *arrayValue & mask;
      
	  //DEBUG
	  /*
	  Serial.print("mask: ");
	  Serial.println(mask);
	  Serial.print("Array value1: ");
	  Serial.println(*arrayValue);
	  */
	  
      mask = 0xFF << (8 - overlength);
      arrayValue = &s_data[((int) p_bitIndex / 8 + 1)];
      byte valueLow = *arrayValue & mask;

	  //DEBUG
	  /*
	  Serial.print("mask: ");
	  Serial.println(mask);
	  Serial.print("Array value2: ");
	  Serial.println(*arrayValue);
	  */
	  
	  if(getMaxRangeValue() <= RANGE256)
	  { 
		  //valueHigh: 00000011, valueLow: 11000000, valueHigh': 00001100, valueLow': 00000011, valueHigh'&valueLow': 00001111
		  valueHigh = valueHigh << overlength;
		  valueLow = valueLow >> (8 - overlength);
		  
		  //DEBUG
		  /*
		  Serial.print("Value1 shifted: ");
		  Serial.println(valueHigh);
		  Serial.print("Value2 shifted: ");
		  Serial.println(valueLow);
		  */
		  
		  return getIntegerValue(0x00, valueHigh | valueLow);
	  }
	  else //>255 and spread across 2 bytes
	  {
		  // value stored before: 00011111|11111100, return value: 00000111|11111111
		  unsigned int valueIntShifted = getIntegerValue(valueHigh, valueLow);
		  
		  //DEBUG
		  /*
		  Serial.print("Value before shift: ");
		  Serial.println(valueIntShifted);
		  */
		  
		  valueIntShifted = valueIntShifted >> (16 - (offset + getBitSize()));

		  //DEBUG
		  /*
		  Serial.print("Value1 shifted: ");
		  Serial.println(((byte *) &valueIntShifted)[1]);
		  Serial.print("Value2 shifted: ");
		  Serial.println(((byte *) &valueIntShifted)[0]);
		  */
		  
		  return valueIntShifted;
	  }
    }
	//>255 and spread across 3 bytes
	else
	{
		int overlength = offset + getBitSize() - 16;
		unsigned long valueLongShifted = 0;
		((byte *) &valueLongShifted)[2] = *arrayValue;
		((byte *) &valueLongShifted)[1] = s_data[((int) p_bitIndex / 8 + 1)];
		((byte *) &valueLongShifted)[0] = s_data[((int) p_bitIndex / 8 + 2)];
		
		//DEBUG
		/*
		Serial.print("Array value1: ");
		Serial.println(*arrayValue);
		Serial.print("Array value2: ");
		Serial.println(s_data[((int) p_bitIndex / 8 + 1)]);
		Serial.print("Array value3: ");
		Serial.println(s_data[((int) p_bitIndex / 8 + 2)]);
		Serial.print("Value before shift: ");
		Serial.println(valueLongShifted);
		*/
		
		valueLongShifted = valueLongShifted << 8 + offset;
		valueLongShifted = valueLongShifted >> 8 + offset + (8 - overlength);
		
		//DEBUG
		/*
		Serial.print("Value1 shifted: ");
		Serial.println(((byte *) &valueLongShifted)[1]);
		Serial.print("Value2 shifted: ");
		Serial.println(((byte *) &valueLongShifted)[0]);
		*/
		
		return ((unsigned int) valueLongShifted);
	}
} //END getValueInternal(bitIndex)

//performs a transformation of two byte into an integer
unsigned int BitBuffer::getIntegerValue(byte highByte, byte lowByte) {
	int ret;
    byte *p = (byte *)&ret;
	
	p[1] = highByte;
	p[0] = lowByte;
	
	return ret;
}
