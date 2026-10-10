#ifndef TCE_GUID_H
#define TCE_GUID_H
int TCE_GUIDFromETKey(const unsigned char *data, int length, char out[33]);
void TCE_InitClientGUID(void);
#endif
