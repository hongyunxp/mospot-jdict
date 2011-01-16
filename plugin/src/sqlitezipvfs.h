/* use standard c file api, no write, no lock */
const char* ZipVfsGetName();
int ZipVfsRegister(int makeDflt);
int  ZipVfsUnregister();
