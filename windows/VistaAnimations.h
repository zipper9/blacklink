#ifndef VISTA_ANIMATIONS_H_
#define VISTA_ANIMATIONS_H_

#include "../client/typedefs.h"
#include "../client/w.h"

namespace VistaAnimations
{

	struct StateTransition
	{
		HBITMAP bitmaps[3];
		uint8_t* bits[3];
		int states[2];
		int nextState;
		RECT rc;
		int64_t startTime;
		double currentValue;
		double startValue;
		double endValue;
		int currentAlpha;
		int duration;
		bool running;

		StateTransition();
		~StateTransition() { cleanup(); }

		void cleanup();
		bool update(int64_t time, int64_t frequency);
		void updateAlpha();
		void updateImage();
		void draw(HDC hdc) const;
		void createBitmaps(HDC hdc, const RECT& rcDraw);
		void start(int64_t time, int duration);
		void reverse(int64_t time, int totalDuration);
		bool isForward() const { return startValue < endValue; }
		bool nextTransition();
		void setNewState(int64_t time, int newState, int duration);
		int getCompletedState() const;
	};

	struct UpdateParams
	{
		HDC hdc;
		RECT rc;
		int64_t timestamp;
		int64_t frequency;
		bool update;
		bool running;
	};

}

#endif // VISTA_ANIMATIONS_H_
