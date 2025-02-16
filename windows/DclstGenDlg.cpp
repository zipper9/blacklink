/*
 * Copyright (C) 2011-2017 FlylinkDC++ Team http://flylinkdc.com
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

#include "DclstGenDlg.h"
#include "DialogLayout.h"
#include "ImageLists.h"
#include "WinUtil.h"
#include "BrowseFile.h"
#include "ConfUI.h"
#include "../client/SettingsManager.h"
#include "../client/BZUtils.h"
#include "../client/FilteredFile.h"
#include "../client/HashUtil.h"
#include "../client/HashManager.h"
#include "../client/ShareManager.h"
#include "../client/DatabaseManager.h"
#include "../client/PathUtil.h"
#include "../client/FormatUtil.h"
#include "../client/Util.h"
#include "../client/LogManager.h"
#include "../client/version.h"

using DialogLayout::FLAG_TRANSLATE;
using DialogLayout::UNSPEC;
using DialogLayout::AUTO;

static const DialogLayout::Align align1 = { 1, DialogLayout::SIDE_RIGHT, U_DU(6) };

static const DialogLayout::Item layoutItems[] =
{
	{ IDC_DCLSTGEN_INFO_SIZESTATIC, FLAG_TRANSLATE, AUTO, UNSPEC },
	{ IDC_DCLSTGEN_INFO_SIZEEDIT, 0, UNSPEC, UNSPEC, 0, &align1 },
	{ IDC_DCLSTGEN_INFO_FILES, 0, UNSPEC, UNSPEC, 0, &align1 },
	{ IDC_DCLSTGEN_NAMEBORDER, FLAG_TRANSLATE, UNSPEC, UNSPEC },
	{ IDC_DCLSTGEN_NAMESTATIC, FLAG_TRANSLATE, UNSPEC, UNSPEC },
	{ IDC_DCLSTGEN_SAVEAS, FLAG_TRANSLATE, UNSPEC, UNSPEC },
	{ IDC_DCLSTGEN_COPYMAGNET, FLAG_TRANSLATE, UNSPEC, UNSPEC },
	{ IDCANCEL, FLAG_TRANSLATE, UNSPEC, UNSPEC }
};

const string& DclstGenDlg::getDcLstDirectory()
{
	auto ss = SettingsManager::instance.getUiSettings();
	if (ss->getBool(Conf::DCLST_CREATE_IN_SAME_FOLDER))
		return Util::emptyString;
	return ss->getString(Conf::DCLST_DIRECTORY);
}

static string validateFileName(const string& name)
{
	if (name.length() == 3 && name[1] == ':' && name[2] == '\\')
		return name.substr(0, 1);
	return Util::validateFileName(name);
}

LRESULT DclstGenDlg::onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	SetWindowText(CTSTRING(DCLSTGEN_TITLE));
	CenterWindow(GetParent());

	DialogLayout::layout(m_hWnd, layoutItems, _countof(layoutItems));

	HICON dialogIcon = g_iconBitmaps.getIcon(IconBitmaps::DCLST, 0);
	SetIcon(dialogIcon, FALSE);
	SetIcon(dialogIcon, TRUE);

	GetDlgItem(IDC_DCLSTGEN_SAVEAS).EnableWindow(FALSE);
	GetDlgItem(IDC_DCLSTGEN_SHARE).EnableWindow(FALSE);
	GetDlgItem(IDC_DCLSTGEN_COPYMAGNET).EnableWindow(FALSE);

	CProgressBarCtrl(GetDlgItem(IDC_DCLSTGEN_PROGRESS)).SetRange32(0, 100);

	if (dir)
	{
		string fileName = dir->getName();
		if (fileName.empty() && user)
			fileName = user->getLastNick();
		listName = getDcLstDirectory();
		if (listName.empty())
			listName = Util::getDownloadDir(UserPtr());
		listName += validateFileName(fileName);
		SetDlgItemText(IDC_DCLSTGEN_SHARE, CTSTRING(DCLSTGEN_SHARE));
	}
	else
	{
		calculatingSize = true;
		Util::appendPathSeparator(dirToHash);
		string fileName = Util::getLastDir(dirToHash);
		listName = getDcLstDirectory();
		if (listName.empty())
			listName = dirToHash;
		listName += validateFileName(fileName);
		SetDlgItemText(IDC_DCLSTGEN_SHARE, CTSTRING(DCLSTGEN_OPEN));
	}
	listName += ".dcls";
	if (File::isExist(listName))
		listName = Util::getNewFileName(listName);

	SetDlgItemText(IDC_DCLSTGEN_NAME, Text::toT(listName).c_str());
	updateDialogItems();

	includeSelf = SettingsManager::instance.getUiSettings()->getBool(Conf::DCLST_INCLUDESELF);

	createTimer(500);
	start(0, "DclstGenDlg");
	return 0;
}

LRESULT DclstGenDlg::onCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	abortFlag.store(true);
	join();
	destroyTimer();
	EndDialog(wID);
	return 0;
}

LRESULT DclstGenDlg::onTimer(UINT uMsg, WPARAM wParam, LPARAM /*lParam*/, BOOL& bHandled)
{
	if (!checkTimerID(wParam))
	{
		bHandled = FALSE;
		return 0;
	}
	updateDialogItems();
	return 0;
}

LRESULT DclstGenDlg::onFinished(UINT uMsg, WPARAM wParam, LPARAM /*lParam*/, BOOL& bHandled)
{
	destroyTimer();
	updateDialogItems();

	SetDlgItemText(IDC_DCLSTGEN_MAGNET, Text::toT(magnet).c_str());
	SetDlgItemText(IDCANCEL, CTSTRING(OK));

	if (dir) GetDlgItem(IDC_DCLSTGEN_SAVEAS).EnableWindow(TRUE);
	GetDlgItem(IDC_DCLSTGEN_SHARE).EnableWindow(TRUE);
	GetDlgItem(IDC_DCLSTGEN_COPYMAGNET).EnableWindow(TRUE);
	return 0;
}

LRESULT DclstGenDlg::onCopyMagnet(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (!magnet.empty())
		WinUtil::setClipboard(magnet);
	return 0;
}

void DclstGenDlg::updateDialogItems()
{
	cs.lock();
	bool calculatingSize = this->calculatingSize;
	auto currentTotalSize = sizeTotal;
	auto currentSize = sizeProcessed;
	auto currentFiles = filesProcessed;
	auto currentFolders = foldersProcessed;
	cs.unlock();

	tstring text;
	if (dir)
		text = Util::formatBytesT(currentSize);
	else if (calculatingSize)
		text = Util::formatBytesT(currentTotalSize);
	else
		text = Util::formatBytesT(currentSize) + _T(" / ") + Util::formatBytesT(currentTotalSize);
	SetDlgItemText(IDC_DCLSTGEN_INFO_SIZEEDIT, text.c_str());
	text = TSTRING_F(DCLSTGEN_INFO_FILES, currentFolders % currentFiles);
	SetDlgItemText(IDC_DCLSTGEN_INFO_FILES, text.c_str());

	int progress;
	if (dir)
	{
		size_t totalFileCount = dir->getTotalFileCount();
		if (!totalFileCount)
			progress = 100;
		else
			progress = int((double) currentFiles * 100.0 / totalFileCount);
	}
	else
	{
		if (calculatingSize || !currentTotalSize)
			progress = 0;
		else
			progress = int((double) currentSize * 100.0 / currentTotalSize);
	}
	CProgressBarCtrl(GetDlgItem(IDC_DCLSTGEN_PROGRESS)).SetPos(progress);
}

int DclstGenDlg::run()
{
	if (!dir)
	{
		uint64_t result = Util::getDirSize(dirToHash, abortFlag, progressFunc, this);
		if (abortFlag.load())
			return 0;
		cs.lock();
		sizeTotal = result;
		calculatingSize = false;
		cs.unlock();
	}

	writeXMLStart();

	if (dir)
		writeFolder(dir);
	else
		hashFolder(dirToHash, validateFileName(Util::getLastDir(dirToHash)));

	if (abortFlag.load())
		return 0;

	writeXMLEnd();
	packAndSave();
	if (calculateTTH())
		PostMessage(WM_FINISHED, 0, 0);
	return 0;
}

void DclstGenDlg::writeXMLStart()
{
	xml = SimpleXML::utf8Header;
	CID cid = user ? user->getCID() : CID::generate();
	xml += "<FileListing Version=\"2\" CID=\"" + cid.toBase32() + "\" Generator=\"DC++ " DCVERSIONSTRING "\"";
	if (includeSelf)
		xml += " IncludeSelf=\"1\"";
	xml += ">\r\n";
}

void DclstGenDlg::writeXMLEnd()
{
	xml += "</FileListing>\r\n";
}

void DclstGenDlg::writeFolder(const DirectoryListing::Directory* dir)
{
	if (abortFlag.load()) return;

	cs.lock();
	foldersProcessed++;
	cs.unlock();

	string dirName = dir->getName();
	xml += "<Directory Name=\"" + SimpleXML::escape(dirName, true) + "\">\r\n";

	for (auto i = dir->directories.cbegin(); i != dir->directories.cend(); ++i)
	{
		dcassert(*i);
		writeFolder(*i);
		if (abortFlag.load()) return;
	}

	for (auto i = dir->files.cbegin(); i != dir->files.cend(); ++i)
	{
		const DirectoryListing::File* f = *i;
		dcassert(f);
		if (!f->isAnySet(DirectoryListing::FLAG_DCLST_SELF)) writeFile(f);
		if (abortFlag.load()) return;
	}

	xml += "</Directory>\r\n";
}

void DclstGenDlg::writeFile(const DirectoryListing::File* file)
{
	cs.lock();
	filesProcessed++;
	sizeProcessed += file->getSize();
	cs.unlock();
	string fileName = file->getName();
	xml += "<File Name=\"" + SimpleXML::escape(fileName, true) + "\" Size=\"" + Util::toString(file->getSize()) + "\" TTH=\"" + file->getTTH().toBase32() + '\"';

	if (file->getTS())
	{
		xml += " TS=\"" + Util::toString(file->getTS()) + '\"';
	}
	const MediaInfoUtil::Info *media = file->getMedia();
	if (media)
	{
		if (media->bitrate)
		{
			xml += " BR=\"" + Util::toString(media->bitrate) + '\"';
		}
		if (media->width && media->height)
		{
			char buf[64];
			int result = sprintf(buf, "%ux%u", media->width, media->height);
			xml += " WH=\"" + string(buf, result) + '\"';
		}
		if (!media->audio.empty())
		{
			string audio = media->audio;
			xml += " MA=\"" + SimpleXML::escape(audio, true) + '\"';
		}
		if (!media->video.empty())
		{
			string video = media->video;
			xml += " MV=\"" + SimpleXML::escape(video, true) + '\"';
		}
	}
	xml += "/>\r\n";
}

void DclstGenDlg::hashFolder(const string& path, const string& dirName)
{
	dcassert(!path.empty() && path.back() == PATH_SEPARATOR);

	cs.lock();
	foldersProcessed++;
	cs.unlock();

	string tmp;
	xml += "<Directory Name=\"" + SimpleXML::escape(dirName, tmp, true) + "\">\r\n";
	string filePath = path;
	filePath += '*';
	for (FileFindIter i(filePath); i != FileFindIter::end; ++i)
	{
		if (abortFlag) break;
		const string& fileName = i->getFileName();
		if (i->isDirectory())
		{
			if (Util::isReservedDirName(fileName)) continue;
			filePath.erase(path.length());
			filePath += fileName;
			filePath += PATH_SEPARATOR;
			hashFolder(filePath, fileName);
		}
		else
		{
			if (i->isTemporary()) continue;
			filePath.erase(path.length());
			filePath += fileName;
			hashFile(filePath);
		}
	}

	xml += "</Directory>\r\n";
}

void DclstGenDlg::hashFile(const string& path)
{
	TigerTree tree;
	if (!Util::getTTH(path, true, 1024*1024, abortFlag, tree)) return;
	cs.lock();
	filesProcessed++;
	sizeProcessed += tree.getFileSize();
	cs.unlock();
	string fileName = Util::getFileName(path);
	xml += "<File Name=\"" + SimpleXML::escape(fileName, true) + "\" Size=\"" + Util::toString(tree.getFileSize()) + "\" TTH=\"" + tree.getRoot().toBase32() + "\"/>\r\n";
}

size_t DclstGenDlg::packAndSave()
{
	size_t outSize = 0;
	try
	{
		unique_ptr<OutputStream> outFilePtr(new File(listName, File::WRITE, File::TRUNCATE | File::CREATE, false));
		FilteredOutputStream<BZFilter, false> outFile(outFilePtr.get());
		outSize += outFile.write(xml.c_str(), xml.size());
		outSize += outFile.flushBuffers(true);
	}
	catch (const FileException& ex)
	{
		tstring error = Text::toT(ex.getError());
		MessageBox(CTSTRING_F(DCLSTGEN_ERROR, error), CTSTRING(DCLSTGEN_TITLE), MB_OK | MB_ICONERROR);
	}
	return outSize;
}

bool DclstGenDlg::calculateTTH()
{
	if (Util::getTTH(listName, true, 1024*1024, abortFlag, listTree) && listTree.getFileSize() > 0)
	{
		makeMagnet();
		return true;
	}
	File::deleteFile(listName);
	return false;
}

void DclstGenDlg::makeMagnet()
{
	magnet = "magnet:?xt=urn:tree:tiger:" + listTree.getRoot().toBase32() +
	         "&xl=" + Util::toString(listTree.getFileSize()) + "&dn=" + Util::encodeUriQuery(Util::getFileName(listName)) + "&dl=" + Util::toString(sizeProcessed);
}

LRESULT DclstGenDlg::onShareOrOpen(WORD /*wNotifyCode*/, WORD /*wID*/, HWND hWndCtl, BOOL& /*bHandled*/)
{
	if (!dir)
	{
		WinUtil::openFileList(Text::toT(listName));
		EndDialog(IDOK);
	}
	else if (!magnet.empty())
	{
		string listDir = Util::getFilePath(listName);
		auto sm = ShareManager::getInstance();
		if (!sm->isDirectoryShared(listDir))
		{
			MessageBox(CTSTRING(DCLSTGEN_METAFILECANNOTBESHARED), CTSTRING(DCLSTGEN_TITLE), MB_OK | MB_ICONERROR);
			return 0;
		}
		try
		{
			sm->addFile(listName, listTree.getRoot());
		}
		catch (ShareException& e)
		{
			MessageBox(Text::toT(e.getError()).c_str(), CTSTRING(DCLSTGEN_TITLE), MB_OK | MB_ICONERROR);
			return 0;
		}
		auto db = DatabaseManager::getInstance();
		auto hashDb = db->getHashDatabaseConnection();
		if (hashDb)
		{
			db->addTree(hashDb, listTree);
			db->putHashDatabaseConnection(hashDb);
		}
		MessageBox(CTSTRING(DCLSTGEN_METAFILEREADY), CTSTRING(DCLSTGEN_TITLE), MB_OK | MB_ICONINFORMATION);
		CButton(hWndCtl).EnableWindow(FALSE);
	}
	return 0;
}

LRESULT DclstGenDlg::onSaveAs(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	tstring targetOld = Text::toT(listName);
	tstring target = targetOld;
	static const TCHAR defaultExt[] = _T("dcls");
	static const WinUtil::FileMaskItem mask[] =
	{
		{ ResourceManager::FILEMASK_DCLST, _T("*.dcls;*.dclst") },
		{ ResourceManager::FILEMASK_ALL,   _T("*.*")            },
		{ ResourceManager::Strings(),      nullptr              }
	};
	if (WinUtil::browseFile(target, *this, true, Util::emptyStringT, WinUtil::getFileMaskString(mask).c_str(), defaultExt))
	{
		if (File::renameFile(targetOld, target))
		{
			listName = Text::fromT(target);
			makeMagnet();

			SetDlgItemText(IDC_DCLSTGEN_MAGNET, Text::toT(magnet).c_str());
			SetDlgItemText(IDC_DCLSTGEN_NAME, target.c_str());
			GetDlgItem(IDC_DCLSTGEN_SHARE).EnableWindow(TRUE);
		}
		else
			MessageBox(CTSTRING(DCLSTGEN_METAFILECANNOTMOVED), CTSTRING(DCLSTGEN_TITLE), MB_OK | MB_ICONERROR);
	}
	return 0;
}

void DclstGenDlg::progressFunc(void *ctx, int64_t fileSize)
{
	DclstGenDlg* dlg = static_cast<DclstGenDlg*>(ctx);
	LOCK(dlg->cs);
	dlg->sizeTotal += fileSize;
}
