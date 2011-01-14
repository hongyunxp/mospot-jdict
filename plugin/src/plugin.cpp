/**
    This is sample code for the Palm webOS PDK.  See the LICENSE file for usage.
    Copyright (C) 2010 Palm, Inc.  All rights reserved.
**/

#include <stdio.h>
#include <syslog.h>
#include "SDL.h"
#include "PDL.h"

#include <unistd.h>
#include <dirent.h>
#include "sqlite/sqlite3.h"
#include "json-c/json.h"

#define EVENT_QUERY_DICTS 721
#define EVENT_OPEN_DICT 722
#define EVENT_CLOSE_DICT 723
#define EVENT_LOOK_UP 724
#define EVENT_LOOK_NEXT 725

static int PushUserEvent(int code, void* data1, void* data2)
{
    /*SDL_Event event = { .user = { SDL_USEREVENT, code, data1, data2 } };*/
    SDL_Event event = { SDL_USEREVENT };
    event.user.code = code;
    event.user.data1 = data1;
    event.user.data2 = data2;
    return SDL_PushEvent(&event);
}
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
static int DbLookUpCb(void* jRowP, int num, char **text, char **name)
{
    struct json_object* jRow = RowITxToJson(num, text, name);
    if(jRowP) (*(struct json_object**)jRowP) = jRow;
}

static const char* const dbPath = "/media/internal/jdict/PWDECAHD_2010.11.18.db";
static sqlite3 *dbP = NULL;

static void DbLookUp(const char* word, const char* callback)
{
    char sql[128];
    sprintf(sql, "Select rowid,* from dict where word>='%s' limit 1", word);
    struct json_object* jRow = NULL;
    sqlite3_exec(dbP, sql, DbLookUpCb, (void*)&jRow, NULL);
    
    if(jRow == NULL) jRow = json_object_new_object();
    const char* jsRow = json_object_to_json_string(jRow);

    PDL_CallJS((const char*)callback, &jsRow, 1);
    json_object_put(jRow);
}

/* { "rowid": 39461, "word": "hello", "expl": "used as a greeting" (, "column": "value")* } */
PDL_bool JS_LookUp(PDL_JSParameters *params)
{
    const char* word = PDL_GetJSParamString(params, 0);
    const char* callback = PDL_GetJSParamString(params, 1);
    
    PushUserEvent(EVENT_LOOK_UP, strdup(word), strdup(callback));
    return PDL_TRUE;
}
static void DbLookNext(int offset, const char* callback)
{
    
}
static int DbLoadCb(void* jRowP, int num, char **text, char **name)
{
    struct json_object* jRow = RowITxToJson(num, text, name);
    if(jRowP) (*(struct json_object**)jRowP) = jRow;
}
struct json_object* DbLoad(const char* filename)
{
    const char* sql = "select * from (select max(rowid) as count from dict)"
        " left join " "(select value as name from meta where key='name')";

    sqlite3 *dbP = NULL;
    if(sqlite3_open_v2(filename, &dbP, SQLITE_OPEN_READONLY, NULL/*VFS*/) != SQLITE_OK)
    {
        sqlite3_close(dbP);
        return NULL;
    }
    
    struct json_object* jRow = NULL;
    sqlite3_exec(dbP, sql, DbLoadCb, (void*)&jRow, NULL);
    if(jRow)
        json_object_object_add(jRow, "filename", json_object_new_string(filename));
    
    sqlite3_close(dbP);
    return jRow;
}
static int dirFilter(const struct dirent* entP)
{
    const char* ext = NULL;
    return (entP->d_type == DT_REG
            && (ext = strrchr(entP->d_name, '.'))
            && strcmp(ext, ".db") == 0);
}
static void QueryDicts(const char* path, const char* callback)
{
    if(path == NULL || path[0] == '\0')
        path = "/media/internal/jdict";
    chdir(path);
    
    struct dirent** entPP = NULL;
    int n = scandir(".", &entPP, dirFilter, alphasort);

    struct json_object* jRows = json_object_new_array();
    for(int i = 0; i < n; ++i)
    {
        struct json_object* jRow = DbLoad(entPP[i]->d_name);
        if(jRow) json_object_array_add(jRows, jRow);
        free(entPP[i]);
    }
    free(entPP);

    const char* jsRow = json_object_to_json_string(jRows);
    PDL_CallJS((const char*)callback, &jsRow, 1);
    json_object_put(jRows);
}
/* [ { "count": 123, "name": "my dict" }* ] */
PDL_bool JS_QueryDicts(PDL_JSParameters *params)
{
    const char* path = PDL_GetJSParamString(params, 0);
    const char* callback = PDL_GetJSParamString(params, 1);

    PushUserEvent(EVENT_QUERY_DICTS, strdup(path), strdup(callback));
    return PDL_TRUE;
}
static void OpenDict(const char* filename, const char* callback)
{
    
}
static void CloseDict(const char* filename, const char* callback)
{
    
}
int main(int argc, char** argv)
{
    //freopen("log.txt", "w", stdout);
    openlog("net.mospot.webos.jdict", 0, LOG_USER);
    syslog(LOG_INFO, "plugin start");
    
    if(argc == 2 && strcmp(argv[1], "queryDict") == 0)
    {
        QueryDicts(NULL, NULL);
    }
    
    SDL_Init(SDL_INIT_VIDEO);
    PDL_Init(0);

    sqlite3_open_v2(dbPath, &dbP, SQLITE_OPEN_READONLY, NULL);

    // register the js callback
    PDL_RegisterJSHandler("lookUp", JS_LookUp);
    PDL_RegisterJSHandler("queryDicts", JS_QueryDicts);
    PDL_JSRegistrationComplete();

    // Event descriptor
    SDL_Event event;
    
    while(SDL_WaitEvent(&event))
    {
        switch(event.type)
        {
        case SDL_USEREVENT:
            switch(event.user.code)
            {
            case EVENT_QUERY_DICTS:
                QueryDicts((const char*)event.user.data1, (const char*)event.user.data2);
                free(event.user.data1);
                break;
            case EVENT_OPEN_DICT:
                OpenDict((const char*)event.user.data1, (const char*)event.user.data2);
                free(event.user.data1);
                break;
            case EVENT_CLOSE_DICT:
                CloseDict((const char*)event.user.data1, (const char*)event.user.data2);
                free(event.user.data1);
                break;
            case EVENT_LOOK_UP:
                DbLookUp((const char*)event.user.data1, (const char*)event.user.data2);
                free(event.user.data1);
                break;
            case EVENT_LOOK_NEXT:
                DbLookNext((int)event.user.data1, (const char*)event.user.data2);
                break;
            default:
                break;
            }
            free(event.user.data2);
            break;
        case SDL_QUIT:
            goto END;
        default:
            break;
        }
    }

END:

    // Cleanup
    PDL_Quit();
    SDL_Quit();

    syslog(LOG_INFO, "plugin exit.");
    return 0;
}
