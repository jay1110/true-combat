#ifndef TCE_CENTERPRINT_H
#define TCE_CENTERPRINT_H
typedef void (*tce_centerPopup_t)(int,const char *,int);
void TCE_CenterPrint(const char *,int,int *,int *,int,tce_centerPopup_t);
void TCE_PriorityCenterPrint(const char *,int,int,int *,int *,int,tce_centerPopup_t);
#endif
