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

#ifndef TRANSFER_VIEW_H
#define TRANSFER_VIEW_H

#include "../client/DownloadManagerListener.h"
#include "../client/UploadManagerListener.h"
#include "../client/ConnectionManagerListener.h"
#include "../client/QueueManagerListener.h"
#include "../client/forward.h"
#include "../client/Util.h"
#include "../client/Download.h"
#include "../client/Upload.h"
#include "../client/TaskQueue.h"

#include "OMenu.h"
#include "UCHandler.h"
#include "TypedTreeListViewCtrl.h"
#include "SearchFrm.h"
#include "TimerHelper.h"
#include "CustomDrawHelpers.h"
#include "BaseHandlers.h"

class TransferView : public CWindowImpl<TransferView>, private DownloadManagerListener,
	private UploadManagerListener,
	private ConnectionManagerListener,
	private QueueManagerListener,
	public UserInfoBaseHandler<TransferView, UserInfoGuiTraits::NO_COPY>,
	public PreviewBaseHandler,
	public UCHandler<TransferView>,
	private SettingsManagerListener
{
	public:
		DECLARE_WND_CLASS(_T("TransferView"))
		
		TransferView();
		~TransferView();
		
		typedef UserInfoBaseHandler<TransferView, UserInfoGuiTraits::NO_COPY> uibBase;
		typedef UCHandler<TransferView> ucBase;
		
		BEGIN_MSG_MAP(TransferView)
		NOTIFY_HANDLER(IDC_TRANSFERS, LVN_GETDISPINFO, ctrlTransfers.onGetDispInfo)
		NOTIFY_HANDLER(IDC_TRANSFERS, LVN_COLUMNCLICK, ctrlTransfers.onColumnClick)
		NOTIFY_HANDLER(IDC_TRANSFERS, LVN_GETINFOTIP, ctrlTransfers.onInfoTip)
		NOTIFY_HANDLER(IDC_TRANSFERS, LVN_KEYDOWN, onKeyDownTransfers)
		NOTIFY_HANDLER(IDC_TRANSFERS, NM_CUSTOMDRAW, onCustomDraw)
		NOTIFY_HANDLER(IDC_TRANSFERS, NM_DBLCLK, onDoubleClickTransfers)
		MESSAGE_HANDLER(WM_CREATE, onCreate)
		MESSAGE_HANDLER(WM_TIMER, onTimer)
		MESSAGE_HANDLER(WM_DESTROY, onDestroy)
		MESSAGE_HANDLER(WM_SPEAKER, onSpeaker)
		MESSAGE_HANDLER(WM_CONTEXTMENU, onContextMenu)
		MESSAGE_HANDLER(WM_SIZE, onSize)
		MESSAGE_HANDLER(WM_NOTIFYFORMAT, onNotifyFormat)
#ifdef IRAINMAN_ENABLE_WHOIS
		COMMAND_ID_HANDLER(IDC_WHOIS_IP, onWhoisIP)
		COMMAND_ID_HANDLER(IDC_WHOIS_IP2, onWhoisIP)
#endif
		COMMAND_ID_HANDLER(IDC_FORCE, onForce)
		COMMAND_ID_HANDLER(IDC_SEARCH_ALTERNATES, onSearchAlternates)
		COMMAND_ID_HANDLER(IDC_ADD_P2P_GUARD, onAddP2PGuard)
		
		COMMAND_ID_HANDLER(IDC_REMOVE, onRemove)
		COMMAND_ID_HANDLER(IDC_REMOVEALL, onRemoveAll)
		COMMAND_ID_HANDLER(IDC_DISCONNECT_ALL, onDisconnectAll)
		COMMAND_ID_HANDLER(IDC_COLLAPSE_ALL, onCollapseAll)
		COMMAND_ID_HANDLER(IDC_EXPAND_ALL, onExpandAll)
		COMMAND_ID_HANDLER(IDC_MENU_SLOWDISCONNECT, onSlowDisconnect)
		
		MESSAGE_HANDLER_HWND(WM_INITMENUPOPUP, OMenu::onInitMenuPopup)
		MESSAGE_HANDLER_HWND(WM_MEASUREITEM, OMenu::onMeasureItem)
		MESSAGE_HANDLER_HWND(WM_DRAWITEM, OMenu::onDrawItem)
		COMMAND_ID_HANDLER(IDC_PRIORITY_PAUSED, onPauseSelectedItem)
		COMMAND_ID_HANDLER(IDC_UPLOAD_QUEUE, onOpenWindows)
		COMMAND_ID_HANDLER(IDC_QUEUE, onOpenWindows)
		COMMAND_ID_HANDLER(IDC_COPY_TTH, onCopy)
		COMMAND_ID_HANDLER(IDC_COPY_LINK, onCopy)
		COMMAND_ID_HANDLER(IDC_COPY_WMLINK, onCopy)
		COMMAND_RANGE_HANDLER(IDC_COPY, IDC_COPY + COLUMN_LAST - 1, onCopy)
		
		CHAIN_COMMANDS(ucBase)
		CHAIN_COMMANDS(uibBase)
		CHAIN_COMMANDS(PreviewBaseHandler)
		END_MSG_MAP()
		
		LRESULT onCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT onContextMenu(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& bHandled);
		LRESULT onSize(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& bHandled);
		LRESULT onForce(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onSearchAlternates(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onCustomDraw(int /*idCtrl*/, LPNMHDR pnmh, BOOL& bHandled);
		LRESULT onDoubleClickTransfers(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/);
		LRESULT onDisconnectAll(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onSlowDisconnect(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onPreviewCommand(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onOpenWindows(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		
#ifdef IRAINMAN_ENABLE_WHOIS
		LRESULT onWhoisIP(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
#endif
		void runUserCommand(UserCommand& uc);
		void prepareClose();
		LRESULT onCollapseAll(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			collapseAll();
			return 0;
		}
		
		LRESULT onExpandAll(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			expandAll();
			return 0;
		}
		
		LRESULT onKeyDownTransfers(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/)
		{
			NMLVKEYDOWN* kd = (NMLVKEYDOWN*) pnmh;
			if (kd->wVKey == VK_DELETE)
			{
				ctrlTransfers.forEachSelected(&ItemInfo::disconnect);
			}
			return 0;
		}
		
		LRESULT onAddP2PGuard(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			ctrlTransfers.forEachSelected(&ItemInfo::disconnectAndBlock);
			return 0;
		}
		
		LRESULT onRemove(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			ctrlTransfers.forEachSelected(&ItemInfo::disconnect);
			return 0;
		}
		
		LRESULT onRemoveAll(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		
		LRESULT onDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
		{
			timer.destroyTimer();
			ctrlTransfers.deleteAll();
			return 0;
		}
		
		LRESULT onNotifyFormat(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
		{
#ifdef _UNICODE
			return NFR_UNICODE;
#else
			return NFR_ANSI;
#endif
		}
		
		LRESULT onPauseSelectedItem(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			pauseSelectedTransfer();
			return 0;
		}

		LRESULT onCopy(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

		LRESULT onTimer(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& bHandled)
		{
			if (!timer.checkTimerID(wParam))
			{
				bHandled = FALSE;
				return 0;
			}
			onTimerInternal();
			return 0;
		}
		
		LRESULT onSpeaker(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/)
		{
			processTasks();
			return 0;
		}
		
	public:
		class ItemInfo;
		typedef TypedTreeListViewCtrl<ItemInfo, IDC_TRANSFERS, tstring> ItemInfoList;
		ItemInfoList& getUserList()
		{
			return ctrlTransfers;
		}
#ifdef IRAINMAN_ENABLE_WHOIS
		tstring selectedIP;
#endif
	private:
		enum Tasks
		{
			TRANSFER_ADD_ITEM,
			TRANSFER_REMOVE_ITEM,
			TRANSFER_UPDATE_ITEM,
			TRANSFER_UPDATE_PARENT,
			TRANSFER_UPDATE_TOKEN_ITEM,
			TRANSFER_REMOVE_DOWNLOAD_ITEM,
			TRANSFER_REMOVE_TOKEN_ITEM
		};
		
		enum
		{
			COLUMN_FIRST,
			COLUMN_USER = COLUMN_FIRST,
			COLUMN_HUB,
			COLUMN_STATUS,
			COLUMN_TIMELEFT,
			COLUMN_SPEED,
			COLUMN_FILE,
			COLUMN_SIZE,
			COLUMN_PATH,
			COLUMN_CIPHER,
			COLUMN_LOCATION,
			COLUMN_IP,
#ifdef FLYLINKDC_USE_COLUMN_RATIO
			COLUMN_RATIO,
#endif
			COLUMN_SHARE,
			COLUMN_SLOTS,
			COLUMN_P2P_GUARD,
			COLUMN_LAST
		};
		
		enum
		{
			IMAGE_DOWNLOAD = 0,
			IMAGE_UPLOAD,
			IMAGE_SEGMENT
		};
		struct UpdateInfo;

	public:
		class ItemInfo : public UserInfoBase
		{
			public:
#ifdef _DEBUG
				static std::atomic<long> g_count_transfer_item;
#endif
				enum Status
				{
					STATUS_RUNNING,
					STATUS_WAITING
				};
				
				ItemInfo();
				ItemInfo(const UpdateInfo& ui);
#ifdef _DEBUG
				virtual ~ItemInfo()
				{
					--g_count_transfer_item;
				}
#endif
				const bool download;
				TTHValue tth;

				bool transferFailed;
				bool collapsed;

				int16_t running;
				int16_t hits;

				ItemInfo* parent;
				HintedUser hintedUser;
				Status status;
				Transfer::Type type;

				int64_t pos;
				int64_t size;
				int64_t actual;
				int64_t speed;
				int64_t timeLeft;
				IpAddress transferIp;
				tstring statusString;
				tstring errorStatusString;
				tstring cipher;
				tstring target;
				tstring nicks;
				tstring hubs;
				string  token;
				
				mutable IPInfo ipInfo;
				
#ifdef FLYLINKDC_USE_COLUMN_RATIO
				tstring ratioText;
#endif
				
				void update(const UpdateInfo& ui);
				void updateNicks();
				const UserPtr& getUser() const
				{
					return hintedUser.user;
				}
				
				void disconnect();
				void disconnectAndBlock();
				void removeAll();
				
				// FIXME
				double getProgressPosition() const
				{
					return (pos > 0) ? (double) actual / (double) pos : 1.0;
				}
				int64_t getPos() const;
				const tstring getText(uint8_t col) const;
				static int compareItems(const ItemInfo* a, const ItemInfo* b, uint8_t col);
				
				uint8_t getImageIndex() const
				{
					return static_cast<uint8_t>(download ? (!parent ? IMAGE_DOWNLOAD : IMAGE_SEGMENT) : IMAGE_UPLOAD);
				}
				static int getStateImageIndex() { return 0; }
				ItemInfo* createParent()
				{
					dcassert(download);
					ItemInfo* ii = new ItemInfo;
					ii->hits = 0;
					ii->statusString = TSTRING(CONNECTING);
					ii->target = target;
					ii->errorStatusString = errorStatusString;
					return ii;
				}
				const tstring& getGroupCond() const
				{
					return target;
				}

			private:
				void init();
		};
		
	private:
		struct UpdateInfo : public Task
		{
			enum
			{
				MASK_POS                 = 0x0001,
				MASK_SIZE                = 0x0002,
				MASK_ACTUAL              = 0x0004,
				MASK_SPEED               = 0x0008,
				MASK_FILE                = 0x0010,
				MASK_STATUS              = 0x0020,
				MASK_TIMELEFT            = 0x0040,
				MASK_IP                  = 0x0080,
				MASK_STATUS_STRING       = 0x0100,
				MASK_SEGMENT             = 0x0200,
				MASK_CIPHER              = 0x0400,
				MASK_USER                = 0x0800,
				MASK_ERROR_STATUS_STRING = 0x2000,
				MASK_TOKEN               = 0x4000,
				MASK_TTH                 = 0x8000
			};
			
			bool operator==(const ItemInfo& ii) const
			{
				return  download == ii.download && hintedUser.user == ii.hintedUser.user;
			}
			UpdateInfo(const HintedUser& user, bool isDownload, bool isTransferFailed = false) :
				updateMask(0), download(isDownload), hintedUser(user), // fix empty string
				transferFailed(isTransferFailed),
				type(Transfer::TYPE_LAST), running(0),
				status(ItemInfo::STATUS_WAITING), pos(0), size(0), actual(0), speed(0), timeLeft(0)
			{
			}
			UpdateInfo() :
				updateMask(0), download(true), transferFailed(false),
				type(Transfer::TYPE_LAST), running(0),
				status(ItemInfo::STATUS_WAITING), pos(0), size(0), actual(0), speed(0), timeLeft(0)
			{
			}
			
			const bool download;
			const bool transferFailed;
			uint32_t updateMask;

			HintedUser hintedUser;
			void setHintedUser(const HintedUser& user)
			{
				hintedUser = user;
				updateMask |= MASK_USER;
			}

			int16_t running;
			void setRunning(int16_t running)
			{
				this->running = running;
				updateMask |= MASK_SEGMENT;
			}

			ItemInfo::Status status;
			void setStatus(ItemInfo::Status status)
			{
				this->status = status;
				updateMask |= MASK_STATUS;
			}

			int64_t pos;
			void setPos(int64_t pos)
			{
				this->pos = pos;
				updateMask |= MASK_POS;
			}

			int64_t size;
			void setSize(int64_t size)
			{
				this->size = size;
				updateMask |= MASK_SIZE;
			}

			int64_t actual;
			void setActual(int64_t actual)
			{
				this->actual = actual;
				updateMask |= MASK_ACTUAL;
			}

			string token;
			void setToken(const string& token)
			{
				dcassert(!token.empty());
				this->token = token;
				updateMask |= MASK_TOKEN;
			}

			int64_t speed;
			void setSpeed(int64_t speed)
			{
				this->speed = speed;
				updateMask |= MASK_SPEED;
			}

			int64_t timeLeft;
			void setTimeLeft(int64_t timeLeft)
			{
				this->timeLeft = timeLeft;
				updateMask |= MASK_TIMELEFT;
			}

			tstring errorStatusString;
			void setErrorStatusString(const tstring& str)
			{
				errorStatusString = str;
				updateMask |= MASK_ERROR_STATUS_STRING;
			}

			tstring statusString;
			void setStatusString(const tstring& str)
			{
				statusString = str;
				updateMask |= MASK_STATUS_STRING;
			}

			tstring target;
			void setTarget(const string& target)
			{
				this->target = Text::toT(target);
				updateMask |= MASK_FILE;
			}

			tstring cipher;
			void setCipher(const tstring& cipher)
			{
				this->cipher = cipher;
				updateMask |= MASK_CIPHER;
			}

			Transfer::Type type;
			void setType(Transfer::Type type)
			{
				this->type = type;
			}

			TTHValue tth;
			void setTTH(const TTHValue& tth)
			{
				this->tth = tth;
				updateMask |= MASK_TTH;
			}

			// !SMT!-IP
			void setIP(const string& aIP)
			{
				const auto l_new_value = Text::toT(aIP);
				if (l_new_value != m_ip)
				{
					dcassert(!(!m_ip.empty() && aIP.empty()));
					m_ip = l_new_value;
#ifdef FLYLINKDC_USE_DNS
					dns = Text::toT(Socket::nslookup(aIP));
#endif
					updateMask |= MASK_IP;
				}
			}
			tstring m_ip; // TODO - зачем тут tstring?

			void formatStatusString(int transferFlags, uint64_t startTime);
#ifdef FLYLINKDC_USE_DEBUG_TRANSFERS
			string dumpInfo(const UserPtr& user) const;
#endif
		};

		void onSpeakerAddItem(const UpdateInfo& ui);
		void parseQueueItemUpdateInfo(UpdateInfo* ui, const QueueItemPtr& qi);
		UpdateInfo* createUpdateInfoForAddedEvent(const HintedUser& hintedUser, bool download, const string& token);
		
		ItemInfoList ctrlTransfers;
		CustomDrawHelpers::CustomDrawState customDrawState;

	public:
		void UpdateLayout();

	private:
		static const int columnId[];
		
		CImageList imgArrows;
		CImageList imgSpeed;
		CImageList imgSpeedBW;
		
		static HIconWrapper g_user_icon;
		//static HIconWrapper g_fiwrewall_icon;
		
		//OMenu transferMenu;
		OMenu segmentedMenu;
		OMenu usercmdsMenu;
		OMenu copyMenu;
		
		StringMap ucLineParams;
		bool shouldSort;

		TaskQueue tasks;
		TimerHelper timer;

		void onTimerInternal();
		void processTasks();
		void addTask(Tasks s, Task* task);

		void on(ConnectionManagerListener::Added, const HintedUser& hintedUser, bool isDownload, const string& token) noexcept override;
		void on(ConnectionManagerListener::FailedDownload, const HintedUser& hintedUser, const string& reason, const string& token) noexcept override;
		void on(ConnectionManagerListener::FailedUpload, const HintedUser& hintedUser, const string& reason, const string& token) noexcept override;
		void on(ConnectionManagerListener::Removed, const HintedUser& hinted_user, bool isDownload, const string& token) noexcept override;
		void on(ConnectionManagerListener::RemoveToken, const string& token) noexcept override;
		void on(ConnectionManagerListener::UserUpdated, const HintedUser& hintedUser, bool isDownload, const string& token) noexcept override;
		void on(ConnectionManagerListener::ConnectionStatusChanged, const HintedUser& hintedUser, bool isDownload, const string& token) noexcept override;
		void on(ConnectionManagerListener::ListenerStarted) noexcept override;
		void on(ConnectionManagerListener::ListenerFailed, const char* type, int af, int errorCode) noexcept override;

		void on(DownloadManagerListener::RemoveToken, const string& token) noexcept override;
		void on(DownloadManagerListener::Requesting, const DownloadPtr& download) noexcept override;
		void on(DownloadManagerListener::Complete, const DownloadPtr& download) noexcept override
		{
			onTransferComplete(download.get(), true, Util::getFileName(download->getPath()), false);
		}
		void on(DownloadManagerListener::Failed, const DownloadPtr& download, const string& reason) noexcept override;
#ifdef FLYLINKDC_USE_DOWNLOAD_STARTING_FIRE
		void on(DownloadManagerListener::Starting, const DownloadPtr& download) noexcept override;
#endif
		void on(DownloadManagerListener::Tick, const DownloadArray& download) noexcept override;
		void on(DownloadManagerListener::Status, const UserConnection*, const Download::ErrorInfo&) noexcept override;

		void on(UploadManagerListener::Starting, const UploadPtr& upload) noexcept override;
		void on(UploadManagerListener::Tick, const UploadArray& upload) noexcept override;
		void on(UploadManagerListener::Complete, const UploadPtr& upload) noexcept override
		{
			onTransferComplete(upload.get(), false, upload->getPath(), false);
		}
		void on(UploadManagerListener::Failed, const UploadPtr& upload, const string& message) noexcept override
		{
			onTransferComplete(upload.get(), false, upload->getPath(), true);
		}
		void on(QueueManagerListener::StatusUpdated, const QueueItemPtr&) noexcept override;
		void on(QueueManagerListener::StatusUpdatedList, const QueueItemList& list) noexcept override;
		void on(QueueManagerListener::Tick, const QueueItemList& list) noexcept override;
		void on(QueueManagerListener::Removed, const QueueItemPtr&) noexcept override;
		
		void on(QueueManagerListener::Finished, const QueueItemPtr&, const string&, const DownloadPtr& download) noexcept override;
		
		void on(SettingsManagerListener::Repaint) override;
		
		void onTransferComplete(const Transfer* t, const bool download, const string& filename, bool failed);
		static void starting(UpdateInfo* ui, const Transfer* t);
		
		void collapseAll();
		void expandAll();
		
		ItemInfo* findItem(const UpdateInfo& ui, int& pos) const;
		void updateItem(int ii, uint32_t updateMask);

		void pauseSelectedTransfer(void);
		void openDownloadQueue(const ItemInfo* ii);
};

#endif // !defined(TRANSFER_VIEW_H)
