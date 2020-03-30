//
//  main.cpp
//  NDIOutTOP
//
//  Created by Valentin Dufois on 2020-03-26.
//  Copyright © 2020 Derivative. All rights reserved.
//

#ifndef ringbuffer_hpp
#define ringbuffer_hpp

using bytes = unsigned char *;

class RingBuffer
{
public:
	RingBuffer(const int &sizeBytes);
	~RingBuffer();

	/// Puts the specified number of bytes from the given array in the buffer.
	/// If the specified number of bytes is greater than the buffer available
	/// bytes, not all of them will be copied.
	/// @param dataPtr The array holding the bytes to put in the buffer
	/// @param numBytes The number of bytes to insert
	/// @returns The number of bytes effectively written
	int write(bytes dataPtr, int numBytes);

	/// Puts the requested number of bytes in the given array, advancing the
	/// read pointer of the same amount. This method will not read any more
	/// bytes that what the buffer is currently holding
	/// @param dataPtr The receiving array
	/// @param numBytes The maximum number of bytes to copy
	/// @returns The number of bytes read
	int read(bytes dataPtr, int numBytes);

	/// Sets all bytes to 0 and resets the pointers.
	bool clear();

	/// Tell the size of the buffer
	/// @returns The size of the buffer
	inline int getSize() const { return _size; }

	/// Tell how many bytes can be written in the buffer before it gets filled
	/// @returns How many bytes are available for writting
	inline int getWriteAvail() const { return _writeBytesAvail; }

	/// Tell how many bytes can be read from the buffer
	/// @returns How many bytes are available for reading
	inline int getReadAvail() const { return _size - _writeBytesAvail; }

private:
	bytes _data;

	int _size;

	int _readPtr = 0;
	int _writePtr = 0;
	int _writeBytesAvail;
};

#endif /* ringbuffer_hpp */
