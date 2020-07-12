////////////////////////////////////////////////
//  ToolbarManager.h
//
//  This is based on portions of:
//  http://www.codeproject.com/wtl/regst.asp
//  (Copyright (c) 2001 Magomed Abdurakhmanov)
//
//  Changed save to xml file instead of registry
//
//  No warranties given. Provided as is.

#ifndef TOOLBARMANAGER_H
#define TOOLBARMANAGER_H

#include "../client/Singleton.h"
#include "../client/SettingsManagerListener.h"
#include "../client/Util.h"

class ToolbarEntry
{
	public:
		typedef ToolbarEntry* Ptr;
		typedef vector<Ptr> List;
		
		ToolbarEntry() { bandcount = 0; }
		
		GETSET(string, name, Name);
		GETSET(string, id, ID);
		GETSET(string, cx, CX);
		GETSET(string, breakline, BreakLine);
		GETSET(int, bandcount, BandCount);
};

class ToolbarManager : public Singleton<ToolbarManager>, public SettingsManagerListener
{
	public:
		// Get & Set toolbar positions
		void getFrom(HWND rebarWnd, const string& name);
		void applyTo(HWND rebarWnd, const string& name) const;

	private:
		~ToolbarManager();

		// Get data by name
		const ToolbarEntry* getToolbarEntry(const string& name) const;

		// Remove old entry, when adding new
		void removeToolbarEntry(const ToolbarEntry* entry);
		void addToolBarEntry(ToolbarEntry* entry)
		{
			toolbarEntries.push_back(entry);
		}
		
		// Store Toolbar infos here
		ToolbarEntry::List toolbarEntries;

		virtual void on(Load, SimpleXML&) override;
		virtual void on(Save, SimpleXML&) override;

		friend class Singleton<ToolbarManager>;
};

#endif // TOOLBARMANAGER_H
