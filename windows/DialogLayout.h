#ifndef DIALOG_LAYOUT_H_
#define DIALOG_LAYOUT_H_

#include "../client/w.h"

namespace DialogLayout
{

static const int FLAG_TRANSLATE = 1;

#define U_PX(val) ((val)<<1)
#define U_DU(val) ((val)<<1 | 1)

static const int UNSPEC = -1;
static const int AUTO   = -2;

enum
{
	SIDE_LEFT,
	SIDE_RIGHT,
	SIDE_TOP,
	SIDE_BOTTOM
};

struct Align
{
	int index;
	int side;
	int offset;
};

struct Item
{
	int id;
	int flags;
	int width;
	int height;
	int group;
	const Align* left;
	const Align* right;
	const Align* top;
	const Align* bottom;
};

void layout(HWND hWnd, const Item* items, int count);

}

#endif // DIALOG_LAYOUT_H_
