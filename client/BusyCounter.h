#ifndef BUSY_COUNTER_H_
#define BUSY_COUNTER_H_

template<typename T>
class BusyCounter
{
		T& value;

	public:
		explicit BusyCounter(T& value) : value(value) { ++value; }
		~BusyCounter() { --value; }
};

template<>
class BusyCounter<bool>
{
		bool& value;

	public:
		explicit BusyCounter(bool& value) : value(value) { value = true; }
		~BusyCounter() { value = false; }
};

#endif // BUSY_COUNTER_H_
