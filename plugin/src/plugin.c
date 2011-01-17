/**
    This is sample code for the Palm webOS PDK.  See the LICENSE file for usage.
    Copyright (C) 2010 Palm, Inc.  All rights reserved.
**/

#include <stdio.h>
#include <stdlib.h>
#include "SDL.h"
#include "PDL.h"

#include <syslog.h>
#include <unistd.h>
#include "json-c/json.h"

#include "sqlitelogvfs.h"
#include "sqlitezipvfs.h"
#include "sqlitecvfs.h"
#include "dictionary.h"

#define EVENT_READY 720

#define EVENT_QUERY_DICTS 721
#define EVENT_GET_META 724
#define EVENT_LOOK_UP 725
#define EVENT_LOOK_UP_CASE_SENSE 726
#define EVENT_LOOK_UP_BY_ID 727

static int PushUserEvent(int code, void* data1, void* data2)
{
    /*SDL_Event event = { .user = { SDL_USEREVENT, code, data1, data2 } };*/
    SDL_Event event = { SDL_USEREVENT };
    event.user.code = code;
    event.user.data1 = data1;
    event.user.data2 = data2;
    return SDL_PushEvent(&event);
}

////////////////////////////////////////////
static void CallJSAndPutJSON(const char* jsFunc, struct json_object* json)
{
    syslog(LOG_INFO, "CallJSAndPutJSON(%s, 0x%x)", jsFunc, json);
    const char* jstring = json ? json_object_to_json_string(json) : "undefined";
    syslog(LOG_INFO, "CallJSAndPutJSON: jstring: %s", jstring);
    PDL_CallJS(jsFunc, &jstring, 1);
    json_object_put(json);
    syslog(LOG_INFO, "CallJSAndPutJSON end");
}
////////////////////////////////////////////////
static void DoLookUp(struct Dict* dictP, const char* word, int caseSense)
{
    struct json_object* jRow = DictLookUp(dictP, word, caseSense);
    CallJSAndPutJSON("onLookUp", jRow);
}
/* { "rowid": 39461, "word": "hello", "expl": "used as a greeting" (, "column": "value")* } */
static PDL_bool JS_LookUp(PDL_JSParameters *params)
{
    int dictP = PDL_GetJSParamInt(params, 0);
    const char* word = PDL_GetJSParamString(params, 1);
    int caseSense = PDL_GetJSParamInt(params, 2);
    syslog(LOG_INFO, "JS_LookUp(0x%x, %s, %d)", dictP, word, caseSense);
    PushUserEvent(caseSense ? EVENT_LOOK_UP_CASE_SENSE : EVENT_LOOK_UP, (void*)dictP, strdup(word));
    syslog(LOG_INFO, "JS_LookUp end");
    return PDL_TRUE;
}
static void DoLookUpById(struct Dict* dictP, int id)
{
    struct json_object* jRow = DictLookUpById(dictP, id);
    CallJSAndPutJSON("onLookUpById", jRow);
    syslog(LOG_INFO, "DoLookUpById end");
}
static PDL_bool JS_LookUpById(PDL_JSParameters *params)
{
    int dictP = PDL_GetJSParamInt(params, 0);
    int rowid = PDL_GetJSParamInt(params, 1);
    syslog(LOG_INFO, "JS_LookUpById(%d)", rowid);
    PushUserEvent(EVENT_LOOK_UP_BY_ID, (void*)dictP, (void*)rowid);
    return PDL_TRUE;
}
////////////////////////////////////////////
static void DoGetMeta(struct Dict* dictP)
{
    syslog(LOG_INFO, "DoGetMeta(0x%x)", dictP);
    struct json_object* jMeta = DictGetMeta(dictP);
    CallJSAndPutJSON("onGetMeta", jMeta);
    syslog(LOG_INFO, "DoGetMeta(0x%x) end");
}
static PDL_bool JS_GetMeta(PDL_JSParameters *params)
{
    int dictP = PDL_GetJSParamInt(params, 0);
    syslog(LOG_INFO, "JS_GetMeta(0x%x)", dictP);
    PushUserEvent(EVENT_GET_META, (void*)dictP, NULL);
    return PDL_TRUE;
}
/////////////////////////////////////////
static struct Dict* DoOpenDict(const char* filename)
{
    syslog(LOG_INFO, "DoOpenDict(%s)", filename);
    return DictOpen(filename);
    syslog(LOG_INFO, "DoOpenDict end");
}
static PDL_bool JS_OpenDict(PDL_JSParameters *params)
{
    const char* filename = PDL_GetJSParamString(params, 0);
    syslog(LOG_INFO, "JS_OpenDict(%s)", filename);
    struct Dict* dictP = DoOpenDict(filename);
    syslog(LOG_INFO, "JS_OpenDict(): 0x%x", dictP);
    char dictStr[16];
    sprintf(dictStr, "%d", (int)dictP);
    syslog(LOG_INFO, "JS_OpenDict(): %s", dictStr);
    PDL_JSReply(params, dictStr);
    return PDL_TRUE;
}
/////////////////////////////////////////
static void DoCloseDict(struct Dict* dictP)
{
    syslog(LOG_INFO, "DoCloseDict(0x%x)", dictP);
    DictClose(dictP);
    syslog(LOG_INFO, "DoCloseDict end");
}
static PDL_bool JS_CloseDict(PDL_JSParameters *params)
{
    int dictP = PDL_GetJSParamInt(params, 0);
    DoCloseDict((struct Dict*)dictP);
    return PDL_TRUE;
}
/////////////////////////////////////////
static void DoQueryDicts(const char* path)
{
    if(path == NULL || path[0] == '\0')
        path = "/media/internal/jdict";
    chdir(path);
    
    struct json_object* jDicts = QueryDicts();
    CallJSAndPutJSON("onQueryDicts", jDicts);
}
static PDL_bool JS_QueryDicts(PDL_JSParameters *params)
{
    const char* path = PDL_GetJSParamString(params, 0);
    PushUserEvent(EVENT_QUERY_DICTS, NULL, strdup(path));
    return PDL_TRUE;
}
/////////////////////////////////////////
int main(int argc, char** argv)
{
    //freopen("log.txt", "w", stdout);
    openlog("net.mospot.webos.jdict", 0, LOG_USER);
    syslog(LOG_INFO, "plugin start");
    
    syslog(LOG_INFO, "SDL_Init vvv");
    SDL_Init(SDL_INIT_VIDEO);
    syslog(LOG_INFO, "SDL_Init ^^^");
    syslog(LOG_INFO, "PDL_Init vvv");
    PDL_Init(0);
    syslog(LOG_INFO, "PDL_Init ^^^");
    
    CVfsRegister(1);
    LogVfsRegister(1);

    // register the js callback
    syslog(LOG_INFO, "PDL_RegisterJSHandler vvv");
    PDL_RegisterJSHandler("queryDicts", JS_QueryDicts);
    syslog(LOG_INFO, "PDL_RegisterJSHandler queryDicts");
    PDL_RegisterJSHandler("openDict", JS_OpenDict);
    syslog(LOG_INFO, "PDL_RegisterJSHandler openDict");
    PDL_RegisterJSHandler("closeDict", JS_CloseDict);
    syslog(LOG_INFO, "PDL_RegisterJSHandler closeDict");
    PDL_RegisterJSHandler("getMeta", JS_GetMeta);
    syslog(LOG_INFO, "PDL_RegisterJSHandler getMeta");
    PDL_RegisterJSHandler("lookUp", JS_LookUp);
    syslog(LOG_INFO, "PDL_RegisterJSHandler lookUp");
    PDL_RegisterJSHandler("lookUpById", JS_LookUpById);
    syslog(LOG_INFO, "PDL_RegisterJSHandler lookUpById");
    syslog(LOG_INFO, "PDL_RegisterJSHandler ^^^");
    syslog(LOG_INFO, "PDL_JSRegistrationComplete vvv");
    PDL_JSRegistrationComplete();
    syslog(LOG_INFO, "PDL_JSRegistrationComplete ^^^");
    
    PushUserEvent(EVENT_READY, NULL, NULL);

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
                DoQueryDicts((const char*)event.user.data2);
                free(event.user.data2);
                break;
            case EVENT_GET_META:
                DoGetMeta((struct Dict*)event.user.data1);
                break;
            case EVENT_LOOK_UP:
                DoLookUp((struct Dict*)event.user.data1, (const char*)event.user.data2, 0);
                free(event.user.data2);
                break;
            case EVENT_LOOK_UP_CASE_SENSE:
                DoLookUp((struct Dict*)event.user.data1, (const char*)event.user.data2, 1);
                free(event.user.data2);
                break;
            case EVENT_LOOK_UP_BY_ID:
                DoLookUpById((struct Dict*)event.user.data1, (int)event.user.data2);
                break;
            case EVENT_READY:
                syslog(LOG_INFO, "EVENT_READY");
                PDL_CallJS("ready", NULL, 0);
                break;
            default:
                break;
            }
            break;
        case SDL_QUIT:
            goto END;
        default:
            break;
        }
    }

END:

    syslog(LOG_INFO, "plugin exit.vvv");
    
    LogVfsUnregister();
    CVfsUnregister();
    
    // Cleanup
    PDL_Quit();
    SDL_Quit();

    syslog(LOG_INFO, "plugin exit.");
    return 0;
}
