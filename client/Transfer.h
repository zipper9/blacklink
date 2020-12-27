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

#ifndef DCPLUSPLUS_DCPP_TRANSFER_H_
#define DCPLUSPLUS_DCPP_TRANSFER_H_

#include "forward.h"
#include "Segment.h"
#include "TransferData.h"
#include "SpeedCalc.h"

class UserConnection;

class Transfer
{
	public:
		enum Type
		{
			TYPE_FILE,
			TYPE_FULL_LIST,
			TYPE_PARTIAL_LIST,
			TYPE_TREE,
			TYPE_LAST
		};
		
		static const string fileTypeNames[TYPE_LAST];
		static const string fileNameFilesXml;
		static const string fileNameFilesBzXml;
		
		Transfer(const Transfer&) = delete;
		Transfer& operator= (const Transfer&) = delete;

		int64_t getPos() const { return pos; }
		int64_t getStartPos() const { return segment.getStart(); }
		void resetPos()
		{
			pos = 0;
			actual = 0;
		}
		void addPos(int64_t aBytes, int64_t aActual)
		{
			pos += aBytes;
			actual += aActual;
		}

		int64_t getRunningAverage() const;
		int64_t getActual() const { return actual; }
		int64_t getSize() const { return segment.getSize(); }
		void setSize(int64_t size) { segment.setSize(size); }
		bool getOverlapped() const { return segment.getOverlapped(); }
		void setOverlapped(bool overlap) { segment.setOverlapped(overlap); }
		void setStartPos(int64_t aPos)
		{
			startPos = aPos;
			pos = aPos;
		}
		
	protected:
		Transfer(UserConnection* conn, const string& path, const TTHValue& tth, const string& ip, const string& cipherName);
		void getParams(const UserConnection* aSource, StringMap& params) const;

	public:
		UserPtr& getUser() { return hintedUser.user; }
		const UserPtr& getUser() const { return hintedUser.user; }
		HintedUser getHintedUser() const { return hintedUser; }
		const string& getPath() const { return path; }
		const TTHValue& getTTH() const { return tth; }
		string getConnectionQueueToken() const;
		const UserConnection* getUserConnection() const { return userConnection; }
		UserConnection* getUserConnection() { return userConnection; }
		const string& getCipherName() const { return cipherName; }
		const string& getIP() const { return ip; }
		
		GETSET(Segment, segment, Segment);
		GETSET(int64_t, fileSize, FileSize);
		GETSET(Type, type, Type);

		uint64_t getStartTime() const { return startTime; }
		void setStartTime(uint64_t tick);
		const uint64_t getLastActivity();
		//string getUserConnectionToken() const;
		//string getConnectionToken() const;
		GETSET(uint64_t, lastTick, LastTick);
		const bool isSecure;
		const bool isTrusted;

	protected:
		uint64_t startTime;
		SpeedCalc<16> speed;
		mutable FastCriticalSection csSpeed;
		
		/** The file being transferred */
		const string path;
		/** TTH of the file being transferred */
		const TTHValue tth;
		/** Bytes transferred over socket */
		int64_t actual;
		/** Bytes transferred to/from file */
		int64_t pos;
		/** Starting position */
		int64_t startPos;
		/** Actual speed */
		int64_t runningAverage;
		UserConnection* userConnection;
		HintedUser hintedUser;
		const string cipherName;
		const string ip;
};

#endif /*TRANSFER_H_*/
