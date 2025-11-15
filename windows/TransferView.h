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
#include "../client/FavoriteManagerListener.h"
#include "../client/UserManagerListener.h"
#include "../client/TaskQueue.h"
#include "../client/UserInfoBase.h"

#include "TypedTreeListViewCtrl.h"
#include "TimerHelper.h"
#include "CustomDrawHelpers.h"
#include "BarShader.h"
#include "UserInfoBaseHandler.h"
#include "UCHandler.h"
#include "BaseHandlers.h"

static const int TRANSFERS_VIEW_TRAITS = UserInfoGuiTraits::NO_COPY;

class TransferView : public CWindowImpl<TransferView>, private DownloadManagerListener,
	private UploadManagerListener,
	private ConnectionManagerListener,
	public UserInfoBaseHandler<TransferView, TRANSFERS_VIEW_TRAITS>,
	public PreviewBaseHandler,
	public InternetSearchBaseHandler,
	public UCHandler<TransferView>,
	private FavoriteManagerListener,
	private UserManagerListener,
	private SettingsManagerListener
{
	public:
		DECLARE_WND_CLASS(_T("TransferView"))

		TransferView();
		~TransferView();

		typedef UserInfoBaseHandler<TransferView, TRANSFERS_VIEW_TRAITS> uibBase;
		typedef UCHandler<TransferView> ucBase;

		class ItemInfo;
		typedef TypedTreeListViewCtrl<ItemInfo, tstring, true> ItemInfoList;

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
		COMMAND_ID_HANDLER(IDC_FORCE, onForce)
		COMMAND_ID_HANDLER(IDC_SEARCH_ALTERNATES, onSearchAlternates)
		COMMAND_ID_HANDLER(IDC_ADD_P2P_GUARD, onAddP2PGuard)

		COMMAND_ID_HANDLER(IDC_REMOVE, onRemove)
		COMMAND_ID_HANDLER(IDC_REMOVEALL, onRemoveAll)
		COMMAND_ID_HANDLER(IDC_DISCONNECT_ALL, onDisconnectAll)
		COMMAND_ID_HANDLER(IDC_COLLAPSE_ALL, onCollapseAll)
		COMMAND_ID_HANDLER(IDC_EXPAND_ALL, onExpandAll)
		COMMAND_ID_HANDLER(IDC_MENU_SLOWDISCONNECT, onSlowDisconnect)
		COMMAND_ID_HANDLER(IDC_TRANSFERS_ONLY_ACTIVE_UPLOADS, onOnlyActiveUploads)

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
		CHAIN_COMMANDS(InternetSearchBaseHandler)
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
		LRESULT onOnlyActiveUploads(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onPreviewCommand(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onPerformWebSearch(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onOpenWindows(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

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

		LRESULT onDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);

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
		void getSelectedUsers(vector<UserPtr>& v) const;

	private:
		enum Tasks
		{
			ADD_TOKEN,
			REMOVE_TOKEN,
			UPDATE_TOKEN,
			POPUP_NOTIF,
			REPAINT
		};

		enum
		{
			COLUMN_FIRST,
			COLUMN_USER = COLUMN_FIRST,
			COLUMN_HUB,
			COLUMN_STATUS,
			COLUMN_TIME_LEFT,
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
			COLUMN_DEBUG_INFO,
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
					STATUS_WAITING,
					STATUS_REQUESTING,
					STATUS_RUNNING
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
				QueueItemPtr qi;

				bool transferFailed;

				uint8_t stateFlags;
				int16_t hits;
				ItemInfoList::GroupInfoPtr groupInfo;

				int16_t running;

				HintedUser hintedUser;
				Status status;
				Transfer::Type type;

				int64_t pos;
				int64_t size;
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

				const UserPtr& getUser() const override { return hintedUser.user; }
				const string& getHubHint() const override { return hintedUser.hint; }

				void update(const UpdateInfo& ui);
				void updateNicks();

				void disconnect();
				void disconnectAndBlock();
				void removeAll();
				void force();

				double getProgressPosition() const
				{
					return (size > 0) ? (double) pos / (double) size : 1.0;
				}
				int64_t getPos() const;
				const tstring getText(uint8_t col) const;
				static int compareItems(const ItemInfo* a, const ItemInfo* b, int col, int flags);
				static int getCompareFlags() { return 0; }
				static int compareTargets(const ItemInfo* a, const ItemInfo* b);
				static int compareUsers(const ItemInfo* a, const ItemInfo* b);

				uint8_t getImageIndex() const
				{
					if (download) return hasParent() ? IMAGE_SEGMENT : IMAGE_DOWNLOAD;
					return IMAGE_UPLOAD;
				}
				static int getStateImageIndex() { return 0; }
				uint8_t getStateFlags() const { return stateFlags; }
				void setStateFlags(uint8_t flags) { stateFlags = flags; }
				ItemInfo* createParent();
				const tstring& getGroupCond() const { return target; }
				bool hasParent() const { return groupInfo && groupInfo->parent && groupInfo->parent != this; }
				bool isParent() const { return groupInfo && groupInfo->parent == this; }
				static tstring formatStatusString(int transferFlags, uint64_t startTime, int64_t pos, int64_t size);

			private:
				void init();
		};

	private:
		struct UpdateInfo : public Task
		{
			enum
			{
				MASK_QUEUE_ITEM    = 0x0001,
				MASK_POS           = 0x0002,
				MASK_SIZE          = 0x0004,
				MASK_SPEED         = 0x0008,
				MASK_FILE          = 0x0010,
				MASK_STATUS        = 0x0020,
				MASK_TIMELEFT      = 0x0040,
				MASK_IP            = 0x0080,
				MASK_STATUS_STRING = 0x0100,
				MASK_SEGMENTS      = 0x0200,
				MASK_CIPHER        = 0x0400,
				MASK_USER          = 0x0800,
				MASK_ERROR_TEXT    = 0x2000,
				MASK_TOKEN         = 0x4000,
				MASK_TTH           = 0x8000
			};

			UpdateInfo(const HintedUser& user, bool isDownload, bool isTransferFailed = false) :
				updateMask(0), download(isDownload), hintedUser(user), // fix empty string
				transferFailed(isTransferFailed),
				type(Transfer::TYPE_LAST), running(0),
				status(ItemInfo::STATUS_WAITING), pos(0), size(0), speed(0), timeLeft(0)
			{
			}
			UpdateInfo() :
				updateMask(0), download(true), transferFailed(false),
				type(Transfer::TYPE_LAST), running(0),
				status(ItemInfo::STATUS_WAITING), pos(0), size(0), speed(0), timeLeft(0)
			{
			}

			const bool download;
			const bool transferFailed;
			uint32_t updateMask;

			QueueItemPtr qi;
			void setQueueItem(const QueueItemPtr& qi)
			{
				this->qi = qi;
				updateMask |= MASK_QUEUE_ITEM;
			}

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
				updateMask |= MASK_SEGMENTS;
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
			void setErrorText(const tstring& str)
			{
				errorStatusString = str;
				updateMask |= MASK_ERROR_TEXT;
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

			IpAddress ip;
			void setIP(const IpAddress& ip)
			{
				this->ip = ip;
				updateMask |= MASK_IP;
			}
		};

		struct PopupTask : public Task
		{
			int setting;
			ResourceManager::Strings title;
			int flags;
			string user;
			string file;
			tstring miscText;
		};

		ItemInfo *addToken(const UpdateInfo& ui);
		ItemInfo* findItemByToken(const string& token, int& index);
		ItemInfo* findItemByTarget(const tstring& target);
		void updateDownload(ItemInfo* ii, int index, int updateMask);
		UpdateInfo* createUpdateInfoForNewItem(const HintedUser& hintedUser, bool isDownload, const string& token);

		ItemInfoList ctrlTransfers;
		CustomDrawHelpers::CustomDrawState customDrawState;
		bool onlyActiveUploads;

	public:
		void UpdateLayout();

	private:
		static const int columnId[];

		ProgressBar progressBar[3]; // download, download segment, upload
		bool showProgressBars;
		bool showSpeedIcon;
		int64_t topUploadSpeed, topDownloadSpeed;
		class BackingStore* backingStore;

		OMenu segmentedMenu;
		OMenu copyMenu;

		StringMap ucLineParams;
		bool shouldSort;
		string selectedIP;

		TaskQueue tasks;
		TimerHelper timer;

		void onTimerInternal();
		void processTasks();
		void addTask(Tasks s, Task* task);
		void createMenus();
		void destroyMenus();

		// ConnectionManagerListener
		void on(ConnectionManagerListener::Added, const HintedUser& hintedUser, bool isDownload, const string& token) noexcept override;
		void on(ConnectionManagerListener::FailedDownload, const HintedUser& hintedUser, const string& reason, const string& token) noexcept override;
		void on(ConnectionManagerListener::FailedUpload, const HintedUser& hintedUser, const string& reason, const string& token) noexcept override;
		void on(ConnectionManagerListener::Removed, const HintedUser& hinted_user, bool isDownload, const string& token) noexcept override;
		void on(ConnectionManagerListener::RemoveToken, const string& token) noexcept override;
		void on(ConnectionManagerListener::UserUpdated, const HintedUser& hintedUser, bool isDownload, const string& token) noexcept override;
		void on(ConnectionManagerListener::ConnectionStatusChanged, const HintedUser& hintedUser, bool isDownload, const string& token) noexcept override;
		void on(ConnectionManagerListener::ListenerStarted) noexcept override;
		void on(ConnectionManagerListener::ListenerFailed, const char* type, int af, int errorCode, const string& errorText) noexcept override;

		// DownloadManagerListener
		void on(DownloadManagerListener::RemoveToken, const string& token) noexcept override;
		void on(DownloadManagerListener::Requesting, const DownloadPtr& download) noexcept override;
		void on(DownloadManagerListener::Complete, const DownloadPtr& download) noexcept override;
		void on(DownloadManagerListener::Failed, const DownloadPtr& download, const string& reason) noexcept override;
#ifdef FLYLINKDC_USE_DOWNLOAD_STARTING_FIRE
		void on(DownloadManagerListener::Starting, const DownloadPtr& download) noexcept override;
#endif
		void on(DownloadManagerListener::Tick, const DownloadArray& download) noexcept override;
		void on(DownloadManagerListener::Status, const UserConnection*, const Download::ErrorInfo&) noexcept override;

		// UploadManagerListener
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

		// FavoriteManagerListener
		void on(FavoriteManagerListener::UserAdded, const FavoriteUser& user) noexcept override
		{
			addTask(REPAINT, nullptr);
		}
		void on(FavoriteManagerListener::UserRemoved, const FavoriteUser& user) noexcept override
		{
			addTask(REPAINT, nullptr);
		}
		void on(FavoriteManagerListener::UserStatusChanged, const UserPtr& user) noexcept override
		{
			addTask(REPAINT, nullptr);
		}

		// UserManagerListener
		void on(UserManagerListener::ReservedSlotChanged, const UserPtr& user) noexcept override
		{
			addTask(REPAINT, nullptr);
		}

		// SettingsManagerListener
		void on(SettingsManagerListener::ApplySettings) override;

		void onTransferComplete(const Transfer* t, bool download, const string& filename, bool failed);
		static void starting(UpdateInfo* ui, const Transfer* t);

		void collapseAll();
		void expandAll();

		void updateItem(int ii, uint32_t updateMask);

		void pauseSelectedTransfer();
		void openDownloadQueue(const ItemInfo* ii);
		void initProgressBars(bool check);
};

#endif // !defined(TRANSFER_VIEW_H)
