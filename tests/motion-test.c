/* Motion estimator regression tests with known pan, roll, zoom and scene cuts. */
#include "../src/motion.h"
#include <stdio.h>

static uint8_t frame[192*128*4];
static double signal(double x,double y)
{
    return 125+35*sin(x*.31)+29*cos(y*.37)+21*sin(x*.55+y*.49)+15*cos(x*.23-y*.61);
}
static void make_frame(double a,double b,double tx,double ty)
{
    double norm=a*a+b*b;
    for(int y=0;y<128;y++) for(int x=0;x<192;x++) {
        double u=(a*(x-tx)+b*(y-ty))/norm, v=(-b*(x-tx)+a*(y-ty))/norm;
        int k=(y*192+x)*4;
        uint8_t value=(uint8_t)fmax(0,fmin(255,signal(u,v)));
        frame[k]=frame[k+1]=frame[k+2]=value; frame[k+3]=255;
    }
}
int main(void)
{
    int failures=0;
    for(int mode=0;mode<3;mode++) {
        struct motion *m=calloc(1,sizeof(*m)); motion_reset(m);
        double worst=0;
        for(int step=0;step<=24;step++) {
            double angle=mode==2 ? step*.005 : 0, scale=mode==1 ? 1+step*.003 : 1;
            double a=scale*cos(angle),b=scale*sin(angle);
            double tx=mode==0 ? step*1.25 : 96-a*96+b*64;
            double ty=mode==0 ? step*-.65 : 64-b*96-a*64;
            make_frame(a,b,tx,ty); motion_update(m,frame,192,128);
            for(int y=32;y<=96;y+=64) for(int x=32;x<=160;x+=128) {
                double dx=m->a*x-m->b*y+m->tx-(a*x-b*y+tx),dy=m->b*x+m->a*y+m->ty-(b*x+a*y+ty);
                double e=sqrt(dx*dx+dy*dy); if(e>worst) worst=e;
            }
        }
        printf("mode=%d worst_anchor_error=%.3f analysis_px inliers=%d/%d\n",mode,worst,m->inliers,m->matched);
        if(worst>.4) failures++;
        double before=m->tx; motion_update(m,frame,192,128);
        if(m->tx!=before) { printf("FAIL static drift\n"); failures++; }
        memset(frame,255,sizeof(frame)); motion_update(m,frame,192,128);
        if(m->a!=1 || m->tx!=0 || m->ty!=0 || m->b!=0) { printf("FAIL cut reset\n"); failures++; }
        free(m);
    }
    printf("MOTION RESULT: %s (%d failures)\n",failures?"FAIL":"PASS",failures);
    return failures?1:0;
}
