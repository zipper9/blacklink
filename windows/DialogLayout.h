#ifndef DIALOG_LAYOUT_H_
#define DIALOG_LAYOUT_H_

#include "../client/w.h"

namespace DialogLayout
{

static const int FLAG_TRANSLATE   = 1;
static const int FLAG_HWND        = 2;
static const int FLAG_PLACEHOLDER = 4;

static const int INDEX_RELATIVE = 0x8000;

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
	UINT_PTR id;
	int flags;
	int width;
	int height;
	int group;
	const Align* left;
	const Align* right;
	const Align* top;
	const Align* bottom;
};

struct Options
{
	int width;
	int height;
	bool show;
};

void layout(HWND hWnd, const Item* items, int count, const Options* opt = nullptr);

}

#endif // DIALOG_LAYOUT_H_
