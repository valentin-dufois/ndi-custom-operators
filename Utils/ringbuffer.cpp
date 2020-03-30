//
//  main.cpp
//  NDIOutTOP
//
//  Created by Valentin Dufois on 2020-03-26.
//  Copyright © 2020 Derivative. All rights reserved.
//

#include <cstring>

#include "ringbuffer.hpp"
#include "fast_memcpy.h"

RingBuffer::RingBuffer(const int &sizeBytes):
_data(new unsigned char[sizeBytes]),
_size(sizeBytes),
_writeBytesAvail(sizeBytes) {
	std::memset(_data, 0, sizeBytes);
}

RingBuffer::~RingBuffer() {
	delete[] _data;
}

// Set all data to 0 and flag buffer as empty.
bool RingBuffer::clear() {
	std::memset(_data, 0, _size);
	_readPtr = 0;
	_writePtr = 0;
	_writeBytesAvail = _size;
	return true;
}

// Write to the ring buffer.  Do not overwrite data that has not yet
// been read.
int RingBuffer::write(bytes dataPtr, int numBytes) {
	// If there's nothing to write or no room available, we can't write anything.
	if(dataPtr == 0 || numBytes <= 0 || _writeBytesAvail == 0) {
		return 0;
	}

	// Cap our write at the number of bytes available to be written.
	if(numBytes > _writeBytesAvail) {
		numBytes = _writeBytesAvail;
	}

	// Simultaneously keep track of how many bytes we've written and our position in the incoming buffer
	if(numBytes > _size - _writePtr) {
		int len = _size-_writePtr;
		memcpy_fast(_data + _writePtr, dataPtr, len);
		memcpy_fast(_data, dataPtr + len, numBytes - len);
	} else {
		memcpy_fast(_data + _writePtr, dataPtr, numBytes);
	}

	_writePtr = (_writePtr + numBytes) % _size;
	_writeBytesAvail -= numBytes;

	return numBytes;
}

int RingBuffer::read(bytes dataPtr, int numBytes) {
	// If there's nothing to read or no data available, then we can't read anything.
	if(dataPtr == 0 || numBytes <= 0 || _writeBytesAvail == _size) {
		return 0;
	}

	int readBytesAvail = _size - _writeBytesAvail;

	// Cap our read at the number of bytes available to be read.
	if(numBytes > readBytesAvail) {
		numBytes = readBytesAvail;
	}

	// Simultaneously keep track of how many bytes we've read and our position in the outgoing buffer
	if(numBytes > _size - _readPtr) {
		int len = _size - _readPtr;
		memcpy_fast(dataPtr, _data+_readPtr, len);
		memcpy_fast(dataPtr + len, _data, numBytes - len);
	} else {
		memcpy_fast(dataPtr, _data + _readPtr, numBytes);
	}

	_readPtr = (_readPtr + numBytes) % _size;
	_writeBytesAvail += numBytes;

	return numBytes;
}
