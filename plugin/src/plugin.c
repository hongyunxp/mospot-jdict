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
    syslog(LOG_INFO, "+CallJSAndPutJSON(%s, 0x%x)", jsFunc, json);
    const char* jstring = json ? json_object_to_json_string(json) : "undefined";
    syslog(LOG_INFO, "=CallJSAndPutJSON: strlen(jstring): %d, jstring: %s", strlen(jstring), jstring);
    // workaround for callback get called too early
    int err = PDL_CallJS(jsFunc, &jstring, 1);
    while(err != 0)
    {
        syslog(LOG_ERR, "PDL_CallJS(%s): %d: %s", jsFunc, err, PDL_GetError());
        usleep(2000);
        err = PDL_CallJS(jsFunc, &jstring, 1);
    }
    json_object_put(json);
    syslog(LOG_INFO, "-CallJSAndPutJSON");
}
////////////////////////////////////////////////
static void DoLookUp(struct Dict* dictP, const char* word, int caseSense)
{
    syslog(LOG_INFO, "+DoLookUp(dictP: 0x%x, word: %s, caseSens: %d)", dictP, word, caseSense);
    struct json_object* jRow = DictLookUp(dictP, word, caseSense);
    CallJSAndPutJSON("onLookUp", jRow);
    syslog(LOG_INFO, "-DoLookUp(dictP: 0x%x, word: %s, caseSens: %d)", dictP, word, caseSense);
}
/* { "rowid": 39461, "word": "hello", "expl": "used as a greeting" (, "column": "value")* } */
static PDL_bool JS_LookUp(PDL_JSParameters *params)
{
    int dictP = PDL_GetJSParamInt(params, 0);
    const char* word = PDL_GetJSParamString(params, 1);
    int caseSense = PDL_GetJSParamInt(params, 2);
    syslog(LOG_INFO, "+JS_LookUp(0x%x, %s, %d)", dictP, word, caseSense);
    PushUserEvent(caseSense ? EVENT_LOOK_UP_CASE_SENSE : EVENT_LOOK_UP, (void*)dictP, strdup(word));
    syslog(LOG_INFO, "-JS_LookUp");
    return PDL_TRUE;
}
static void DoLookUpById(struct Dict* dictP, int id)
{
    syslog(LOG_INFO, "+DoLookUpById(dictP: 0x%x, id: %d)", dictP, id);
    struct json_object* jRow = DictLookUpById(dictP, id);
    CallJSAndPutJSON("onLookUpById", jRow);
    syslog(LOG_INFO, "-DoLookUpById end");
}
static PDL_bool JS_LookUpById(PDL_JSParameters *params)
{
    int dictP = PDL_GetJSParamInt(params, 0);
    int rowid = PDL_GetJSParamInt(params, 1);
    syslog(LOG_INFO, "+JS_LookUpById(%d)", rowid);
    PushUserEvent(EVENT_LOOK_UP_BY_ID, (void*)dictP, (void*)rowid);
    syslog(LOG_INFO, "-JS_LookUpById");
    return PDL_TRUE;
}
////////////////////////////////////////////
static void DoGetMeta(struct Dict* dictP)
{
    syslog(LOG_INFO, "+DoGetMeta(0x%x)", dictP);
    struct json_object* jMeta = DictGetMeta(dictP);
    CallJSAndPutJSON("onGetMeta", jMeta);
    syslog(LOG_INFO, "-DoGetMeta(0x%x) end");
}
static PDL_bool JS_GetMeta(PDL_JSParameters *params)
{
    int dictP = PDL_GetJSParamInt(params, 0);
    syslog(LOG_INFO, "+JS_GetMeta(0x%x)", dictP);
    PushUserEvent(EVENT_GET_META, (void*)dictP, NULL);
    syslog(LOG_INFO, "-JS_GetMeta");
    return PDL_TRUE;
}
/////////////////////////////////////////
static struct Dict* DoOpenDict(const char* filename)
{
    return DictOpen(filename);
}
static PDL_bool JS_OpenDict(PDL_JSParameters *params)
{
    const char* filename = PDL_GetJSParamString(params, 0);
    syslog(LOG_INFO, "+JS_OpenDict(%s)", filename);
    struct Dict* dictP = DoOpenDict(filename);
    syslog(LOG_INFO, "=JS_OpenDict(): 0x%x", dictP);
    char dictStr[16];
    sprintf(dictStr, "%d", (int)dictP);
    PDL_JSReply(params, dictStr);
    syslog(LOG_INFO, "-JS_OpenDict(): %s", dictStr);
    return PDL_TRUE;
}
/////////////////////////////////////////
static void DoCloseDict(struct Dict* dictP)
{
    DictClose(dictP);
}
static PDL_bool JS_CloseDict(PDL_JSParameters *params)
{
    int dictP = PDL_GetJSParamInt(params, 0);
    syslog(LOG_INFO, "+JS_CloseDict(dictP: %d", dictP);
    DoCloseDict((struct Dict*)dictP);
    syslog(LOG_INFO, "-JS_CloseDict");
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
    
    SDL_Init(SDL_INIT_VIDEO);
    PDL_Init(0);
    
    ZipVfsRegister(0);
    //LogVfsRegister("net.mospot.zipvfs");

    // register the js callback
    PDL_RegisterJSHandler("queryDicts", JS_QueryDicts);
    PDL_RegisterJSHandler("openDict", JS_OpenDict);
    PDL_RegisterJSHandler("closeDict", JS_CloseDict);
    PDL_RegisterJSHandler("getMeta", JS_GetMeta);
    PDL_RegisterJSHandler("lookUp", JS_LookUp);
    PDL_RegisterJSHandler("lookUpById", JS_LookUpById);
    PDL_JSRegistrationComplete();
    
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
    
    //LogVfsUnregister();
    ZipVfsUnregister();
    
    // Cleanup
    PDL_Quit();
    SDL_Quit();

    syslog(LOG_INFO, "plugin exit.");
    return 0;
}
