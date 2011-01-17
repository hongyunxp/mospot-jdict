#include "sqlitecvfs.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <syslog.h>

#include "sqlite/sqlite3.h"

#define MAX_PATHNAME 512

struct CFile
{
	const struct sqlite3_io_methods *pMethods;
	FILE* file;
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

sqlite3_io_methods g_cIoMethods =
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
	NULL, // XShmMap,                   // int (*xShmMap)(sqlite3_file*, int iPg, int pgsz, int, void volatile**);
	NULL, // XShmLock,                  // int (*xShmLock)(sqlite3_file*, int offset, int n, int flags);
	NULL, // XShmBarrier,               // void (*xShmBarrier)(sqlite3_file*);
	NULL, // XShmUnmap,                 // int (*xShmUnmap)(sqlite3_file*, int deleteFlag);
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

sqlite3_vfs g_cVfs =
{
	1,                         // int iVersion;            /* Structure version number (currently 2) */
	sizeof(struct CFile),    // int szOsFile;            /* Size of subclassed sqlite3_file */
	MAX_PATHNAME,              // int mxPathname;          /* Maximum file pathname length */
	NULL,                      // sqlite3_vfs *pNext;      /* Next registered VFS */
	"net.mospot.cvfs", // const char *zName;       /* Name of this virtual file system */
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
	NULL, // XCurrentTimeInt64,         // int (*xCurrentTimeInt64)(sqlite3_vfs*, sqlite3_int64*);
	                           // /*
	                           // ** The methods above are in versions 1 and 2 of the sqlite_vfs object.
	                           // ** New fields may be appended in figure versions.  The iVersion
	                           // ** value will increment whenever this happens. 
	                           // */
};


const char* CVfsGetName()
{
	return g_cVfs.zName;
}
int CVfsRegister(int makeDflt)
{
	sqlite3_vfs* defVfsP = sqlite3_vfs_find(NULL);
	g_cVfs.iVersion = defVfsP->iVersion;
	g_cVfs.mxPathname = defVfsP->mxPathname;
	g_cVfs.pAppData = defVfsP;
	return sqlite3_vfs_register(&g_cVfs, makeDflt);
}
int  CVfsUnregister()
{
	return sqlite3_vfs_unregister(&g_cVfs);
}
/////////////////////////////////
static int XClose(sqlite3_file* sqFileP)
{
	struct CFile* cFileP = (struct CFile*)sqFileP;
	return fclose(cFileP->file) == 0 ? SQLITE_OK : SQLITE_IOERR;
}
static int XRead(sqlite3_file* sqFileP, void* pBuf, int iAmt, sqlite3_int64 iOfst)
{
	struct CFile* cFileP = (struct CFile*)sqFileP;
	FILE* file = cFileP->file;
	if(fseek(file, iOfst, SEEK_SET) != 0)
		return SQLITE_IOERR_READ;
	size_t readCount = fread(pBuf, 1, iAmt, file);
	if(readCount == iAmt)
		return SQLITE_OK;
	else if(readCount < iAmt && feof(file))
	{
		memset(&((char*)pBuf)[readCount], 0, iAmt - readCount);
		return SQLITE_IOERR_SHORT_READ;
	}
	return SQLITE_IOERR_READ;
}
static int XWrite(sqlite3_file* sqFileP, const void* pBuf, int iAmt, sqlite3_int64 iOfst)
{
	struct CFile* cFileP = (struct CFile*)sqFileP;
	// cFileP->lastError = SQLITE_READONLY;
	return SQLITE_IOERR_WRITE;
}
static int XTruncate(sqlite3_file* sqFileP, sqlite3_int64 size)
{
	struct CFile* cFileP = (struct CFile*)sqFileP;
	// cFileP->lastError = SQLITE_READONLY;
	return SQLITE_IOERR_TRUNCATE;
}
static int XSync(sqlite3_file* sqFileP, int flags)
{
	struct CFile* cFileP = (struct CFile*)sqFileP;
	// cFileP->lastError = SQLITE_READONLY;
	return SQLITE_IOERR_FSYNC; // return fflush(cFileP->file) == 0 ? SQLITE_OK : SQLITE_IOERR_FSYNC;
}
static int XFileSize(sqlite3_file* sqFileP, sqlite3_int64 *pSize)
{
	struct CFile* cFileP = (struct CFile*)sqFileP;
	FILE* file = cFileP->file;
	if(fseek(file, 0, SEEK_END) != 0)
		return SQLITE_IOERR_FSTAT;
	long int pos = ftell(file);
	if(pos == -1L)
		return SQLITE_IOERR_FSTAT;
	*pSize = pos;
	return SQLITE_OK;	
}
static int XLock(sqlite3_file* sqFileP, int locktype)
{
	struct CFile* cFileP = (struct CFile*)sqFileP;
	return SQLITE_OK; // no lock
}
static int XUnlock(sqlite3_file* sqFileP, int locktype)
{
	struct CFile* cFileP = (struct CFile*)sqFileP;
	return SQLITE_OK; // no lock
}
static int XCheckReservedLock(sqlite3_file* sqFileP, int *pResOut)
{
	struct CFile* cFileP = (struct CFile*)sqFileP;
	*pResOut = 0;
	return SQLITE_OK; // no lock
}
static int XFileControl(sqlite3_file* sqFileP, int op, void *pArg)
{
	struct CFile* cFileP = (struct CFile*)sqFileP;
	/*switch(op){
	case SQLITE_FCNTL_LOCKSTATE:
		*(int*)pArg = ((os2File*)id)->locktype;
		OSTRACE(( "FCNTL_LOCKSTATE %d lock=%d\n",
		((os2File*)id)->h, ((os2File*)id)->locktype ));
		return SQLITE_OK;
	}*/
	return SQLITE_ERROR;
}
static int XSectorSize(sqlite3_file* sqFileP)
{
	return 512; // SQLITE_DEFAULT_SECTOR_SIZE;
}
static int XDeviceCharacteristics(sqlite3_file* sqFileP)
{
	return 0;
}
/* Methods above are valid for version 1 */
/*static int XShmMap(sqlite3_file* sqFileP, int iPg, int pgsz, int bExtend, void volatile** pp)
{
	syslog(LOG_INFO, "+XShmMap(sqFileP: 0x%x, iPg: %d, pgsz: %d, bExtend: %d, pp: 0x%x)", sqFileP, iPg, pgsz, bExtend, pp);
	struct CFile* cFileP = (struct CFile*)sqFileP;
	int ret = cFileP->pOldFile->pMethods->xShmMap(cFileP->pOldFile, iPg, pgsz, bExtend, pp);
	syslog(LOG_INFO, "-XShmMap(iPg: %d, pgsz: %d, bExtend: %d, pp: 0x%x): %d", iPg, pgsz, bExtend, pp, ret);
	return ret;		
}
static int XShmLock(sqlite3_file* sqFileP, int offset, int n, int flags)
{
	syslog(LOG_INFO, "+XShmLock(sqFileP: 0x%x, offset: %d, n: %d, flags: 0x%x)", sqFileP, offset, n, flags);
	struct CFile* cFileP = (struct CFile*)sqFileP;
	int ret = cFileP->pOldFile->pMethods->xShmLock(cFileP->pOldFile, offset, n, flags);
	syslog(LOG_INFO, "-XShmLock(offset: %d, n: %d, flags: 0x%x): %d", offset, n, flags, ret);
	return ret;	
}
static void XShmBarrier(sqlite3_file* sqFileP)
{
	syslog(LOG_INFO, "+XShmBarrier(sqFileP: 0x%x)", sqFileP);
	struct CFile* cFileP = (struct CFile*)sqFileP;
	cFileP->pOldFile->pMethods->xShmBarrier(cFileP->pOldFile);
	syslog(LOG_INFO, "-XShmBarrier()");
}
static int XShmUnmap(sqlite3_file* sqFileP, int deleteFlag)
{
	syslog(LOG_INFO, "+XShmUnmap(sqFileP: 0x%x, deleteFlag: 0x%x)", sqFileP, deleteFlag);
	struct CFile* cFileP = (struct CFile*)sqFileP;
	int ret = cFileP->pOldFile->pMethods->xShmUnmap(cFileP->pOldFile, deleteFlag);
	syslog(LOG_INFO, "-XShmUnmap(deleteFlag: 0x%x): %d", deleteFlag, ret);
	return ret;	
}*/
/* Methods above are valid for version 2 */
/////////////////////////////////
static int XOpen(sqlite3_vfs* vfsP, const char *zName, sqlite3_file* sqFileP, int flags, int *pOutFlags)
{
	sqlite3_vfs* defVfsP = (sqlite3_vfs*)vfsP->pAppData;
	
	struct CFile* cFileP = (struct CFile*)sqFileP;
	cFileP->pMethods = NULL;
	cFileP->file = fopen(zName, "rb");
	if(cFileP->file == NULL)
		return SQLITE_CANTOPEN;
	cFileP->pMethods = &g_cIoMethods;
	if(pOutFlags) *pOutFlags = SQLITE_OPEN_READONLY;
	return SQLITE_OK;
}
static int XDelete(sqlite3_vfs* vfsP, const char *zName, int syncDir)
{
	sqlite3_vfs* defVfsP = (sqlite3_vfs*)vfsP->pAppData;
	return defVfsP->xDelete(defVfsP, zName, syncDir);
}
static int XAccess(sqlite3_vfs* vfsP, const char *zName, int flags, int *pResOut)
{
	sqlite3_vfs* defVfsP = (sqlite3_vfs*)vfsP->pAppData;
	return defVfsP->xAccess(defVfsP, zName, flags, pResOut);
}
static int XFullPathname(sqlite3_vfs* vfsP, const char *zName, int nOut, char *zOut)
{
	sqlite3_vfs* defVfsP = (sqlite3_vfs*)vfsP->pAppData;
	return defVfsP->xFullPathname(defVfsP, zName, nOut, zOut);
}
static void* XDlOpen(sqlite3_vfs* vfsP, const char *zFilename)
{
	sqlite3_vfs* defVfsP = (sqlite3_vfs*)vfsP->pAppData;
	return defVfsP->xDlOpen(defVfsP, zFilename);
}
static void XDlError(sqlite3_vfs* vfsP, int nByte, char *zErrMsg)
{
	sqlite3_vfs* defVfsP = (sqlite3_vfs*)vfsP->pAppData;
	defVfsP->xDlError(defVfsP, nByte, zErrMsg);
}
static void (*XDlSym(sqlite3_vfs* vfsP, void* handle, const char *zSymbol))(void)
{
	sqlite3_vfs* defVfsP = (sqlite3_vfs*)vfsP->pAppData;
	defVfsP->xDlSym(defVfsP, handle, zSymbol);
}
static void XDlClose(sqlite3_vfs* vfsP, void* handle)
{
	sqlite3_vfs* defVfsP = (sqlite3_vfs*)vfsP->pAppData;
	defVfsP->xDlClose(defVfsP, handle);
}	
static int XRandomness(sqlite3_vfs* vfsP, int nByte, char *zOut)
{
	sqlite3_vfs* defVfsP = (sqlite3_vfs*)vfsP->pAppData;
	return defVfsP->xRandomness(defVfsP, nByte, zOut);
}	
static int XSleep(sqlite3_vfs* vfsP, int microseconds)
{
	sqlite3_vfs* defVfsP = (sqlite3_vfs*)vfsP->pAppData;
	return defVfsP->xSleep(defVfsP, microseconds);
}	
static int XCurrentTime(sqlite3_vfs* vfsP, double* timeP)
{
	sqlite3_vfs* defVfsP = (sqlite3_vfs*)vfsP->pAppData;
	return defVfsP->xCurrentTime(defVfsP, timeP);
}	
static int XGetLastError(sqlite3_vfs* vfsP, int nByte, char *zErrMsg)
{
	sqlite3_vfs* defVfsP = (sqlite3_vfs*)vfsP->pAppData;
	return defVfsP->xGetLastError(defVfsP, nByte, zErrMsg);
}
/* The methods above are in version 1 of the sqlite_vfs object */
/*static int XCurrentTimeInt64(sqlite3_vfs* vfsP, sqlite3_int64* timeP)
{
	syslog(LOG_INFO, "+XCurrentTimeInt64(vfsP: 0x%x, timeP: 0x%x)", vfsP, timeP);
	sqlite3_vfs* defVfsP = (sqlite3_vfs*)vfsP->pAppData;
	int ret = defVfsP->xCurrentTimeInt64(defVfsP, timeP);
	syslog(LOG_INFO, "-XCurrentTimeInt64(*timeP: %d): %d", (unsigned long)*timeP, ret);
	return ret;
}*/
/* Methods above are valid for version 2 */
