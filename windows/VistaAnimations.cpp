#include "stdafx.h"
#include "VistaAnimations.h"
#include "GdiUtil.h"

using namespace VistaAnimations;

template<typename T>
void clampValue(T& val, T minVal, T maxVal)
{
	if (val < minVal)
		val = minVal;
	else if (val > maxVal)
		val = maxVal;
}

StateTransition::StateTransition()
{
	for (int i = 0; i < _countof(bitmaps); i++)
	{
		bitmaps[i] = nullptr;
		bits[i] = 0;
	}
	rc.left = rc.right = rc.top = rc.bottom = 0;
	states[0] = states[1] = nextState = -1;
	startTime = 0;
	currentValue = 0;
	currentAlpha = -1;
	duration = 0;
	startValue = 0;
	endValue = 1;
	running = true;
}

void StateTransition::cleanup()
{
	for (int i = 0; i < _countof(bitmaps); ++i)
		if (bitmaps[i])
		{
			DeleteObject(bitmaps[i]);
			bitmaps[i] = nullptr;
		}
}

bool StateTransition::update(int64_t time, int64_t frequency)
{
	if (!running || time <= startTime || frequency <= 0) return false;
	double elapsed = double(time - startTime) * 1000 / double(frequency);
	if (elapsed <= duration && duration > 0)
	{
		currentValue = startValue + (endValue - startValue) * (elapsed / duration);
		if (endValue > startValue)
		{
			if (currentValue > endValue) running = false;
		}
		else
			if (currentValue < endValue) running = false;
	}
	else
	{
		currentValue = endValue;
		running = false;
	}
#ifdef DEBUG_TRANSITION_ANIM
	if (!running) ATLTRACE("Transition %p finished\n", this);
#endif
	updateAlpha();
	return true;
}

void StateTransition::updateAlpha()
{
	int intVal = (int) (256 * currentValue);
	clampValue(intVal, 0, 256);
	if (currentAlpha == intVal) return;
	currentAlpha = intVal;
	if (currentAlpha != 0 && currentAlpha != 256) updateImage();
}

void StateTransition::updateImage()
{
	ATLASSERT(currentAlpha >= 0 && currentAlpha <= 256);
	unsigned width = rc.right - rc.left;
	unsigned height = rc.bottom - rc.top;
	WinUtil::blend32(bits[0], bits[1], bits[2], width * height, 256 - currentAlpha);
	GdiFlush();
}

void StateTransition::draw(HDC hdc) const
{
	int index = 2;
	if (currentAlpha == 0)
		index = 0;
	else if (currentAlpha == 256)
		index = 1;
	int width = rc.right - rc.left;
	int height = rc.bottom - rc.top;
	HDC bitmapDC = CreateCompatibleDC(hdc);
	HGDIOBJ oldBitmap = SelectObject(bitmapDC, bitmaps[index]);
	BitBlt(hdc, rc.left, rc.top, width, height, bitmapDC, 0, 0, SRCCOPY);
	SelectObject(bitmapDC, oldBitmap);
	DeleteDC(bitmapDC);
}

void StateTransition::createBitmaps(HDC hdc, const RECT& rcDraw)
{
	int width = rc.right - rc.left;
	int height = rc.bottom - rc.top;
	if (width != rcDraw.right - rcDraw.left || height != rcDraw.bottom - rcDraw.top)
	{
		width = rcDraw.right - rcDraw.left;
		height = rcDraw.bottom - rcDraw.top;
		cleanup();
	}

	rc = rcDraw;

	BITMAPINFOHEADER bmi = {};
	bmi.biWidth = width;
	bmi.biHeight = -height;
	bmi.biBitCount = 32;
	bmi.biCompression = BI_RGB;
	bmi.biPlanes = 1;
	bmi.biSize = sizeof(bmi);
	bmi.biSizeImage = width * height << 2;

	for (int i = 0; i < 3; i++)
		if (!bitmaps[i])
			bitmaps[i] = CreateDIBSection(nullptr, (BITMAPINFO*) &bmi, DIB_RGB_COLORS, (void **) &bits[i], nullptr, 0);
}

void StateTransition::start(int64_t time, int duration)
{
	startTime = time;
	currentValue = 0;
	startValue = 0;
	endValue = 1;
	currentAlpha = 0;
	nextState = -1;
	this->duration = duration;
	running = true;
#ifdef DEBUG_TRANSITION_ANIM
	ATLTRACE("Transition %p started\n", this);
#endif
}

void StateTransition::reverse(int64_t time, int totalDuration)
{
	if (startValue < endValue)
	{
		endValue = 0;
		duration = (int) (totalDuration * currentValue);
	}
	else
	{
		endValue = 1;
		duration = (int) (totalDuration * (1 - currentValue));
	}
	startTime = time;
	startValue = currentValue;
#ifdef DEBUG_TRANSITION_ANIM
	ATLTRACE("Transition %p reversed: start=%f, end=%f\n", this, startValue, endValue);
#endif
}

bool StateTransition::nextTransition()
{
	if (nextState == -1) return false;
	if (isForward()) states[0] = states[1];
#ifdef DEBUG_TRANSITION_ANIM
	ATLTRACE("Transition %p: next state is %d\n", this, nextState);
#endif
	states[1] = nextState;
	nextState = -1;
	return true;
}

void StateTransition::setNewState(int64_t time, int newState, int duration)
{
	ATLASSERT(running);
	if (states[0] == newState || states[1] == newState)
	{
		nextState = -1;
		if (states[0] == newState)
		{
			if (isForward())
				reverse(time, duration);
		}
		else
		{
			if (!isForward())
				reverse(time, duration);
		}
	}
	else
		nextState = newState;
}

int StateTransition::getCompletedState() const
{
	return states[isForward() ? 1 : 0];
}
