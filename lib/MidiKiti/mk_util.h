#pragma once

#include <Arduino.h>

namespace mk
{

class ElapsedMicros
{
private:
	unsigned long us;
public:
	ElapsedMicros(void) { us = micros(); }
	ElapsedMicros(unsigned long val) { us = micros() - val; }
	ElapsedMicros(const ElapsedMicros &orig) { us = orig.us; }
	operator unsigned long () const { return micros() - us; }
	ElapsedMicros & operator = (const ElapsedMicros &rhs) { us = rhs.us; return *this; }
	ElapsedMicros & operator = (unsigned long val) { us = micros() - val; return *this; }
	ElapsedMicros & operator -= (unsigned long val)      { us += val ; return *this; }
	ElapsedMicros & operator += (unsigned long val)      { us -= val ; return *this; }
	ElapsedMicros operator - (int val) const           { ElapsedMicros r(*this); r.us += val; return r; }
	ElapsedMicros operator - (unsigned int val) const  { ElapsedMicros r(*this); r.us += val; return r; }
	ElapsedMicros operator - (long val) const          { ElapsedMicros r(*this); r.us += val; return r; }
	ElapsedMicros operator - (unsigned long val) const { ElapsedMicros r(*this); r.us += val; return r; }
	ElapsedMicros operator + (int val) const           { ElapsedMicros r(*this); r.us -= val; return r; }
	ElapsedMicros operator + (unsigned int val) const  { ElapsedMicros r(*this); r.us -= val; return r; }
	ElapsedMicros operator + (long val) const          { ElapsedMicros r(*this); r.us -= val; return r; }
	ElapsedMicros operator + (unsigned long val) const { ElapsedMicros r(*this); r.us -= val; return r; }
};

}