#include "sqlitelogvfs.h"

#include <stdlib.h>
#include <syslog.h>

#include "sqlite/sqlite3.h"

#define MAX_PATHNAME 512

struct LogFile
{
	const struct sqlite3_io_methods *pMethods;
	sqlite3_file *pOldFile;
};

static int XClose(sqlite3_file*);
static int XRead(sqlite3_file*, void*, int iAmt, sqlite3_int64 iOfst);
static int XWrite(sqlite3_file*, const void*, int iAmt, sqlite3_int64 iOfst);
static int XTruncate(sqlite3_file*, sqlite3_int64 size);
static int XSync(sqlite3_file*, int flags);
static int XFileSize(sqlite3_file*, sqlite3_int64 *pSize);
static int XLock(sqlite3_file*, int);
static int XUnlock(sqlite3_file*, int);
static int XCheckReservedLock(sqlite3_file*, int *pResOut);
static int XFileControl(sqlite3_file*, int op, void *pArg);
static int XSectorSize(sqlite3_file*);
static int XDeviceCharacteristics(sqlite3_file*);
/* Methods above are valid for version 1 */
static int XShmMap(sqlite3_file*, int iPg, int pgsz, int, void volatile**);
static int XShmLock(sqlite3_file*, int offset, int n, int flags);
static void XShmBarrier(sqlite3_file*);
static int XShmUnmap(sqlite3_file*, int deleteFlag);
/* Methods above are valid for version 2 */

sqlite3_io_methods g_logIoMethods =
{
	1,			               // int iVersion;
	XClose,                    // int (*xClose)(sqlite3_file*);
	XRead,                     // int (*xRead)(sqlite3_file*, void*, int iAmt, sqlite3_int64 iOfst);
	XWrite,                    // int (*xWrite)(sqlite3_file*, const void*, int iAmt, sqlite3_int64 iOfst);
	XTruncate,                 // int (*xTruncate)(sqlite3_file*, sqlite3_int64 size);
	XSync,                     // int (*xSync)(sqlite3_file*, int flags);
	XFileSize,                 // int (*xFileSize)(sqlite3_file*, sqlite3_int64 *pSize);
	XLock,                     // int (*xLock)(sqlite3_file*, int);
	XUnlock,                   // int (*xUnlock)(sqlite3_file*, int);
	XCheckReservedLock,        // int (*xCheckReservedLock)(sqlite3_file*, int *pResOut);
	XFileControl,              // int (*xFileControl)(sqlite3_file*, int op, void *pArg);
	XSectorSize,               // int (*xSectorSize)(sqlite3_file*);
	XDeviceCharacteristics,    // int (*xDeviceCharacteristics)(sqlite3_file*);
	                           // /* Methods above are valid for version 1 */
	XShmMap,                   // int (*xShmMap)(sqlite3_file*, int iPg, int pgsz, int, void volatile**);
	XShmLock,                  // int (*xShmLock)(sqlite3_file*, int offset, int n, int flags);
	XShmBarrier,               // void (*xShmBarrier)(sqlite3_file*);
	XShmUnmap,                 // int (*xShmUnmap)(sqlite3_file*, int deleteFlag);
	                           // /* Methods above are valid for version 2 */
	                           // /* Additional methods may be added in future releases */
};

static int XOpen(sqlite3_vfs*, const char *zName, sqlite3_file*, int flags, int *pOutFlags);
static int XDelete(sqlite3_vfs*, const char *zName, int syncDir);
static int XAccess(sqlite3_vfs*, const char *zName, int flags, int *pResOut);
static int XFullPathname(sqlite3_vfs*, const char *zName, int nOut, char *zOut);
static void* XDlOpen(sqlite3_vfs*, const char *zFilename);
static void XDlError(sqlite3_vfs*, int nByte, char *zErrMsg);
static void (*XDlSym(sqlite3_vfs*, void*, const char *zSymbol))(void);
static void XDlClose(sqlite3_vfs*, void*);
static int XRandomness(sqlite3_vfs*, int nByte, char *zOut);
static int XSleep(sqlite3_vfs*, int microseconds);
static int XCurrentTime(sqlite3_vfs*, double*);
static int XGetLastError(sqlite3_vfs*, int, char *);
/* The methods above are in version 1 of the sqlite_vfs object */
static int XCurrentTimeInt64(sqlite3_vfs*, sqlite3_int64*);
/* The methods above are in versions 1 and 2 of the sqlite_vfs object. */

sqlite3_vfs g_logVfs =
{
	1,                         // int iVersion;            /* Structure version number (currently 2) */
	sizeof(struct LogFile),    // int szOsFile;            /* Size of subclassed sqlite3_file */
	MAX_PATHNAME,              // int mxPathname;          /* Maximum file pathname length */
	NULL,                      // sqlite3_vfs *pNext;      /* Next registered VFS */
	"net.mospot.logvfs", // const char *zName;       /* Name of this virtual file system */
	NULL,                      // void *pAppData;          /* Pointer to application-specific data */
	XOpen,                     // int (*xOpen)(sqlite3_vfs*, const char *zName, sqlite3_file*,
                               // 			 int flags, int *pOutFlags);
	XDelete,                   // int (*xDelete)(sqlite3_vfs*, const char *zName, int syncDir);
	XAccess,                   // int (*xAccess)(sqlite3_vfs*, const char *zName, int flags, int *pResOut);
	XFullPathname,             // int (*xFullPathname)(sqlite3_vfs*, const char *zName, int nOut, char *zOut);
	XDlOpen,                   // void *(*xDlOpen)(sqlite3_vfs*, const char *zFilename);
	XDlError,                  // void (*xDlError)(sqlite3_vfs*, int nByte, char *zErrMsg);
	XDlSym,                    // void (*(*xDlSym)(sqlite3_vfs*,void*, const char *zSymbol))(void);
	XDlClose,                  // void (*xDlClose)(sqlite3_vfs*, void*);
	XRandomness,               // int (*xRandomness)(sqlite3_vfs*, int nByte, char *zOut);
	XSleep,                    // int (*xSleep)(sqlite3_vfs*, int microseconds);
	XCurrentTime,              // int (*xCurrentTime)(sqlite3_vfs*, double*);
	XGetLastError,             // int (*xGetLastError)(sqlite3_vfs*, int, char *);
	                           // /*
	                           // ** The methods above are in version 1 of the sqlite_vfs object
	                           // ** definition.  Those that follow are added in version 2 or later
	                           // */
	XCurrentTimeInt64,         // int (*xCurrentTimeInt64)(sqlite3_vfs*, sqlite3_int64*);
	                           // /*
	                           // ** The methods above are in versions 1 and 2 of the sqlite_vfs object.
	                           // ** New fields may be appended in figure versions.  The iVersion
	                           // ** value will increment whenever this happens. 
	                           // */
};


const char* LogVfsGetName()
{
	return g_logVfs.zName;
}
int LogVfsRegister(int makeDflt)
{
	syslog(LOG_INFO, "+LogVfsRegister(makeDflt: %d)", makeDflt);
	syslog(LOG_INFO, "=LogVfsRegister: sizeof: sqlite3_int64: %d, long: %d, long long: %d, size_t: %d",
			sizeof(sqlite3_int64), sizeof(long), sizeof(long long), sizeof(size_t));
	
	sqlite3_vfs* defVfsP = sqlite3_vfs_find(NULL);
	g_logVfs.iVersion = defVfsP->iVersion;
	g_logVfs.mxPathname = defVfsP->mxPathname;
	g_logVfs.pAppData = defVfsP;
	syslog(LOG_INFO, "=LogVfsRegister: defVfsP: iVersion: %d, szOsFile: %d, mxPathname: %d, zName: %s",
		   defVfsP->iVersion, defVfsP->szOsFile, defVfsP->mxPathname, defVfsP->zName);
	
	int ret = sqlite3_vfs_register(&g_logVfs, makeDflt);
	syslog(LOG_INFO, "-LogVfsRegister(makeDflt: %d): %d", makeDflt, ret);
	return ret;
}
int LogVfsUnregister()
{
	syslog(LOG_INFO, "+LogVfsUnregister");
	int ret = sqlite3_vfs_unregister(&g_logVfs);
	syslog(LOG_INFO, "-LogVfsUnregister(g_logVfs: 0x%x): %d", ret);
	return ret;
}
/////////////////////////////////
static int XClose(sqlite3_file* sqFileP)
{
	syslog(LOG_INFO, "+XClose(sqFileP: 0x%x)", sqFileP);
	struct LogFile* logFileP = (struct LogFile*)sqFileP;
	int ret = logFileP->pOldFile->pMethods->xClose(logFileP->pOldFile);
	free(logFileP->pOldFile);
	syslog(LOG_INFO, "-XClose(): %d", ret);
	return ret;
}
static int XRead(sqlite3_file* sqFileP, void* pBuf, int iAmt, sqlite3_int64 iOfst)
{
	syslog(LOG_INFO, "+XRead(sqFileP: 0x%x, pBuf: 0x%x, iAmt: %d, iOfst: %lld)", sqFileP, pBuf, iAmt, (long long)iOfst);
	struct LogFile* logFileP = (struct LogFile*)sqFileP;
	int ret = logFileP->pOldFile->pMethods->xRead(logFileP->pOldFile, pBuf, iAmt, iOfst);
	syslog(LOG_INFO, "-XRead(pBuf: 0x%x, iAmt: %d, iOfst: %lld): %d", pBuf, iAmt, (long long)iOfst, ret);
	return ret;
}
static int XWrite(sqlite3_file* sqFileP, const void* pBuf, int iAmt, sqlite3_int64 iOfst)
{
	syslog(LOG_INFO, "+XWrite(sqFileP: 0x%x, pBuf: 0x%x, iAmt: %d, iOfst: %lld)", sqFileP, pBuf, iAmt, (long long)iOfst);
	struct LogFile* logFileP = (struct LogFile*)sqFileP;
	int ret = logFileP->pOldFile->pMethods->xWrite(logFileP->pOldFile, pBuf, iAmt, iOfst);
	syslog(LOG_INFO, "-XWrite(pBuf: 0x%x, iAmt: %d, iOfst: %lld): %d", pBuf, iAmt, (long long)iOfst, ret);
	return ret;
}
static int XTruncate(sqlite3_file* sqFileP, sqlite3_int64 size)
{
	syslog(LOG_INFO, "+XTruncate(sqFileP: 0x%x, size: %lld)", sqFileP, (long long)size);
	struct LogFile* logFileP = (struct LogFile*)sqFileP;
	int ret = logFileP->pOldFile->pMethods->xTruncate(logFileP->pOldFile, size);
	syslog(LOG_INFO, "-XTruncate(size: %lld): %d", (long long)size, ret);
	return ret;	
}
static int XSync(sqlite3_file* sqFileP, int flags)
{
	syslog(LOG_INFO, "+XSync(sqFileP: 0x%x, flags: 0x%x)", sqFileP, flags);
	struct LogFile* logFileP = (struct LogFile*)sqFileP;
	int ret = logFileP->pOldFile->pMethods->xSync(logFileP->pOldFile, flags);
	syslog(LOG_INFO, "-XSync(flags: 0x%x): %d", flags, ret);
	return ret;	
}
static int XFileSize(sqlite3_file* sqFileP, sqlite3_int64 *pSize)
{
	syslog(LOG_INFO, "+XFileSize(sqFileP: 0x%x, pSize: 0x%x)", sqFileP, pSize);
	struct LogFile* logFileP = (struct LogFile*)sqFileP;
	int ret = logFileP->pOldFile->pMethods->xFileSize(logFileP->pOldFile, pSize);
	syslog(LOG_INFO, "-XFileSize(*pSize: %lld): %d", (long long)*pSize, ret);
	return ret;	
}
static int XLock(sqlite3_file* sqFileP, int locktype)
{
	syslog(LOG_INFO, "+XLock(sqFileP: 0x%x, locktype: 0x%x)", sqFileP, locktype);
	struct LogFile* logFileP = (struct LogFile*)sqFileP;
	int ret = logFileP->pOldFile->pMethods->xLock(logFileP->pOldFile, locktype);
	syslog(LOG_INFO, "-XLock(locktype: 0x%x): %d", locktype, ret);
	return ret;	
}
static int XUnlock(sqlite3_file* sqFileP, int locktype)
{
	syslog(LOG_INFO, "+XUnlock(sqFileP: 0x%x, locktype: 0x%x)", sqFileP, locktype);
	struct LogFile* logFileP = (struct LogFile*)sqFileP;
	int ret = logFileP->pOldFile->pMethods->xUnlock(logFileP->pOldFile, locktype);
	syslog(LOG_INFO, "-XUnlock(locktype: 0x%x): %d", locktype, ret);
	return ret;	
}
static int XCheckReservedLock(sqlite3_file* sqFileP, int *pResOut)
{
	syslog(LOG_INFO, "+XCheckReservedLock(sqFileP: 0x%x, pResOut: 0x%x)", sqFileP, pResOut);
	struct LogFile* logFileP = (struct LogFile*)sqFileP;
	int ret = logFileP->pOldFile->pMethods->xCheckReservedLock(logFileP->pOldFile, pResOut);
	syslog(LOG_INFO, "-XCheckReservedLock(*pResOut: 0x%x): %d", *pResOut, ret);
	return ret;	
}
static int XFileControl(sqlite3_file* sqFileP, int op, void *pArg)
{
	syslog(LOG_INFO, "+XFileControl(sqFileP: 0x%x, op: 0x%x, pArg: 0x%x)", sqFileP, op, pArg);
	struct LogFile* logFileP = (struct LogFile*)sqFileP;
	int ret = logFileP->pOldFile->pMethods->xFileControl(logFileP->pOldFile, op, pArg);
	syslog(LOG_INFO, "-XFileControl(op: 0x%x, pArg: 0x%x): %d", op, pArg, ret);
	return ret;	
}
static int XSectorSize(sqlite3_file* sqFileP)
{
	syslog(LOG_INFO, "+XSectorSize(sqFileP: 0x%x)", sqFileP);
	struct LogFile* logFileP = (struct LogFile*)sqFileP;
	int ret = logFileP->pOldFile->pMethods->xSectorSize(logFileP->pOldFile);
	syslog(LOG_INFO, "-XSectorSize(): %d", ret);
	return ret;	
}
static int XDeviceCharacteristics(sqlite3_file* sqFileP)
{
	syslog(LOG_INFO, "+XDeviceCharacteristics(sqFileP: 0x%x)", sqFileP);
	struct LogFile* logFileP = (struct LogFile*)sqFileP;
	int ret = logFileP->pOldFile->pMethods->xDeviceCharacteristics(logFileP->pOldFile);
	syslog(LOG_INFO, "-XDeviceCharacteristics(): %d", ret);
	return ret;	
}
/* Methods above are valid for version 1 */
static int XShmMap(sqlite3_file* sqFileP, int iPg, int pgsz, int bExtend, void volatile** pp)
{
	syslog(LOG_INFO, "+XShmMap(sqFileP: 0x%x, iPg: %d, pgsz: %d, bExtend: %d, pp: 0x%x)", sqFileP, iPg, pgsz, bExtend, pp);
	struct LogFile* logFileP = (struct LogFile*)sqFileP;
	int ret = logFileP->pOldFile->pMethods->xShmMap(logFileP->pOldFile, iPg, pgsz, bExtend, pp);
	syslog(LOG_INFO, "-XShmMap(iPg: %d, pgsz: %d, bExtend: %d, pp: 0x%x): %d", iPg, pgsz, bExtend, pp, ret);
	return ret;		
}
static int XShmLock(sqlite3_file* sqFileP, int offset, int n, int flags)
{
	syslog(LOG_INFO, "+XShmLock(sqFileP: 0x%x, offset: %d, n: %d, flags: 0x%x)", sqFileP, offset, n, flags);
	struct LogFile* logFileP = (struct LogFile*)sqFileP;
	int ret = logFileP->pOldFile->pMethods->xShmLock(logFileP->pOldFile, offset, n, flags);
	syslog(LOG_INFO, "-XShmLock(offset: %d, n: %d, flags: 0x%x): %d", offset, n, flags, ret);
	return ret;	
}
static void XShmBarrier(sqlite3_file* sqFileP)
{
	syslog(LOG_INFO, "+XShmBarrier(sqFileP: 0x%x)", sqFileP);
	struct LogFile* logFileP = (struct LogFile*)sqFileP;
	logFileP->pOldFile->pMethods->xShmBarrier(logFileP->pOldFile);
	syslog(LOG_INFO, "-XShmBarrier()");
}
static int XShmUnmap(sqlite3_file* sqFileP, int deleteFlag)
{
	syslog(LOG_INFO, "+XShmUnmap(sqFileP: 0x%x, deleteFlag: 0x%x)", sqFileP, deleteFlag);
	struct LogFile* logFileP = (struct LogFile*)sqFileP;
	int ret = logFileP->pOldFile->pMethods->xShmUnmap(logFileP->pOldFile, deleteFlag);
	syslog(LOG_INFO, "-XShmUnmap(deleteFlag: 0x%x): %d", deleteFlag, ret);
	return ret;	
}
/* Methods above are valid for version 2 */
/////////////////////////////////
static int XOpen(sqlite3_vfs* vfsP, const char *zName, sqlite3_file* sqFileP, int flags, int *pOutFlags)
{
	syslog(LOG_INFO, "+XOpen(vfsP: 0x%x, zName: %s, flags: 0x%x, pOutFlags: 0x%x)", vfsP, zName, flags, pOutFlags);
	
	sqlite3_vfs* defVfsP = (sqlite3_vfs*)vfsP->pAppData;
	
	struct LogFile* logFileP = (struct LogFile*)sqFileP;
	logFileP->pOldFile = malloc(defVfsP->szOsFile);

	int ret = defVfsP->xOpen(defVfsP, zName, logFileP->pOldFile, flags, pOutFlags);
	syslog(LOG_INFO, "=XOpen: pOldFile->pMethods: 0x%x, pOldFile->pMethods->iVersion: %d",
		   logFileP->pMethods, logFileP->pMethods ? logFileP->pMethods->iVersion : 0);
	if(logFileP->pOldFile->pMethods)
	{
		logFileP->pMethods = &g_logIoMethods;
		((sqlite3_io_methods*)(logFileP->pMethods))->iVersion = logFileP->pOldFile->pMethods->iVersion;
	}
	else
	{
		free(logFileP->pOldFile);
	}
		
	syslog(LOG_INFO, "-XOpen(zName: %s, flags: 0x%x, *pOutFlags: 0x%x): %d", zName, flags, *pOutFlags, ret);
	return ret;
}
static int XDelete(sqlite3_vfs* vfsP, const char *zName, int syncDir)
{
	syslog(LOG_INFO, "+XDelete(vfsP: 0x%x, zName: %s, syncDir: %d)", vfsP, zName, syncDir);
	sqlite3_vfs* defVfsP = (sqlite3_vfs*)vfsP->pAppData;
	int ret = defVfsP->xDelete(defVfsP, zName, syncDir);
	syslog(LOG_INFO, "-XDelete(zName: %s, syncDir: %d): %d", zName, syncDir, ret);
	return ret;
}
static int XAccess(sqlite3_vfs* vfsP, const char *zName, int flags, int *pResOut)
{
	syslog(LOG_INFO, "+XAccess(vfsP: 0x%x, zName: %s, flags: 0x%x, pResOut: 0x%x): %d", vfsP, zName, flags, pResOut);
	sqlite3_vfs* defVfsP = (sqlite3_vfs*)vfsP->pAppData;
	int ret = defVfsP->xAccess(defVfsP, zName, flags, pResOut);
	syslog(LOG_INFO, "-XAccess(zName: %s, flags: 0x%x, *pResOut: 0x%x): %d", zName, flags, *pResOut, ret);
	return ret;
}
static int XFullPathname(sqlite3_vfs* vfsP, const char *zName, int nOut, char *zOut)
{
	syslog(LOG_INFO, "+XFullPathname(vfsP: 0x%x, zName: %s, nOut: %d, zOut: 0x%x)", vfsP, zName, nOut, zOut);
	sqlite3_vfs* defVfsP = (sqlite3_vfs*)vfsP->pAppData;
	int ret = defVfsP->xFullPathname(defVfsP, zName, nOut, zOut);
	syslog(LOG_INFO, "-XFullPathname(zName: %s, nOut: %d, zOut: %s): %d", zName, nOut, zOut, ret);
	return ret;
}
static void* XDlOpen(sqlite3_vfs* vfsP, const char *zFilename)
{
	syslog(LOG_INFO, "+XDlOpen(vfsP: 0x%x, zFilename: %s)", zFilename, vfsP);
	sqlite3_vfs* defVfsP = (sqlite3_vfs*)vfsP->pAppData;
	void* ret = defVfsP->xDlOpen(defVfsP, zFilename);
	syslog(LOG_INFO, "-XDlOpen(zFilename: %s): 0x%x", zFilename, ret);
	return ret;
}
static void XDlError(sqlite3_vfs* vfsP, int nByte, char *zErrMsg)
{
	syslog(LOG_INFO, "+XDlError(vfsP: 0x%x, nByte: %d, zErrMsg: 0x%x)", vfsP, nByte, zErrMsg);
	sqlite3_vfs* defVfsP = (sqlite3_vfs*)vfsP->pAppData;
	defVfsP->xDlError(defVfsP, nByte, zErrMsg);
	syslog(LOG_INFO, "-XDlError(nByte: %d, *zErrMsg: %s)", nByte, *zErrMsg);
}
static void (*XDlSym(sqlite3_vfs* vfsP, void* handle, const char *zSymbol))(void)
{
	syslog(LOG_INFO, "+XDlSym(vfsP: 0x%x, handle: 0x%x, zSymbol: %s)", vfsP, handle, zSymbol);
	sqlite3_vfs* defVfsP = (sqlite3_vfs*)vfsP->pAppData;
	defVfsP->xDlSym(defVfsP, handle, zSymbol);
	syslog(LOG_INFO, "-XDlSym(handle: 0x%x, zSymbol: %s)", handle, zSymbol);
}
static void XDlClose(sqlite3_vfs* vfsP, void* handle)
{
	syslog(LOG_INFO, "+XDlClose(vfsP: 0x%x, handle: 0x%x)", vfsP, handle);
	sqlite3_vfs* defVfsP = (sqlite3_vfs*)vfsP->pAppData;
	defVfsP->xDlClose(defVfsP, handle);
	syslog(LOG_INFO, "-XDlClose(handle: 0x%x)", handle);
}	
static int XRandomness(sqlite3_vfs* vfsP, int nByte, char *zOut)
{
	syslog(LOG_INFO, "+XRandomness(vfsP: 0x%x, nByte: %d, zOut: 0x%x)", vfsP, nByte, zOut);
	sqlite3_vfs* defVfsP = (sqlite3_vfs*)vfsP->pAppData;
	int ret = defVfsP->xRandomness(defVfsP, nByte, zOut);
	syslog(LOG_INFO, "-XRandomness(nByte: %d, zOut: %s): %d", nByte, zOut, ret);
	return ret;
}	
static int XSleep(sqlite3_vfs* vfsP, int microseconds)
{
	syslog(LOG_INFO, "+XSleep(vfsP: 0x%x, microseconds: %d)", vfsP, microseconds);
	sqlite3_vfs* defVfsP = (sqlite3_vfs*)vfsP->pAppData;
	int ret = defVfsP->xSleep(defVfsP, microseconds);
	syslog(LOG_INFO, "-XSleep(microseconds: %d): %d", microseconds, ret);
	return ret;
}	
static int XCurrentTime(sqlite3_vfs* vfsP, double* timeP)
{
	syslog(LOG_INFO, "+XCurrentTime(vfsP: 0x%x, *timeP: %g)", vfsP, timeP);
	sqlite3_vfs* defVfsP = (sqlite3_vfs*)vfsP->pAppData;
	int ret = defVfsP->xCurrentTime(defVfsP, timeP);
	syslog(LOG_INFO, "-XCurrentTime(*timeP: %g): %d", timeP, ret);
	return ret;
}	
static int XGetLastError(sqlite3_vfs* vfsP, int nByte, char *zErrMsg)
{
	syslog(LOG_INFO, "XGetLastError(vfsP: 0x%x, nByte: %d, zErrMsg: 0x%x)", zErrMsg, nByte);
	sqlite3_vfs* defVfsP = (sqlite3_vfs*)vfsP->pAppData;
	int ret = defVfsP->xGetLastError(defVfsP, nByte, zErrMsg);
	syslog(LOG_INFO, "XGetLastError(nByte: %d, zErrMsg: %s): %d", nByte, zErrMsg, ret);
	return ret;
}
/* The methods above are in version 1 of the sqlite_vfs object */
static int XCurrentTimeInt64(sqlite3_vfs* vfsP, sqlite3_int64* timeP)
{
	syslog(LOG_INFO, "+XCurrentTimeInt64(vfsP: 0x%x, timeP: 0x%x)", vfsP, timeP);
	sqlite3_vfs* defVfsP = (sqlite3_vfs*)vfsP->pAppData;
	int ret = defVfsP->xCurrentTimeInt64(defVfsP, timeP);
	syslog(LOG_INFO, "-XCurrentTimeInt64(*timeP: %lld): %d", (long long)*timeP, ret);
	return ret;
}
/* Methods above are valid for version 2 */
