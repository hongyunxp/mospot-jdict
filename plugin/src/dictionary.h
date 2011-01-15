
#include "json-c/json.h"

struct Dict;

struct json_object* DictPeek(const char* filename);
struct json_object* QueryDicts(/*const char* path*/);

struct Dict* DictOpen(const char* filename);
void DictClose(struct Dict* dictP);

struct json_object* DictGetMeta(struct Dict* dictP);

struct json_object* DictLookUp(struct Dict* dictP, const char* word, int caseSense);
struct json_object* DictLookUpById(struct Dict* dictP, int rowid);
