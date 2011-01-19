#include "dictionary.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <dirent.h>
#include "sqlite/sqlite3.h"

struct Dict
{
	sqlite3* dbP;
};

void DictClose(struct Dict* dictP)
{
	if(dictP)
	{
		sqlite3_close(dictP->dbP);
		free(dictP);
	}
}
struct Dict* DictOpen(const char* filename)
{
	if(filename == NULL || filename[0] == '\0') return NULL;
	
	struct Dict* dictP = (struct Dict*)malloc(sizeof(*dictP));
	memset(dictP, 0, sizeof(*dictP));
	
	const char* ext = NULL;
	const char* vfs = NULL;
	if((ext = strrchr(filename, '.')) && (strcmp(ext, ".jdict") == 0))
		vfs = "net.mospot.zipvfs";
    if(sqlite3_open_v2(filename, &dictP->dbP, SQLITE_OPEN_READONLY, vfs) != SQLITE_OK)
    {
        DictClose(dictP);
        return NULL;
    }
	
	return dictP;
}
/////////////////////////////
static int DictGetMetaCb(void* jsonP, int num, char **text, char **name)
{
	if(num != 2) return 0;
    struct json_object* jMeta = (struct json_object*)jsonP;
    if(jMeta) json_object_object_add(jMeta, text[0], json_object_new_string(text[1]));
	return 0;
}
struct json_object* DictGetMeta(struct Dict* dictP)
{
	if(dictP == NULL) return NULL;
	
    const char* sql = "select * from meta";

    struct json_object* jMeta = json_object_new_object();
    sqlite3_exec(dictP->dbP, sql, DictGetMetaCb, jMeta, NULL);
	
	return jMeta;
}
//////////////////////////////////
/* row is { integer (, text)* } */
static struct json_object* RowITxToJson(int num, char** text, char** name)
{
    struct json_object* jRow = json_object_new_object();
    json_object_object_add(jRow, name[0], json_object_new_int(atol(text[0])));
    
    for(int i = 1; i < num; ++i)
    {
        json_object_object_add(jRow, name[i], json_object_new_string(text[i]));
    }
    
    return jRow;
}
/////////////////////////
static int DictLookUpCb(void* jRowP, int num, char **text, char **name)
{
    if(jRowP) (*(struct json_object**)jRowP) = RowITxToJson(num, text, name);
	return 0;
}
static char* EscapeSqlKey(char* buf, int bufSize, const char* word)
{
	if(buf == NULL || bufSize <= 0 || word == NULL) return buf;
	
	char* pd = buf;
	char* pend = buf + bufSize - 1;
	const char* ps = word;
	while(*ps && pd < pend)
	{
		if(*ps == '\'') *pd++ = '\'';
		*pd++ = *ps++;
	}
	if(pd > pend) *pend = '\0';
	else *pd = '\0';
	return buf;
}
struct json_object* DictLookUp(struct Dict* dictP, const char* word, int caseSense)
{
	if(dictP == NULL) return NULL;
	
	const char* caseCollate = caseSense ? "" : "collate nocase";
	char wordEsc[64];
	EscapeSqlKey(wordEsc, sizeof(wordEsc)/sizeof(wordEsc[0]), word);
	char sql[128];
    sprintf(sql, "Select rowid,* from dict where word>='%s' %s limit 1", wordEsc, caseCollate);
	
    struct json_object* jRow = NULL;
    sqlite3_exec(dictP->dbP, sql, DictLookUpCb, (void*)&jRow, NULL);
    
	return jRow;
}
struct json_object* DictLookUpById(struct Dict* dictP, int rowid)
{
	if(dictP == NULL) return NULL;
    char sql[64];
    sprintf(sql, "Select rowid,* from dict where rowid=%d", rowid);
    struct json_object* jRow = NULL;
    sqlite3_exec(dictP->dbP, sql, DictLookUpCb, (void*)&jRow, NULL);
    
	return jRow;
}

/////////////////////////////////
static int DictPeekCb(void* jInfoP, int num, char **text, char **name)
{
    if(jInfoP) (*(struct json_object**)jInfoP) = RowITxToJson(num, text, name);
	return 0;
}
/* { "name": "my dict", count: 2345, "filename": "xxxx.jdic" } */
struct json_object* DictPeek(const char* filename)
{
	if(filename == NULL || filename[0] == '\0') return NULL;
	
    const char* sql = "select * from (select max(rowid) as count from dict)"
        " left join " "(select value as name from meta where key='name')";


	struct Dict* dictP = DictOpen(filename);
	if(dictP == NULL) return NULL;
    
    struct json_object* jInfo = NULL;
    sqlite3_exec(dictP->dbP, sql, DictPeekCb, (void*)&jInfo, NULL);
    if(jInfo)
        json_object_object_add(jInfo, "filename", json_object_new_string(filename));
    
    DictClose(dictP);
    return jInfo;
}
//////////////////////////////////////////
static int dirFilter(const struct dirent* entP)
{
    const char* ext = NULL;
    return (entP->d_type == DT_REG
            && (ext = strrchr(entP->d_name, '.'))
            && (strcmp(ext, ".db") == 0 || strcmp(ext, ".jdict") == 0));
}
/* [ jInfo* ] */
struct json_object* QueryDicts(/*const char* path*/)
{
    struct dirent** entPP = NULL;
    int n = scandir(".", &entPP, dirFilter, alphasort);

    struct json_object* jDicts = json_object_new_array();
    for(int i = 0; i < n; ++i)
    {
        struct json_object* jInfo = DictPeek(entPP[i]->d_name);
        if(jInfo) json_object_array_add(jDicts, jInfo);
        free(entPP[i]);
    }
    free(entPP);
	
	return jDicts;
}