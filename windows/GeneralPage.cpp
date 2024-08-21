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
#include "../client/File.h"
#include "../client/AppPaths.h"
#include "../client/PathUtil.h"
#include "../client/SettingsUtil.h"
#include "../client/ConfCore.h"

static const string defLangFileName("en-US.xml");

static const char* connSpeeds[] = { "0.005", "0.01", "0.02", "0.05", "0.1", "0.2", "0.5", "1", "2", "5", "10", "20", "50", "100", "1000" };

static const WinUtil::TextItem texts[] =
{
	{ IDC_SETTINGS_PERSONAL_INFORMATION, ResourceManager::SETTINGS_PERSONAL_INFORMATION },
	{ IDC_SETTINGS_NICK, ResourceManager::NICK },
	{ IDC_SETTINGS_EMAIL, ResourceManager::EMAIL },
	{ IDC_SETTINGS_GENDER, ResourceManager::FLY_GENDER },
	{ IDC_SETTINGS_DESCRIPTION, ResourceManager::DESCRIPTION },
	{ IDC_SETTINGS_UPLOAD_LINE_SPEED, ResourceManager::SETTINGS_UPLOAD_LINE_SPEED },
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
	{ IDC_NICK,          Conf::NICK,               PropPage::T_STR  },
	{ IDC_EMAIL,         Conf::EMAIL,              PropPage::T_STR  },
	{ IDC_DESCRIPTION,   Conf::DESCRIPTION,        PropPage::T_STR  },
	{ IDC_CLIENT_ID,     Conf::OVERRIDE_CLIENT_ID, PropPage::T_BOOL },
	{ IDC_CLIENT_ID_BOX, Conf::CLIENT_ID,          PropPage::T_STR  },
#ifdef IRAINMAN_ENABLE_SLOTS_AND_LIMIT_IN_DESCRIPTION
	{ IDC_CHECK_ADD_TO_DESCRIPTION, Conf::ADD_TO_DESCRIPTION,    PropPage::T_BOOL },
	{ IDC_CHECK_ADD_LIMIT,          Conf::ADD_DESCRIPTION_LIMIT, PropPage::T_BOOL },
	{ IDC_CHECK_ADD_SLOTS,          Conf::ADD_DESCRIPTION_SLOTS, PropPage::T_BOOL },
#endif
	{ 0, 0, PropPage::T_END }
};

void GeneralPage::write()
{
	uint8_t oldUserInfo[TigerHash::BYTES], userInfo[TigerHash::BYTES];
	Util::getUserInfoHash(oldUserInfo);

	PropPage::write(*this, items);
	CComboBox ctrlConnection(GetDlgItem(IDC_CONNECTION));
	const int uploadSpeedIndex = ctrlConnection.GetCurSel();

	bool updateLanguage = false;
	string languageFile;
	CComboBox ctrlLanguage(GetDlgItem(IDC_LANGUAGE));
	int selIndex = ctrlLanguage.GetCurSel();
	if (selIndex != -1)
		languageFile = static_cast<const char*>(ctrlLanguage.GetItemDataPtr(selIndex));

	const int gender = ctrlGender.GetCurSel();
	const string defaultCodePage = Text::charsetToString(WinUtil::getSelectedCharset(CComboBox(GetDlgItem(IDC_ENCODING))));

	auto ss = SettingsManager::instance.getCoreSettings();
	ss->lockWrite();
	if (uploadSpeedIndex >= 0)
		ss->setString(Conf::UPLOAD_SPEED, connSpeeds[uploadSpeedIndex]);
	ss->setInt(Conf::GENDER, gender);
	ss->setString(Conf::DEFAULT_CODEPAGE, defaultCodePage);
	if (!languageFile.empty())
	{
		const string& oldLanguage = ss->getString(Conf::LANGUAGE_FILE);
		if (oldLanguage != languageFile)
		{
			ss->setString(Conf::LANGUAGE_FILE, languageFile);
			updateLanguage = true;
		}
	}
	ss->unlockWrite();

	Util::getUserInfoHash(userInfo);
	if (memcmp(userInfo, oldUserInfo, TigerHash::BYTES))
		ClientManager::resendMyInfo();

	if (updateLanguage)
	{
		ResourceManager::loadLanguage(Util::getLanguagesPath() + languageFile);
		if (languageList.size() != 1)
			MessageBox(CTSTRING(CHANGE_LANGUAGE_INFO), CTSTRING(CHANGE_LANGUAGE), MB_OK | MB_ICONEXCLAMATION);
	}
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

	auto ss = SettingsManager::instance.getCoreSettings();
	ss->lockRead();
	const string uploadSpeed = ss->getString(Conf::UPLOAD_SPEED);
	const string defUploadSpeed = ss->getStringDefault(Conf::UPLOAD_SPEED);
	const string languageFile = ss->getString(Conf::LANGUAGE_FILE);
	const int gender = ss->getInt(Conf::GENDER);
	const string defaultCodePage = ss->getString(Conf::DEFAULT_CODEPAGE);
	const bool overrideClientId = ss->getBool(Conf::OVERRIDE_CLIENT_ID);
	ss->unlockRead();

	CComboBox ctrlConnection(GetDlgItem(IDC_CONNECTION));
	int selIndex = -1, defIndex = -1;
	for (int i = 0; i < _countof(connSpeeds); ++i)
	{
		if (defIndex < 0 && defUploadSpeed == connSpeeds[i])
			defIndex = i;
		if (selIndex < 0 && uploadSpeed == connSpeeds[i])
			selIndex = i;
		tstring s = Text::toT(connSpeeds[i]);
		s += _T(' ');
		s += TSTRING(MBITPS);
		ctrlConnection.AddString(s.c_str());
	}
	if (selIndex < 0)
		selIndex = defIndex;
	ctrlConnection.SetCurSel(selIndex);
	
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
	
	CComboBox ctrlLanguage(GetDlgItem(IDC_LANGUAGE));
	getLangList();
	for (auto i = languageList.cbegin(); i != languageList.cend(); ++i)
	{
		int index = ctrlLanguage.AddString(Text::toT(i->language).c_str());
		ctrlLanguage.SetItemDataPtr(index, const_cast<char*>(i->filename.c_str()));
	}

	selIndex = defIndex = -1;
	int itemCount = ctrlLanguage.GetCount();
	for (int i = 0; i < itemCount; ++i)
	{
		const char* text = static_cast<const char*>(ctrlLanguage.GetItemDataPtr(i));
		if (languageFile == text)
		{
			selIndex = i;
			break;
		}
		if (defIndex < 0 && defLangFileName == text)
			defIndex = i;
	}

	if (selIndex < 0)
		selIndex = defIndex;
	ctrlLanguage.SetCurSel(selIndex);

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
	ctrlGender.SetCurSel(gender);

	int charset = Text::charsetFromString(defaultCodePage);
	CComboBox comboBox(GetDlgItem(IDC_ENCODING));
	WinUtil::fillCharsetList(comboBox, charset, false, false);

	comboClientId.EnableWindow(overrideClientId ? TRUE : FALSE);

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
		const StringList files = File::findFiles(Util::getLanguagesPath(), "*-*.xml");
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
