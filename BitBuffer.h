#ifndef ByteBuffer_h
#define ByteBuffer_h

#if defined(ARDUINO) && ARDUINO >= 100
#include "Arduino.h"
#else
#include "WProgram.h"
#endif

class BitBuffer
{
	public:
		// ##### static constANTS #####
		// static constant for ranges defining the maximum number of distinct values to be stored
		static const byte RANGE2;
		static const byte RANGE4;
		static const byte RANGE8;
		static const byte RANGE16;
		static const byte RANGE32;
		static const byte RANGE64;
		static const byte RANGE128;
		static const byte RANGE256;
		static const byte RANGE512;
		static const byte RANGE1024;
		static const byte RANGE2048;
		static const byte RANGE4096;
		static const byte RANGE8192;
		static const byte RANGE16384;
		static const byte RANGE32768;

		// static constants for overflow state
		static const byte OVERFLOW_MAX;
		static const byte OVERFLOW_MIN;
		static const byte OVERFLOW_SKIP;
	
	
		// ##### static constRUCTOR #####
		BitBuffer(byte p_range, unsigned int p_size);
		
		// ##### METHODS #####
		/*
		 * Resets buffer instance and frees memory
		 */
		void flush();
		
		/*
		 * Overflow handling
		 * Defines the behavior in case the defined values is beyond the defined range
		 * OVERFLOW_MAX - the maximum value for this range will be written
		 * OVERFLOW_MIN - zero will be written
		 * OVERFLOW_SKIP - value will not be stored
		 */
		byte getOverflowState();
		void setOverflowState(byte p_overflow);
		
		// returns capacity of values that can be stored in buffer for defined range
		unsigned int getSize();
		
		// returns the number of values currently stored in buffer
		unsigned int getValueCount();
		
		/*
		 * central methods for buffer to fill and retrieve values.
		 * buffer will act like a FIFO, replacing old values once capacity of buffer was reached. calling
		 * pop will remove retrieved value from buffer and free up on index.
		 * internal handling will ensure that values are mapped to defined range, if value does not
		 * correspond to defined range it will be mapped or skipped according to define overflow state.
		 * 
		 * returns: whether action could be performed successfully / first value in buffer
		 */
		boolean push(unsigned int p_value);
		unsigned int pop();
		
		/*
		 * Returns the specified index in the buffer without deleting it.
		 * The index does not correspond with the internal bit-Index nor the array-Index but represents the
		 * index in a FIFO. If for example one more value was added than the capacity of the buffer and the first
		 * index is requested this would be the second one added to the buffer, with the first value being already 
		 * overwritten.
		 *
		 * p_index: index in FIFO starting with 1
		 */
		unsigned int getValue(unsigned p_index);
		
		//DEBUG
		void runTest();
		
		//DEBUG
		/*
		 * method for tracing behavior of buffer. This will print out all previously added values in a 
		 * string to Serial.
		 */
		void printContent2Serial();
		
	private:
		// ###### VARIABLES #####
		byte s_range; //value range
		byte s_overflow; //overflow behaviour
		byte* s_data; //dataset array
		unsigned long s_bitIndex; //location index for next write on bitlevel
		unsigned int s_popCount; //number of values currently retrieved from buffer
		unsigned int s_size; //capacity of values that can be stored in buffer for defined range
		unsigned int s_bitSize; //number of bits per value for defined range
		boolean s_full; //keep state whether first overrun of FIFO happened already

		// ##### METHODS #####
		// returns the maximum value that can be stored in buffer for defined range.
		unsigned int getMaxRangeValue();
		unsigned int getMaxRangeValue(byte p_range);
		
		/*
		 * returns size of values in bits for defined range
		 * value range defines the number of bits required, as range defines the maximum value we have to calclate the exponent
		 * example range = 0x07 that corresponds to 2^X=8; the exponent X is what we are looking for
		 * math.h does not define a logarithm to the base of two, http://stackoverflow.com/questions/758001/log2-not-found-in-my-math-h
		 */
		unsigned int getBitSize();
		
		// returns the size of the byte array for internal calculation
		unsigned int getArraySize();

		/*
		 * Returns the value for the defined bitIndex
		 */
		unsigned int getValueInternal(unsigned long p_bitIndex);
		
		//performs a transformation of two byte into an integer
		unsigned int getIntegerValue(byte highByte, byte lowByte);
};

#endif
