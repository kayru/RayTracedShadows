#pragma once

#include <Rush/Rush.h>

template <typename T, size_t SIZE>
struct MovingAverage
{
	MovingAverage()
	{
		reset();
	}

	inline void reset()
	{
		idx = 0;
		sum = 0;
		for( size_t i=0; i<SIZE; ++i )
		{
			buf[i] = 0;
		}
	}

	inline void add(T v)
	{
		sum += v;
		sum -= buf[idx];
		buf[idx] = v;
		idx = (idx+1)%SIZE;
	}

	inline T get() const
	{
		return sum / SIZE;
	}

	size_t idx;
	T sum;
	T buf[SIZE];
};
