#ifndef TCE_POPUP_TEXT_H
#define TCE_POPUP_TEXT_H
typedef struct {int kind,action,client,density,localTeam,snapshotTeam;float origin[3];} tce_popup_text_t;
typedef struct {const char *(*config)(int);const char *(*name)(int);const char *(*location)(const float *);} tce_popup_text_api_t;
const char *TCE_GetPMItemText(const tce_popup_text_t *,const tce_popup_text_api_t *,char *,unsigned int);
#endif
