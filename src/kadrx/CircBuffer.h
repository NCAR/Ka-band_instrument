////////////////////////////////////////////////////////////////////
// CircBuffer.hh
//
// Circular buffer class
//
// Mike Dixon, EOL, NCAR, POBox 3000, Boulder, CO, 80307, USA
//
// Dec 2010
//
////////////////////////////////////////////////////////////////////
//
// CircBuffer implementation details:
//
////////////////////////////////////////////////////////////////////

#ifndef CIRC_BUFFER_H_
#define CIRC_BUFFER_H_

#include <cstdio>
#include <boost/thread/mutex.hpp>
using namespace std;

template <class T>
class CircBuffer
{
public:

  // constructor

  CircBuffer(size_t size);

  // destructor

  ~CircBuffer();

  // copy constructor

  CircBuffer(const CircBuffer &rhs);

  // assignment (operator =)
  
  CircBuffer & operator=(const CircBuffer &rhs);
  
  // get size

  inline size_t size() const { return _size; }

  // Write (insert) an element to the buffer.
  // You pass in a pointer to the object you want to write.
  // The method returns a pointer to the replaced object. This
  // should be re-used for the next write.
  // The caller is responsible for allocating the element passed
  // in on the initial write.
  
  T *write(T *element);

  // Read (retrieve) an element from the buffer.
  // You pass in a pointer to an object which will replace the
  // retrieved object.
  // The method returns a pointer to the retrieved object, or NULL
  // if no object is available for retrieval.
  // The caller is responsible for allocating the element passed
  // in on the initial read.

  T *read(T *element);
  
private:

  T **_buf;
  size_t _size;
  mutable boost::mutex _mutex;
  int _writeIndex;
  int _readIndex;
  
  T *_alloc(size_t size);
  CircBuffer &_copy(const CircBuffer &rhs);
  void _free();

};

////////////////////////////////////////////////
// The Implementation.

// constructor

template <class T>
CircBuffer<T>::CircBuffer(size_t size)
{
  _buf = new T*[size];
  for (size_t ii = 0; ii < size; ii++) {
    _buf[ii] = new T;
  }
  _size = size;
  _writeIndex = -1;
  _readIndex = -1;
}

// destructor

template <class T>
CircBuffer<T>::~CircBuffer()
{
  _free();
}

// allocation

template <class T>
T *CircBuffer<T>::_alloc(size_t size)
{
  if (size == _size) {
    return _buf;
  }
  _free();
  _buf = new T*[size];
  for (size_t ii = 0; ii < size; ii++) {
    _buf[ii] = new T;
  }
  _size = size;
  return _buf;
}

// copy constructor

template <class T>
CircBuffer<T>::CircBuffer(const CircBuffer<T> &rhs) {
  if (this != &rhs) {
    _buf = NULL;
    _size = 0;
    _copy(rhs);
  }
}

// assignment

template <class T>
CircBuffer<T>& CircBuffer<T>::operator=(const CircBuffer<T> &rhs) {
  return _copy(rhs);
}

// free up

template <class T>
void CircBuffer<T>::_free()
{
  if (_buf != NULL) {
    for (size_t ii = 0; ii < _size; ii++) {
      delete _buf[ii];
    }
    delete[] _buf;
  }
  _buf = NULL;
  _size = 0;
}

// copy

template <class T>
CircBuffer<T> &CircBuffer<T>::_copy(const CircBuffer<T> &rhs)

{

  if (&rhs == this) {
    return *this;
  }

  if (rhs._size == 0 || rhs._buf == NULL) {
    _free();
    return *this;
  }

  _alloc(rhs._size);
  memcpy(_buf, rhs._buf, _size * sizeof(T));

  return *this;

}

// write

template <class T>
T *CircBuffer<T>::write(T *element)
{
  boost::mutex::scoped_lock guard(_mutex);
  int newIndex = (_writeIndex + 1) % _size;
  T *retVal = _buf[newIndex];
  _buf[newIndex] = element;
  _writeIndex = newIndex;
  return retVal;
}

// read

template <class T>
T *CircBuffer<T>::read(T *element)
{
  boost::mutex::scoped_lock guard(_mutex);
  if (_writeIndex == _readIndex) {
    // no new data available
    return NULL;
  }
  int newIndex = (_readIndex + 1) % _size;
  T *retVal = _buf[newIndex];
  _buf[newIndex] = element;
  _readIndex = newIndex;
  return retVal;
}

#endif
