#ifndef WAITABLE_EVENT_H_
#define WAITABLE_EVENT_H_

#ifdef _WIN32
#include "WinEvent.h"
typedef WinEvent<TRUE> WaitableEvent;
#elif defined(__linux__) || defined(linux)
#include "LinuxEvent.h"
typedef LinuxEvent WaitableEvent;
#else
#include "PipeEvent.h"
typedef PipeEvent WaitableEvent;
#endif

#endif // WAITABLE_EVENT_H_
