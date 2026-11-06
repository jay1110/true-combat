/* Actual shared BIND/list painting, with shipped TTF advances and draw records. */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "src/ui/tce_viewport.h"
typedef int qboolean;
typedef int qhandle_t;
typedef float vec4_t[4];
typedef struct {int top,imageHeight,pitch,imageWidth,xSkip;} glyphInfo_t;
typedef struct {glyphInfo_t glyphs[256];float glyphScale;} fontInfo_t;
typedef struct {float x,y,w,h;} rectDef_t;
typedef struct {int maxPaintChars;} editFieldDef_t;
typedef struct {int count;} multiDef_t;
typedef struct {vec4_t focusColor;} menuDef_t;
typedef struct {
 struct {int flags;rectDef_t rect;float borderSize;vec4_t foreColor,borderColor,outlineColor;} window;
 rectDef_t textRect;void *parent,*typeData;const char *cvar,*text;
 float textscale,textalignx,textaligny,special;int textStyle,cursorPos,font,textalignment,type;
} itemDef_t;
typedef struct {
 int startPos,endPos,drawPadding,cursorPos;float elementWidth,elementHeight;
 int elementStyle,numColumns;struct {int pos,width,maxChars;} columnInfo[16];
} listBoxDef_t;
#define WINDOW_HASFOCUS 2
#define WINDOW_FOCUSPULSE 0x08000000
#define WINDOW_HORIZONTAL 0x400
#define WINDOW_AUTOWRAPPED 0x10000
#define WINDOW_WRAPPED 0x20000
#define SLIDER_WIDTH 96
#define LISTBOX_IMAGE 1
#define SCROLLBAR_SIZE 16.f
#define PULSE_DIVISOR 75
#define ITEM_ALIGN_LEFT 0
#define ITEM_ALIGN_CENTER 1
#define ITEM_ALIGN_RIGHT 2
#define ITEM_ALIGN_CENTER2 3
enum {ITEM_TYPE_TEXT,ITEM_TYPE_BUTTON,ITEM_TYPE_EDITFIELD,ITEM_TYPE_NUMERICFIELD,
 ITEM_TYPE_CHECKBOX,ITEM_TYPE_TRICHECKBOX,ITEM_TYPE_YESNO,ITEM_TYPE_MULTI,
 ITEM_TYPE_BIND,ITEM_TYPE_SLIDER,ITEM_TYPE_LISTBOX};
#define qfalse 0
#define qtrue 1
#define Q_IsColorString(p) ((p)[0]=='^'&&(p)[1]&&(p)[1]!='^')
/* FONT_ADVANCES */
/* FONT_GLYPHS */
static int currentFont,modernEnabled,checks,drawCount,labelCalls;
static itemDef_t *g_bindItem;
static const char *binding,*columnText[2],*cvarText;
static char g_nameBind1[32];
static struct {float x,y,scale;int limit,style;char text[2048];vec4_t color;} draws[2048];
static qboolean TCE_ModernUIEnabled(void){return modernEnabled;}
static struct {struct {struct {int vidWidth,vidHeight;} glconfig;} uiDC;} uiInfo={{{1920,1080}}};
static float trap_Cvar_VariableValue(const char *name){return !strcmp(name,"ui_menuMaxHeight")?1080.f:1.f;}
static float TextWidthExact(const char *s,float scale,int limit){
 float width=0;int count=0;
 while(*s && (!limit||count<limit)){
  if(s[0]=='^'&&s[1]&&s[1]!='^'){s+=2;continue;}
  width+=fontAdvances[currentFont][(unsigned char)*s++];++count;
 }
 return width*scale;
}
static int TextWidth(const char *s,float scale,int limit){return (int)TextWidthExact(s,scale,limit);}
static float Cvar(const char *key){(void)key;return 1;}
static void Text(float x,float y,float scale,const float *color,const char *text,float adjust,int limit,int style){
 (void)adjust;draws[drawCount].x=x;draws[drawCount].y=y;draws[drawCount].scale=scale;
 draws[drawCount].limit=limit;draws[drawCount].style=style;strcpy(draws[drawCount].text,text);
 memcpy(draws[drawCount].color,color,sizeof(vec4_t));++drawCount;
}
static int Count(float feeder){(void)feeder;return 1;}
static int Image(float feeder,int row){(void)feeder;(void)row;return 0;}
static const char *Feeder(float feeder,int row,int col,qhandle_t *images,int *count){(void)feeder;(void)row;(void)images;*count=0;return columnText[col];}
static void Pic(float x,float y,float w,float h,qhandle_t image){(void)x;(void)y;(void)w;(void)h;(void)image;}
static void Rect(float x,float y,float w,float h,float border,const float *color){(void)x;(void)y;(void)w;(void)h;(void)border;(void)color;}
static void Fill(float x,float y,float w,float h,const float *color){(void)x;(void)y;(void)w;(void)h;(void)color;}
static struct {
 int realTime;struct {int scrollBarArrowLeft,scrollBarArrowRight,scrollBarArrowUp,scrollBarArrowDown,scrollBar,scrollBarThumb;fontInfo_t fonts[6];} Assets;
 int (*textWidth)(const char *,float,int);float (*getCVarValue)(const char *);
 void (*drawText)(float,float,float,const float *,const char *,float,int,int);
 int (*feederCount)(float);int (*feederItemImage)(float,int);
 const char *(*feederItemText)(float,int,int,qhandle_t *,int *);
 void (*drawHandlePic)(float,float,float,float,qhandle_t);
 void (*drawRect)(float,float,float,float,float,const float *);
 void (*fillRect)(float,float,float,float,const float *);
 void (*getCVarString)(const char *,char *,int);
 const char *(*translateString)(const char *);
} context={0,{0},TextWidth,Cvar,Text,Count,Image,Feeder,Pic,Rect,Fill},*DC=&context;
static void Item_Text_Paint(itemDef_t *item){(void)item;++labelCalls;}
static void BindingFromName(const char *name){(void)name;strcpy(g_nameBind1,binding);}
static const char *Item_Multi_Setting(itemDef_t *item){(void)item;return binding;}
static const char *Translate(const char *s){return s;}
static void LerpColor(const float *a,const float *b,float *out,float f){int i;for(i=0;i<4;++i)out[i]=a[i]+f*(b[i]-a[i]);}
static void TCE_MenuAssetPic(float x,float y,float w,float h,int image,const float *color){(void)color;Pic(x,y,w,h,image);}
static float Item_ListBox_ThumbDrawPosition(itemDef_t *item){return item->window.rect.y;}
static void Item_TextColor(itemDef_t *item,vec4_t *color){memcpy(*color,item->window.foreColor,sizeof(vec4_t));}
static void Item_SetTextExtents(itemDef_t *item,int *width,int *height,const char *text){
 const unsigned char *p=(const unsigned char *)text;int h=0;
 *width=TextWidth(text,item->textscale,0);
 while(*p){if(Q_IsColorString(p)){p+=2;continue;}if(DC->Assets.fonts[item->font].glyphs[*p].imageHeight>h)h=DC->Assets.fonts[item->font].glyphs[*p].imageHeight;++p;}
 *height=(int)(h*item->textscale*DC->Assets.fonts[item->font].glyphScale);
}
static void ToWindowCoords(float *x,float *y,void *window){itemDef_t *item=(itemDef_t *)window;*x+=item->window.rect.x;*y+=item->window.rect.y;}
static void CvarString(const char *name,char *out,int size){(void)name;strncpy(out,cvarText,size-1);out[size-1]=0;}
/* ACTUAL_FIELD_PAINT */
#define CHECK(test) do {++checks;if(!(test)){fprintf(stderr,"failed line %d: %s\n",__LINE__,#test);return 1;}} while(0)
#define CLOSE(a,b) (fabsf((a)-(b))<.0001f)
static void Reset(itemDef_t *item){
 memset(item,0,sizeof(*item));memset(draws,0,sizeof(draws));drawCount=labelCalls=0;
 item->window.rect.x=24;item->window.rect.y=120;item->window.rect.w=240;item->window.rect.h=20;
 item->window.foreColor[0]=.84f;item->window.foreColor[1]=.89f;item->window.foreColor[2]=.94f;item->window.foreColor[3]=1;
 item->textRect.x=24;item->textRect.y=134;item->textRect.w=120;item->textscale=.22f;
 item->cvar="+forward";item->text="Forward:";item->textStyle=3;
 item->font=currentFont;
}
static int Paragraph(const char *text,int modern,int useCvar){
 itemDef_t item;int i;char restored[4096];float bounds[4];
 Reset(&item);item.window.rect.x=228;item.window.rect.y=146;item.window.rect.w=396;item.window.rect.h=132;
 item.textscale=.26f;item.textalignx=0;item.textaligny=15;item.textStyle=0;
 item.text=useCvar?NULL:text;cvarText=text;DC->getCVarString=CvarString;modernEnabled=modern;
 Item_Text_AutoWrapped_Paint(&item);restored[0]=0;
 for(i=0;i<drawCount;++i){
  TCE_ParagraphInk(&item,draws[i].text,draws[i].scale,bounds);
  if(modern){CHECK(draws[i].x+bounds[0]>=228-.002f&&draws[i].x+bounds[2]<=624+.002f);
   CHECK(draws[i].y+bounds[1]>=146-.002f&&draws[i].y+bounds[3]<=278+.002f);}
  CHECK(draws[i].scale>0&&draws[i].scale<=(modern?TCE_ModernReadableScale(.26f,&DC->Assets.fonts[item.font]):.26f));strcat(restored,draws[i].text);
 }
 /* All non-whitespace input bytes, including every color token, survive. */
 {char expected[4096],actual[4096];int n=0,m=0;const char *p=text;
  while(*p){if(*p!=' '&&*p!='\n'&&*p!='\r'&&*p!='\t')expected[n++]=*p;++p;}expected[n]=0;p=restored;
  while(*p){if(*p!=' '&&*p!='\n'&&*p!='\r'&&*p!='\t')actual[m++]=*p;++p;}actual[m]=0;
  CHECK(!strcmp(actual,expected));}
 return 0;
}
int main(void){
 itemDef_t item;editFieldDef_t edit={0};menuDef_t menu={{1,1,1,1}};listBoxDef_t list;
 char wide39[40],wide13[14],colored[48],longword[1024],coloredLong[1024];int font,i,limit;
 static const char missingPacks[]=/* MISSING_PACK_CASE */;
 DC->translateString=Translate;DC->getCVarString=CvarString;
 for(font=0;font<4;++font)DC->Assets.fonts[font]=fontGlyphs[font];
 memset(wide39,'W',39);wide39[39]=0;memset(wide13,'W',13);wide13[13]=0;
 sprintf(colored,"^1%s^7",wide39);
 for(font=0;font<4;++font){
  currentFont=font;Reset(&item);item.parent=&menu;item.typeData=&edit;modernEnabled=1;
  binding="LEFTALT";Item_Bind_Paint(&item);CHECK(drawCount==1&&labelCalls==1);
  CHECK(draws[0].scale==item.textscale&&!strcmp(draws[0].text,binding));
  CHECK(CLOSE(draws[0].x,152)&&CLOSE(draws[0].y,134));CHECK(draws[0].limit==0&&draws[0].style==3);
  drawCount=0;binding="KP_DOWNARROW or KP_RIGHTARROW";Item_Bind_Paint(&item);
  CHECK(drawCount==1&&!strcmp(draws[0].text,binding));CHECK(draws[0].scale<item.textscale&&draws[0].scale>0);
  CHECK(draws[0].x+TextWidthExact(draws[0].text,draws[0].scale,0)<=262.0001f);
  CHECK(!memcmp(draws[0].color,item.window.foreColor,sizeof(vec4_t)));
  modernEnabled=0;drawCount=0;Item_Bind_Paint(&item);
  CHECK(draws[0].scale==item.textscale&&draws[0].x==152&&draws[0].y==134);CHECK(!strcmp(draws[0].text,binding));
  for(limit=0;limit<=5;limit+=5){
   memset(&list,0,sizeof(list));list.elementHeight=21;list.numColumns=2;
   list.columnInfo[0].width=282;list.columnInfo[0].maxChars=limit?limit:39;
   list.columnInfo[1].pos=282;list.columnInfo[1].width=88;list.columnInfo[1].maxChars=13;
   columnText[0]=limit?colored:wide39;columnText[1]=wide13;
   Reset(&item);item.typeData=&list;item.textscale=.21f;item.textaligny=-2;
   item.window.rect.w=604;item.window.rect.h=168;item.cursorPos=-1;
   modernEnabled=1;Item_ListBox_Paint(&item);CHECK(drawCount==2);
   for(i=0;i<2;++i){
    CHECK(!strcmp(draws[i].text,columnText[i])&&draws[i].limit==list.columnInfo[i].maxChars);
    CHECK(draws[i].scale>0&&draws[i].scale<=item.textscale);
    CHECK(draws[i].x+TextWidthExact(draws[i].text,draws[i].scale,draws[i].limit)<=24+list.columnInfo[i].pos+list.columnInfo[i].width+.0001f);
    CHECK(CLOSE(draws[i].y,139)&&draws[i].style==item.textStyle);
    CHECK(!memcmp(draws[i].color,item.window.foreColor,sizeof(vec4_t)));
   }
   drawCount=0;modernEnabled=0;list.startPos=0;Item_ListBox_Paint(&item);CHECK(drawCount==2);
   CHECK(draws[0].scale==.21f&&draws[1].scale==.21f);CHECK(draws[0].limit==list.columnInfo[0].maxChars);
  }
  modernEnabled=1;CHECK(TCE_FitTextScale(&item,"",.22f,0,100)==.22f);
  CHECK(TCE_FitTextScale(&item,"text",.22f,0,0)==0&&TCE_FitTextScale(&item,"text",.22f,0,-1)==0);
  CHECK(!Paragraph(missingPacks,1,1));CHECK(drawCount>6&&draws[0].scale<.26f);
  CHECK(!Paragraph(missingPacks,0,1));CHECK(draws[drawCount-1].y>278); /* actual old painter overflows */
  CHECK(!Paragraph("Connection lost.\nPlease reconnect.",1,0));CHECK(drawCount==2&&draws[0].scale==.26f);
  {float x=draws[1].x,y=draws[1].y;CHECK(!Paragraph("Connection lost.\nPlease reconnect.",0,0));CHECK(drawCount==2&&draws[1].x==x&&draws[1].y==y);}
  memset(longword,'W',1023);longword[1023]=0;CHECK(!Paragraph(longword,1,1));
  strcpy(coloredLong,"^1Error^7\n\n^3");{int n=(int)strlen(coloredLong);memset(coloredLong+n,'W',1000);coloredLong[n+1000]=0;}CHECK(!Paragraph(coloredLong,1,1));
 }
 /* Actual minimum-size helper and real BIND/list/paragraph painters. The
  * preferred cap is tested before field fitting, including descenders on the
  * final list row and a numeric slider readout below its track. */
 for(font=0;font<4;++font){
  static const int widths[]={640,800,1920};int r;
  currentFont=font;
  for(r=0;r<3;++r){float y,scale,bounds[4],factor=(float)widths[r]/(640.f/.75f);
   uiInfo.uiDC.glconfig.vidWidth=widths[r];uiInfo.uiDC.glconfig.vidHeight=r==0?480:r==1?600:1080;
   Reset(&item);item.type=ITEM_TYPE_TEXT;item.text="Help";item.textalignx=2;item.textaligny=14;
   modernEnabled=1;y=item.textaligny;scale=TCE_ModernItemTextScale(&item,&y);
   if(r<2) CHECK((DC->Assets.fonts[font].glyphs['H'].top-1)*DC->Assets.fonts[font].glyphScale*scale*factor>=7.99f);
   else CHECK(scale==item.textscale&&y==item.textaligny);
   TCE_ParagraphInk(&item,item.text,scale,bounds);CHECK(y+bounds[1]>=.999f&&y+bounds[3]<=19.001f);
   CHECK(item.textscale==.22f&&item.textaligny==14); /* policy itself is pure */
   modernEnabled=0;y=item.textaligny;CHECK(TCE_ModernItemTextScale(&item,&y)==.22f&&y==14);
   if(r==2)continue;
   modernEnabled=1;item.text=wide39;item.window.rect.w=40;y=14;scale=TCE_ModernItemTextScale(&item,&y);
   CHECK(TextWidthExact(wide39,scale,0)<=38.001f);CHECK(scale<.22f);
   Reset(&item);item.parent=&menu;item.typeData=&edit;item.type=ITEM_TYPE_BIND;
   item.textalignx=120;item.textalignment=ITEM_ALIGN_RIGHT;item.textaligny=14;
   binding="KP_DOWNARROW or KP_RIGHTARROW";y=14;scale=TCE_ModernItemTextScale(&item,&y);
   item.textscale=scale;item.textRect.y=item.window.rect.y+y;Item_Bind_Paint(&item);
   CHECK(draws[0].x+TextWidthExact(draws[0].text,draws[0].scale,0)<=262.001f);
   memset(&list,0,sizeof(list));list.elementHeight=21;list.numColumns=2;
   list.columnInfo[0].width=282;list.columnInfo[0].maxChars=39;
   list.columnInfo[1].pos=282;list.columnInfo[1].width=88;list.columnInfo[1].maxChars=13;
   Reset(&item);item.type=ITEM_TYPE_LISTBOX;item.typeData=&list;item.window.rect.w=604;item.window.rect.h=21;
   item.text=NULL;item.cvar=NULL;item.textaligny=-2;columnText[0]="gyp";columnText[1]=wide13;
   y=-2;scale=TCE_ModernItemTextScale(&item,&y);item.textscale=scale;item.textaligny=y;
   Item_ListBox_Paint(&item);CHECK(drawCount==2);
   for(i=0;i<2;++i){TCE_ParagraphInk(&item,draws[i].text,draws[i].scale,bounds);
    CHECK(draws[i].y+bounds[1]>=120-.002f&&draws[i].y+bounds[3]<=141.002f);}
   Reset(&item);item.type=ITEM_TYPE_TEXT;item.text=NULL;item.cvar="sensitivity";cvarText="25.00";
   item.window.rect.w=60;item.window.rect.h=14;item.textalignment=ITEM_ALIGN_RIGHT;
   item.textalignx=60;item.textaligny=11;y=11;scale=TCE_ModernItemTextScale(&item,&y);
   TCE_ParagraphInk(&item,cvarText,scale,bounds);CHECK(y+bounds[1]>=.999f&&y+bounds[3]<=13.001f);
   CHECK(TextWidthExact(cvarText,scale,0)<=59.001f);
   CHECK(!Paragraph("Connection lost.\nPlease reconnect.",1,0));CHECK(CLOSE(draws[0].scale,TCE_ModernReadableScale(.26f,&DC->Assets.fonts[font])));
   CHECK(!Paragraph(missingPacks,1,1));CHECK(draws[0].scale<TCE_ModernReadableScale(.26f,&DC->Assets.fonts[font]));
  }
 }
 currentFont=0;CHECK(TextWidthExact("KP_DOWNARROW or KP_RIGHTARROW",.22f,0)>177);
 CHECK(TextWidthExact(wide39,.21f,39)>345&&TextWidthExact(wide13,.21f,13)>115);
 printf("%d real-font field/list paint checks passed\n",checks);return 0;
}
