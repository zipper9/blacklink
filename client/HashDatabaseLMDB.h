#ifndef HASH_DATABASE_LMDB_H
#define HASH_DATABASE_LMDB_H

#include <lmdb.h>
#include "Locks.h"
#include "MerkleTree.h"
#include "WaitableEvent.h"

class HashDatabaseLMDB;

class HashDatabaseConnection
{
		friend class HashDatabaseLMDB;

	public:
		struct DbInfo
		{
			size_t numPages;
			size_t numKeys;
			int64_t mapSize;
			int64_t totalKeysSize;
			int64_t totalDataSize;
			int64_t totalTreesSize;
			uint8_t keysHash[24];
			uint8_t dataHash[24];
		};

		enum
		{
			GET_DB_INFO_DETAILS  = 1,
			GET_DB_INFO_TREES    = 2,
			GET_DB_INFO_DIGEST   = 4,
			GET_DB_INFO_MAP_SIZE = 8
		};

		~HashDatabaseConnection() noexcept;
		HashDatabaseConnection(const HashDatabaseConnection&) = delete;
		HashDatabaseConnection& operator= (const HashDatabaseConnection&) = delete;

		bool getFileInfo(const void *tth, unsigned &flags, uint64_t *fileSize, string *path, size_t *treeSize, uint32_t *uploadCount) noexcept;
		bool getTigerTree(const void *tth, TigerTree &tree) noexcept;
		bool putFileInfo(const void *tth, unsigned flags, uint64_t fileSize, const string *path, bool incUploadCount) noexcept;
		bool putTigerTree(const TigerTree &tree) noexcept;
		bool getDBInfo(DbInfo &info, int flags) noexcept;

	private:
		bool busy = true;
		uint64_t removeTime = 0;
		MDB_txn *txnRead = nullptr;
		HashDatabaseLMDB *const parent;
		std::vector<uint8_t> buf;

		HashDatabaseConnection(HashDatabaseLMDB *parent) : parent(parent) {}
		bool createReadTxn(MDB_dbi &dbi) noexcept;
		void completeReadTxn() noexcept;
		bool createWriteTxn(MDB_dbi &dbi, MDB_txn* &txnWrite) noexcept;
		void abortWriteTxn(MDB_txn *txnWrite) noexcept;
		bool writeData(MDB_txn *txnWrite, MDB_dbi dbi, MDB_val &key, MDB_val &val) noexcept;
		bool resizeMap() noexcept;
};

class HashDatabaseLMDB
{
		friend class HashDatabaseConnection;

	public:
		HashDatabaseLMDB() {}
		~HashDatabaseLMDB() noexcept { close(); }
		HashDatabaseLMDB(const HashDatabaseLMDB&) = delete;
		HashDatabaseLMDB& operator= (const HashDatabaseLMDB&) = delete;

		bool open() noexcept;
		void close() noexcept;
		bool getDBInfo(size_t &dataItems, uint64_t &dbSize) noexcept;
		static string getDBPath() noexcept;
		HashDatabaseConnection *getDefaultConnection() noexcept;
		HashDatabaseConnection *getConnection() noexcept;
		void putConnection(HashDatabaseConnection *conn) noexcept;
		void closeIdleConnections(uint64_t tick) noexcept;

	private:
		enum
		{
			STATE_NORMAL,
			STATE_SHUTDOWN,
			STATE_RESIZE,
			STATE_WANT_RESIZE
		};

		MDB_env *env = nullptr;
		mutable CriticalSection cs;
		std::unique_ptr<HashDatabaseConnection> defConn;
		std::list<std::unique_ptr<HashDatabaseConnection>> conn;
		uintptr_t defThreadId = 0;

		mutable FastCriticalSection csSharedState;
		int sharedState = STATE_NORMAL;
		int numTxn = 0;
		WaitableEvent resizeComplete;
		WaitableEvent mayResize;

		static bool checkError(int error, const char *what) noexcept;
		bool addTransaction() noexcept;
		void releaseTransaction(HashDatabaseConnection *conn) noexcept;
		bool resizeMap() noexcept;
		bool waitResizeComplete() noexcept;
		bool waitMayResize() noexcept;
		void completeResize() noexcept;
		int getSharedState() const noexcept;
		void shutdown() noexcept; // FIXME: not used
};

#endif /* HASH_DATABASE_LMDB_H */
