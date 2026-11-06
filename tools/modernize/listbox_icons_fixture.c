/* Actual optional feeder-image renderer with a recording draw service. */
#include <stdio.h>
#include <string.h>
#include <math.h>
typedef int qboolean;
typedef int qhandle_t;
typedef struct {
 float elementHeight;
 struct { int pos, width; } columnInfo[2];
} listBoxDef_t;
typedef struct { float x,y,w,h;int image; } draw_t;
static draw_t draws[16];
static int drawCount,modernEnabled,checks;
static qboolean TCE_ModernUIEnabled(void) { return modernEnabled; }
static void Draw(float x,float y,float w,float h,qhandle_t image) {
 draw_t *p=&draws[drawCount++];p->x=x;p->y=y;p->w=w;p->h=h;p->image=image;
}
static struct { void (*drawHandlePic)(float,float,float,float,qhandle_t); } context={Draw},*DC=&context;
/* ACTUAL_ICON_RENDERER */
#define CHECK(test) do {++checks;if(!(test)){fprintf(stderr,"failed line %d: %s\n",__LINE__,#test);return 1;}} while(0)
#define CLOSE(a,b) (fabsf((a)-(b))<.0001f)
static void Paint(int modern,float height,int width,int count,qhandle_t *images) {
 listBoxDef_t list;
 memset(&list,0,sizeof(list));memset(draws,0,sizeof(draws));drawCount=0;
 modernEnabled=modern;list.elementHeight=height;list.columnInfo[0].pos=492;list.columnInfo[0].width=width;
 TCE_ListBoxOptionalImages(224,232,&list,0,count,images);
}
int main(void) {
 qhandle_t images[8]={10,11,12,13,14,15,16,17};
 draw_t classic[16];int k,mask,expected;
 Paint(0,21,84,7,images);CHECK(drawCount==7);
 for(k=0;k<7;++k){CHECK(CLOSE(draws[k].x,717+k*21));CHECK(CLOSE(draws[k].y,233));CHECK(CLOSE(draws[k].w,19)&&CLOSE(draws[k].h,19));}
 CHECK(CLOSE(draws[6].x+draws[6].w,862)); /* exact legacy footprint, independent of column width */
 Paint(0,12,84,7,images);CHECK(drawCount==7);memcpy(classic,draws,sizeof(draws));
 for(k=0;k<7;++k){CHECK(CLOSE(draws[k].x,717+k*12));CHECK(CLOSE(draws[k].y,233));CHECK(CLOSE(draws[k].w,10)&&CLOSE(draws[k].h,10));}
 Paint(1,12,84,7,images);CHECK(!memcmp(classic,draws,sizeof(draws))); /* original 12-unit layout stays identical */
 Paint(1,21,84,7,images);CHECK(drawCount==7);
 for(k=0;k<7;++k){CHECK(CLOSE(draws[k].x,717+k*12));CHECK(CLOSE(draws[k].y,237.5f));CHECK(CLOSE(draws[k].w,10)&&CLOSE(draws[k].h,10));CHECK(draws[k].x>=716&&draws[k].x+draws[k].w<=800);}
 Paint(1,21,12,1,images);CHECK(drawCount==1);CHECK(CLOSE(draws[0].x,717)&&CLOSE(draws[0].y,237.5f));CHECK(CLOSE(draws[0].w,10)&&CLOSE(draws[0].h,10));CHECK(draws[0].x+draws[0].w<728);
 Paint(0,21,12,1,images);CHECK(drawCount==1&&CLOSE(draws[0].w,19)&&CLOSE(draws[0].y,233));
 Paint(1,21,196,7,images);CHECK(drawCount==7&&CLOSE(draws[6].x,843)&&CLOSE(draws[6].w,19));
 Paint(1,21,28,7,images);CHECK(drawCount==7&&CLOSE(draws[6].x,741)&&CLOSE(draws[6].w,2));
 for(mask=0;mask<128;++mask) {
  expected=0;
  for(k=0;k<7;++k) images[k]=(mask&(1<<k))?10+k:-1;
  Paint(1,21,84,7,images);
  for(k=0;k<7;++k)if(mask&(1<<k)) {
   CHECK(draws[expected].image==10+k);CHECK(CLOSE(draws[expected].x,717+k*12));
   CHECK(CLOSE(draws[expected].y,237.5f));CHECK(CLOSE(draws[expected].w,10)&&CLOSE(draws[expected].h,10));++expected;
  }
  CHECK(drawCount==expected); /* absent flags preserve their original fixed slots */
 }
 images[0]=0;images[1]=-1;Paint(1,21,24,2,images);CHECK(drawCount==1&&draws[0].image==0&&CLOSE(draws[0].w,10));
 Paint(1,21,0,1,images);CHECK(!drawCount);Paint(1,21,-1,1,images);CHECK(!drawCount);
 Paint(1,2,84,1,images);CHECK(!drawCount);Paint(1,21,14,7,images);CHECK(!drawCount);
 Paint(1,21,84,0,images);CHECK(!drawCount);Paint(0,21,0,1,images);CHECK(drawCount==1&&CLOSE(draws[0].w,19));
 printf("%d actual listbox icon checks passed\n",checks);return 0;
}
