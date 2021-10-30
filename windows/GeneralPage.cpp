/*
 * Copyright (C) 2001-2017 Jacek Sieka, arnetheduck on gmail point com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "stdafx.h"
#include "GeneralPage.h"
#include "WinUtil.h"
#include "ResourceLoader.h"
#include "KnownClients.h"
#include "../client/SimpleXMLReader.h"

static const string defLangFileName("en-US.xml");

static const WinUtil::TextItem texts[] =
{
	{ IDC_SETTINGS_PERSONAL_INFORMATION, ResourceManager::SETTINGS_PERSONAL_INFORMATION },
	{ IDC_SETTINGS_NICK, ResourceManager::NICK },
	{ IDC_SETTINGS_EMAIL, ResourceManager::EMAIL },
	{ IDC_SETTINGS_GENDER, ResourceManager::FLY_GENDER },
	{ IDC_SETTINGS_DESCRIPTION, ResourceManager::DESCRIPTION },
	{ IDC_SETTINGS_UPLOAD_LINE_SPEED, ResourceManager::SETTINGS_UPLOAD_LINE_SPEED },
	{ IDC_SETTINGS_MEBIBITS, ResourceManager::MBPS },
#ifdef IRAINMAN_ENABLE_SLOTS_AND_LIMIT_IN_DESCRIPTION
	{ IDC_CHECK_ADD_TO_DESCRIPTION, ResourceManager::ADD_TO_DESCRIPTION },
	{ IDC_CHECK_ADD_LIMIT, ResourceManager::ADD_LIMIT },
	{ IDC_CHECK_ADD_SLOTS, ResourceManager::ADD_SLOTS },
#endif
	{ IDC_SETTINGS_NOMINALBW, ResourceManager::SETTINGS_NOMINAL_BANDWIDTH },
	{ IDC_CLIENT_ID, ResourceManager::CLIENT_ID },
	{ IDC_SETTINGS_LANGUAGE, ResourceManager::SETTINGS_LANGUAGE },
	{ IDC_ENCODINGTEXT, ResourceManager::DEFAULT_CHARACTER_SET },
	{ 0, ResourceManager::Strings() }
};

static const PropPage::Item items[] =
{
	{ IDC_NICK,          SettingsManager::NICK,               PropPage::T_STR  },
	{ IDC_EMAIL,         SettingsManager::EMAIL,              PropPage::T_STR  },
	{ IDC_DESCRIPTION,   SettingsManager::DESCRIPTION,        PropPage::T_STR  },
	{ IDC_CONNECTION,    SettingsManager::UPLOAD_SPEED,       PropPage::T_STR  },
	{ IDC_CLIENT_ID,     SettingsManager::OVERRIDE_CLIENT_ID, PropPage::T_BOOL },
	{ IDC_CLIENT_ID_BOX, SettingsManager::CLIENT_ID,          PropPage::T_STR  },
#ifdef IRAINMAN_ENABLE_SLOTS_AND_LIMIT_IN_DESCRIPTION
	{ IDC_CHECK_ADD_TO_DESCRIPTION, SettingsManager::ADD_TO_DESCRIPTION,    PropPage::T_BOOL },
	{ IDC_CHECK_ADD_LIMIT,          SettingsManager::ADD_DESCRIPTION_LIMIT, PropPage::T_BOOL },
	{ IDC_CHECK_ADD_SLOTS,          SettingsManager::ADD_DESCRIPTION_SLOTS, PropPage::T_BOOL },
#endif
	{ 0, 0, PropPage::T_END }
};

void GeneralPage::write()
{
	string oldNick = SETTING(NICK);
	string oldEmail = SETTING(EMAIL);
	string oldDescription = SETTING(DESCRIPTION);
	string oldUploadSpeed = SETTING(UPLOAD_SPEED);
	int oldGender = SETTING(GENDER);
	PropPage::write(*this, items);
	int selIndex = ctrlLanguage.GetCurSel();
	if (selIndex != -1)
	{
		string langFile(static_cast<const char*>(ctrlLanguage.GetItemDataPtr(selIndex)));
		if (SETTING(LANGUAGE_FILE) != langFile)
		{
			g_settings->set(SettingsManager::LANGUAGE_FILE, langFile);
			SettingsManager::getInstance()->save();
			ResourceManager::loadLanguage(Util::getLocalisationPath() + langFile);
			if (languageList.size() != 1)
				MessageBox(CTSTRING(CHANGE_LANGUAGE_INFO), CTSTRING(CHANGE_LANGUAGE), MB_OK | MB_ICONEXCLAMATION);
		}
	}
	g_settings->set(SettingsManager::GENDER, ctrlGender.GetCurSel());
	int charset = WinUtil::getSelectedCharset(CComboBox(GetDlgItem(IDC_ENCODING)));
	g_settings->set(SettingsManager::DEFAULT_CODEPAGE, Text::charsetToString(charset));
	if (SETTING(NICK) != oldNick ||
	    SETTING(EMAIL) != oldEmail ||
	    SETTING(DESCRIPTION) != oldDescription ||
	    SETTING(UPLOAD_SPEED) != oldUploadSpeed ||
	    SETTING(GENDER) != oldGender)
			ClientManager::resendMyInfo();
}

void GeneralPage::addGenderItem(const TCHAR* text, int imageIndex, int index)
{
	COMBOBOXEXITEM cbitem = {CBEIF_TEXT | CBEIF_IMAGE | CBEIF_SELECTEDIMAGE};
	cbitem.pszText = const_cast<TCHAR*>(text);
	cbitem.iItem = index;
	cbitem.iImage = imageIndex;
	cbitem.iSelectedImage = imageIndex;
	ctrlGender.InsertItem(&cbitem);
}

LRESULT GeneralPage::onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
#ifndef IRAINMAN_ENABLE_SLOTS_AND_LIMIT_IN_DESCRIPTION
	::EnableWindow(GetDlgItem(IDC_CHECK_ADD_TO_DESCRIPTION), FALSE);
	::EnableWindow(GetDlgItem(IDC_CHECK_ADD_SLOTS), FALSE);
	::EnableWindow(GetDlgItem(IDC_CHECK_ADD_LIMIT), FALSE);
	GetDlgItem(IDC_CHECK_ADD_TO_DESCRIPTION).ShowWindow(FALSE);
	GetDlgItem(IDC_CHECK_ADD_SLOTS).ShowWindow(FALSE);
	GetDlgItem(IDC_CHECK_ADD_LIMIT).ShowWindow(FALSE);
#endif	
	WinUtil::translate(*this, texts);
	CComboBox ctrlConnection(GetDlgItem(IDC_CONNECTION));
	
	for (auto i = SettingsManager::g_connectionSpeeds.cbegin(); i != SettingsManager::g_connectionSpeeds.cend(); ++i)
		ctrlConnection.AddString(Text::toT(*i).c_str());
	
	CComboBox comboClientId(GetDlgItem(IDC_CLIENT_ID_BOX));
	for (size_t i = 0; KnownClients::clients[i].name; ++i)
	{
		string clientId = KnownClients::clients[i].name;
		clientId += ' ';
		clientId += KnownClients::clients[i].version;
		comboClientId.AddString(Text::toT(clientId).c_str());
	}
	comboClientId.SetCurSel(0);

	PropPage::read(*this, items);
	
	ctrlLanguage.Attach(GetDlgItem(IDC_LANGUAGE));
	getLangList();
	for (auto i = languageList.cbegin(); i != languageList.cend(); ++i)
	{
		int index = ctrlLanguage.AddString(Text::toT(i->language).c_str());
		ctrlLanguage.SetItemDataPtr(index, const_cast<char*>(i->filename.c_str()));
	}
	
	int selIndex = -1;
	int defLangIndex = -1;
	int itemCount = ctrlLanguage.GetCount();
	for (int i = 0; i < itemCount; ++i)
	{
		const char* text = static_cast<const char*>(ctrlLanguage.GetItemDataPtr(i));
		if (SETTING(LANGUAGE_FILE) == text)
		{
			selIndex = i;
			break;
		}
		if (defLangIndex < 0 && defLangFileName == text)
			defLangIndex = i;
	}
	if (selIndex < 0)
		selIndex = defLangIndex;	

	ctrlLanguage.SetCurSel(selIndex);
	
	ctrlConnection.SetCurSel(ctrlConnection.FindString(0, Text::toT(SETTING(UPLOAD_SPEED)).c_str()));
	
	ctrlGender.Attach(GetDlgItem(IDC_GENDER));
	ResourceLoader::LoadImageList(IDR_GENDER_USERS, imageListGender, 16, 16);
	ctrlGender.SetImageList(imageListGender);

	CEdit nick(GetDlgItem(IDC_NICK));
	nick.LimitText(49);
	
	CEdit desc(GetDlgItem(IDC_DESCRIPTION));
	desc.LimitText(100);
	
	desc.Detach();	
	desc.Attach(GetDlgItem(IDC_SETTINGS_EMAIL));
	desc.LimitText(64);
	
	int id = 0;
	addGenderItem(CTSTRING(FLY_GENDER_NONE), id++, 0);
	addGenderItem(CTSTRING(FLY_GENDER_MALE), id++, 1);
	addGenderItem(CTSTRING(FLY_GENDER_FEMALE), id++, 2);
	addGenderItem(CTSTRING(FLY_GENDER_ASEXUAL), id++, 3);
	ctrlGender.SetCurSel(SETTING(GENDER));
	
	int charset = Text::charsetFromString(SETTING(DEFAULT_CODEPAGE));
	CComboBox comboBox(GetDlgItem(IDC_ENCODING));
	WinUtil::fillCharsetList(comboBox, charset, false, false);

	comboClientId.EnableWindow(SETTING(OVERRIDE_CLIENT_ID) ? TRUE : FALSE);
	
	fixControls();
	return TRUE;
}

LRESULT GeneralPage::onTextChanged(WORD /*wNotifyCode*/, WORD wID, HWND hWndCtl, BOOL& /*bHandled*/)
{
	tstring buf;
	WinUtil::getWindowText(GetDlgItem(wID), buf);
	tstring old = buf;
	
	// TODO: move to Text and cleanup.
	if (!buf.empty())
	{
		// Strip '$', '|', '<', '>' and ' ' from text
		TCHAR *b = &buf[0], *f = &buf[0], c;
		while ((c = *b++) != '\0')
		{
			if (c != '$' && c != '|' && (wID == IDC_DESCRIPTION || c != ' ') && ((wID != IDC_NICK && wID != IDC_DESCRIPTION && wID != IDC_SETTINGS_EMAIL) || (c != '<' && c != '>')))
				*f++ = c;
		}
		
		*f = '\0';
	}
	if (old != buf)
	{
		// Something changed; update window text without changing cursor pos
		CEdit tmp;
		tmp.Attach(hWndCtl);
		int start, end;
		tmp.GetSel(start, end);
		tmp.SetWindowText(buf.data());
		if (start > 0) start--;
		if (end > 0) end--;
		tmp.SetSel(start, end);
		tmp.Detach();
	}
	
	return TRUE;
}

LRESULT GeneralPage::onClickedActive(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	fixControls();
	return 0;
}

class LangFileXMLCallback : public SimpleXMLReader::CallBack
{
	void startTag(const string& name, StringPairList& attribs, bool simple)
	{
		if (name == "Language")
		{
			if (language.empty())
				language = getAttrib(attribs, "Name", 0);
		}
	}

	void endTag(const string& name, const std::string& data) {}

public:
	string language;
};

static string getLangFromFile(const string& filename)
{
	char buf[2048];
	LangFileXMLCallback cb;
	try
	{
		File f(filename, File::READ, File::OPEN);
		size_t len = sizeof(buf);
		f.read(buf, len);
		SimpleXMLReader reader(&cb);
		reader.parse(buf, len, false);
	}
	catch (FileException&) {}
	catch (SimpleXMLException&) {}
	return cb.language;
}

void GeneralPage::getLangList()
{
	if (languageList.empty())
	{
		LanguageInfo defLang;
		defLang.filename = defLangFileName;
		defLang.language = "English";
		languageList.push_back(defLang);
		const StringList files = File::findFiles(Util::getLocalisationPath(), "*-*.xml");
		for (auto i = files.cbegin(); i != files.cend(); ++i)
		{
			LanguageInfo lang;
			lang.filename = Util::getFileName(*i);
			if (lang.filename == defLang.filename) continue;
			lang.language = getLangFromFile(*i);
			if (lang.language.empty()) continue;
			languageList.push_back(lang);
		}
	}
}

void GeneralPage::fixControls()
{
#ifdef IRAINMAN_ENABLE_SLOTS_AND_LIMIT_IN_DESCRIPTION
	BOOL use_description = IsDlgButtonChecked(IDC_CHECK_ADD_TO_DESCRIPTION) == BST_CHECKED;
	
	::EnableWindow(GetDlgItem(IDC_CHECK_ADD_SLOTS), use_description);
	::EnableWindow(GetDlgItem(IDC_CHECK_ADD_LIMIT), use_description);
#endif
}

LRESULT GeneralPage::onChangeId(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	::EnableWindow(GetDlgItem(IDC_CLIENT_ID_BOX), IsDlgButtonChecked(IDC_CLIENT_ID) == BST_CHECKED);
	return 0;
}
