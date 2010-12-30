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

  size_t size() const { return _size; }

  // Insert an element to the buffer.
  // You pass in a pointer to the object you want to insert.
  // The method returns a pointer to the replaced object. This
  // should be re-used for the next insert.

  T *insert(T *val);

  // Retrieve an element to the buffer.
  // You pass in a pointer to and object which will replace the
  // retrieved object.
  // The method returns a pointer to the retrieved object, or NULL
  // if no object is availalable for retrieval.

  T *retrieve(T *val);

private:

  T *_buf;
  size_t _size;
  mutable boost::mutex _mutex;
  
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
  _buf = new T[size];
  _size = size;
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
  _buf = new T[size];
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

#endif
