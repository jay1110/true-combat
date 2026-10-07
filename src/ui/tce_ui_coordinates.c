#include "tce_ui_coordinates.h"
/* UI 40002600 and shared UI 4001c370. Preserve original float constants. */
void TCE_UI_AdjustCoordinates(float *x,float *y,float *w,float *h,
                             int width,int height,float xs,float ys) {
    const double horizontal=0.750000059604644775390625;
    double scale,shifted;
    if(width*480!=height*640) {
        scale=((double)width*480.0/(double)height)*0.00117187504656612873077392578125;
        *x=(float)((double)xs * *x * horizontal);
        shifted=(1.0/scale-1.0)*240.0+*y;
        *y=(float)shifted;
        *y=(float)(shifted*ys*scale);
        *w=(float)((double)xs * *w * horizontal);
        *h=(float)((double)ys * *h * scale);
    } else {
        *x=(float)((double)xs * *x * horizontal);
        shifted=(double)*y+80.0;*y=(float)shifted;
        *y=(float)(shifted*ys*horizontal);
        *w=(float)((double)xs * *w * horizontal);
        *h=(float)((double)ys * *h * horizontal);
    }
}
