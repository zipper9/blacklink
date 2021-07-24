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

#include "../client/BZUtils.h"
#include "../client/FilteredFile.h"
#include "../client/HashUtil.h"
#include "../client/HashManager.h"
#include "../client/ShareManager.h"
#include "../client/DatabaseManager.h"
#include "../client/LogManager.h"
#include "DclstGenDlg.h"
#include "WinUtil.h"

static const WinUtil::TextItem texts[] =
{
	{ IDC_DCLSTGEN_INFO_SIZESTATIC,    ResourceManager::DCLSTGEN_INFO_SIZESTATIC    },
	{ IDC_DCLSTGEN_INFO_FOLDERSSTATIC, ResourceManager::DCLSTGEN_INFO_FOLDERSSTATIC },
	{ IDC_DCLSTGEN_INFO_FILESSTATIC,   ResourceManager::DCLSTGEN_INFO_FILESSTATIC   },
	{ IDC_DCLSTGEN_NAMEBORDER,         ResourceManager::DCLSTGEN_NAMEBORDER         },
	{ IDC_DCLSTGEN_NAMESTATIC,         ResourceManager::MAGNET_DLG_FILE             },
	{ IDC_DCLSTGEN_SAVEAS,             ResourceManager::DCLSTGEN_RENAMEAS           },
	{ IDC_DCLSTGEN_SHARE,              ResourceManager::DCLSTGEN_SHARE              },
	{ IDC_DCLSTGEN_COPYMAGNET,         ResourceManager::COPY_MAGNET                 },
	{ IDCANCEL,                        ResourceManager::CANCEL                      },
	{ 0,                               ResourceManager::Strings()                   }
};

LRESULT DclstGenDlg::onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	if (dir == nullptr || user == nullptr)
		return TRUE;		
		
	SetWindowText(CTSTRING(DCLSTGEN_TITLE));
	CenterWindow(GetParent());
	
	WinUtil::translate(*this, texts);

	HICON dialogIcon = g_iconBitmaps.getIcon(IconBitmaps::DCLST, 0);
	SetIcon(dialogIcon, FALSE);
	SetIcon(dialogIcon, TRUE);
	
	GetDlgItem(IDC_DCLSTGEN_SAVEAS).EnableWindow(FALSE);
	GetDlgItem(IDC_DCLSTGEN_SHARE).EnableWindow(FALSE);
	GetDlgItem(IDC_DCLSTGEN_COPYMAGNET).EnableWindow(FALSE);
	
	CProgressBarCtrl(GetDlgItem(IDC_DCLSTGEN_PROGRESS)).SetRange32(0, 100);
	
	std::string fileName = dir->getName();
	if (fileName.empty())
		fileName = user->getLastNick();
	listName = getDclstName(fileName);
	SetDlgItemText(IDC_DCLSTGEN_NAME, Text::toT(listName).c_str());
	
	updateDialogItems();
	
	createTimer(1000);
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
	
	GetDlgItem(IDC_DCLSTGEN_SAVEAS).EnableWindow(TRUE);
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
	auto currentSize = sizeProcessed;
	auto currentFiles = filesProcessed;
	auto currentFolders = foldersProcessed;
	cs.unlock();

	SetDlgItemText(IDC_DCLST_GEN_INFOEDIT, Util::formatBytesT(currentSize).c_str());
	SetDlgItemText(IDC_DCLSTGEN_INFO_FOLDERSEDIT, Util::toStringT(currentFolders).c_str());
	SetDlgItemText(IDC_DCLSTGEN_INFO_FILESEDIT, Util::toStringT(currentFiles).c_str());
	
	size_t totalFileCount = dir->getTotalFileCount();
	CProgressBarCtrl(GetDlgItem(IDC_DCLSTGEN_PROGRESS)).SetPos(int((double) currentFiles * 100.0 / totalFileCount));
}

string DclstGenDlg::getDclstName(const string& folderName)
{
	string folderDirName;
	if (!BOOLSETTING(DCLST_CREATE_IN_SAME_FOLDER) && !SETTING(DCLST_DIRECTORY).empty())
		folderDirName = SETTING(DCLST_DIRECTORY);
	else
		folderDirName = Util::getDownloadDir(UserPtr());
	folderDirName += Util::validateFileName(folderName);
	folderDirName += ".dcls";
	string retValue = File::isExist(folderDirName) ? Util::getFilenameForRenaming(folderDirName) : folderDirName;
	return retValue;
}

int DclstGenDlg::run()
{
	writeXMLStart();
	writeFolder(dir);
	
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
	xml += "<FileListing Version=\"2\" CID=\"" + user->getCID().toBase32() + "\" Generator=\"DC++ " DCVERSIONSTRING "\"";
	if (BOOLSETTING(DCLST_INCLUDESELF))
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
		dcassert(*i);
		writeFile(*i);
		if (abortFlag.load()) return;
	}
	
	xml += "</Directory>\r\n";
}

void DclstGenDlg::writeFile(const DirectoryListing::File* file)
{
	if (abortFlag.load()) return;
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
	const DirectoryListing::MediaInfo *media = file->getMedia();
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
		LogManager::message("DCLSTGenerator::File error = " + ex.getError() + " File = " + listName, false);
		MessageBox(CTSTRING(DCLSTGEN_METAFILECANNOTBECREATED), CTSTRING(DCLSTGEN_TITLE), MB_OK | MB_ICONERROR);
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
	         "&xl=" + Util::toString(listTree.getFileSize()) + "&dn=" + Util::encodeURI(Util::getFileName(listName)) + "&dl=" + Util::toString(sizeProcessed);
}

LRESULT DclstGenDlg::onShareThis(WORD /*wNotifyCode*/, WORD /*wID*/, HWND hWndCtl, BOOL& /*bHandled*/)
{
	if (!magnet.empty())
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
		DatabaseManager::getInstance()->addTree(listTree);
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
