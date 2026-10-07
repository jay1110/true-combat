#ifndef TCE_LIMBO_WIDGETS_H
#define TCE_LIMBO_WIDGETS_H
typedef struct { float x,y,w,h; } tce_limboRect_t;
void TCE_LimboDrawBorder(float x,float y,float w,float h,int fill,int hover,
 int (*inside)(const tce_limboRect_t *),
 void (*rect)(float,float,float,float,const float *));
typedef struct { float x,y,w,h; int data[8]; } tce_limboButton_t;
typedef struct {
 int classOff,classOn,weaponOff,weaponOn,selectedSlot;
 int (*team)(void); int (*playerClass)(void); int (*counter)(const tce_limboButton_t *);
 const int *teamOrder;
 void (*pic)(float,float,float,float,int);
 void (*border)(float,float,float,float,int,int);
 void (*text)(float,float,float,float,const float *,const char *);
} tce_limboWidgets_t;
void TCE_LimboClass(const tce_limboButton_t *,const tce_limboWidgets_t *);
void TCE_LimboCounter(const tce_limboButton_t *,const tce_limboWidgets_t *);
void TCE_LimboWeaponLight(const tce_limboButton_t *,const tce_limboWidgets_t *);
void TCE_LimboBorder(const tce_limboButton_t *,const tce_limboWidgets_t *);
#endif
