////////////////////////////////////////////////
//  ToolbarManager.cpp
//
//  This is based on portions of:
//  http://www.codeproject.com/wtl/regst.asp
//  (Copyright (c) 2001 Magomed Abdurakhmanov)
//
//  Changed save to xml file instead of registry
//
//  No warranties given. Provided as is.

#include "stdafx.h"
#include "ToolbarManager.h"
#include "../client/StringTokenizer.h"
#include "../client/ClientManager.h"
#include "../client/SimpleXML.h"

#ifdef REBARBANDINFOW_V6_SIZE
#define COMPAT_REBARBANDINFO_SIZE REBARBANDINFOW_V6_SIZE
#else
#define COMPAT_REBARBANDINFO_SIZE sizeof(REBARBANDINFO)
#endif

ToolbarManager::~ToolbarManager()
{
	for_each(toolbarEntries.begin(), toolbarEntries.end(), [](auto p) { delete p; });
}

void ToolbarManager::on(SettingsManagerListener::Load, SimpleXML& xml)
{
	xml.resetCurrentChild();
	if (xml.findChild("Rebars"))
	{
		xml.stepIn();
		while (xml.findChild("Rebar"))
		{
			ToolbarEntry* t = new ToolbarEntry();
			t->setName(xml.getChildAttrib("Name"));
			t->setID(xml.getChildAttrib("ID"));
			t->setCX(xml.getChildAttrib("CX"));
			t->setBreakLine(xml.getChildAttrib("BreakLine"));
			t->setBandCount(xml.getIntChildAttrib("BandCount"));
			addToolBarEntry(t);
		}
		xml.stepOut();
	}
}

void ToolbarManager::on(SettingsManagerListener::Save, SimpleXML& xml)
{
	xml.addTag("Rebars");
	xml.stepIn();
	for (auto i = toolbarEntries.cbegin(); i != toolbarEntries.cend(); ++i)
	{
		xml.addTag("Rebar");
		xml.addChildAttrib("Name", (*i)->getName());
		xml.addChildAttrib("ID", (*i)->getID());
		xml.addChildAttrib("CX", (*i)->getCX());
		xml.addChildAttrib("BreakLine", (*i)->getBreakLine());
		xml.addChildAttrib("BandCount", (*i)->getBandCount());
	}
	xml.stepOut();
}

void ToolbarManager::getFrom(HWND rebarWnd, const string& name)
{
	CReBarCtrl rebar(rebarWnd);
	dcassert(rebar.IsWindow());
	if (rebar.IsWindow())
	{
		removeToolbarEntry(getToolbarEntry(name));

		ToolbarEntry* t = new ToolbarEntry();
		string id, cx, bl;
		t->setName(name);
		t->setBandCount(rebar.GetBandCount());

		int count = t->getBandCount();
		for (int i = 0; i < count; i++)
		{
			const string& dl = i > 0 ? "," : "";
			REBARBANDINFO rbi = {};
			rbi.cbSize = COMPAT_REBARBANDINFO_SIZE;
			rbi.fMask = RBBIM_ID | RBBIM_SIZE | RBBIM_STYLE;
			rebar.GetBandInfo(i, &rbi);
			id += dl + Util::toString(rbi.wID);
			cx += dl + Util::toString(rbi.cx);
			bl += dl + ((rbi.fStyle & RBBS_BREAK) ? "1" : "0");

		}

		t->setID(id);
		t->setCX(cx);
		t->setBreakLine(bl);
		addToolBarEntry(t);
	}
}

void ToolbarManager::applyTo(HWND rebarWnd, const string& name) const
{
	CReBarCtrl rebar(rebarWnd);
	dcassert(rebar.IsWindow());
	if (rebar.IsWindow())
	{
		const ToolbarEntry* t = getToolbarEntry(name);
		if (t)
		{
			const StringTokenizer<string> id(t->getID(), ',');
			const StringList& idList = id.getTokens();
			const StringTokenizer<string> cx(t->getCX(), ',');
			const StringList& cxList = cx.getTokens();
			const StringTokenizer<string> bl(t->getBreakLine(), ',');
			const StringList& blList = bl.getTokens();

			size_t listSize = std::min(idList.size(), cxList.size());
			int count = (int) std::min<size_t>(t->getBandCount(), listSize);
			for (int i = 0; i < count; i++)
			{
				int index = rebar.IdToIndex(Util::toInt(idList[i]));
				if (index < 0) continue;
				rebar.MoveBand(index, i);

				REBARBANDINFO rbi = {};
				rbi.cbSize = COMPAT_REBARBANDINFO_SIZE;
				rbi.fMask = RBBIM_ID | RBBIM_SIZE | RBBIM_STYLE;
				rebar.GetBandInfo(i, &rbi);

				rbi.cx = Util::toInt(cxList[i]);
				int breakLine = i < (int) blList.size() ? Util::toInt(blList[i]) : 0;
				if (breakLine > 0)
					rbi.fStyle |= RBBS_BREAK;
				else
					rbi.fStyle &= ~RBBS_BREAK;
				rebar.SetBandInfo(i, &rbi);
			}
		}
	}
}

void ToolbarManager::removeToolbarEntry(const ToolbarEntry* entry)
{
	if (entry)
	{
		auto i = find(toolbarEntries.begin(), toolbarEntries.end(), entry);
		if (i == toolbarEntries.end())
			return;
		toolbarEntries.erase(i);
		delete entry;
	}
}

const ToolbarEntry* ToolbarManager::getToolbarEntry(const string& name) const
{
	for (const ToolbarEntry* t : toolbarEntries)
		if (Text::asciiEqual(t->getName(), name)) return t;
	return nullptr;
}
