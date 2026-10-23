/* SPDX-License-Identifier: GPL-2.0-or-later
 * Camera similarity tracking on a small, aspect-preserving image. No SDK or
 * camera access: only the already rendered source. Coordinates are image pixels.
 */
#ifndef SKETCH_MOTION_H
#define SKETCH_MOTION_H
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#define MOTION_MAX 192
#define MOTION_POINTS 160
struct motion_point { double x, y, u, v; };
struct motion {
    uint8_t previous[MOTION_MAX*MOTION_MAX*4];
    int w, h;
    bool ready;
    double a, b, tx, ty; /* initial -> current: [a -b; b a] p + t */
    double anchor_a, anchor_b, anchor_tx, anchor_ty;
    int matched, inliers;
};
static void motion_reset(struct motion *m)
{
    m->ready = false; m->a = 1; m->b = m->tx = m->ty = 0;
    m->anchor_a=1; m->anchor_b=m->anchor_tx=m->anchor_ty=0;
    m->matched = m->inliers = 0;
}
static double motion_cost(const uint8_t *p, const uint8_t *q, int w, int x, int y, int u, int v)
{
    double sum = 0, squared = 0;
    for (int j=-2; j<=2; j++) for (int i=-2; i<=2; i++) {
        int k=((y+j)*w+x+i)*4, l=((v+j)*w+u+i)*4;
        if (p[k+3]<64 || q[l+3]<64) return 1e9;
        double d=(double)p[k]-q[l]; sum+=d; squared+=d*d;
    }
    // Mean removal makes tracking tolerant of exposure changes; retain a small
    // brightness penalty to disambiguate similar patches on different surfaces.
    return (squared - .9*sum*sum/25)/25;
}
static double motion_error(struct motion_point p, double a, double b, double tx, double ty)
{
    double dx=a*p.x-b*p.y+tx-p.u, dy=b*p.x+a*p.y+ty-p.v;
    return dx*dx+dy*dy;
}
static double motion_sample(const uint8_t *p,int w,int h,double x,double y)
{
    x=fmax(0,fmin(w-1.001,x)); y=fmax(0,fmin(h-1.001,y));
    int ix=(int)x,iy=(int)y; double fx=x-ix,fy=y-iy;
    int k=(iy*w+ix)*4;
    return (p[k]*(1-fx)+p[k+4]*fx)*(1-fy)+(p[k+w*4]*(1-fx)+p[k+w*4+4]*fx)*fy;
}
static void motion_refine(const uint8_t *p,const uint8_t *q,int w,int h,int x,int y,double *u,double *v)
{
    // Joint subpixel refinement avoids accumulating the axis-wise parabola bias
    // during slow pans and rotations. Solve brightness-normalised Lucas-Kanade.
    for(int iter=0;iter<6;iter++) {
        double residual[25],gx[25],gy[25],mean=0,mx=0,my=0; int k=0;
        for(int j=-2;j<=2;j++) for(int i=-2;i<=2;i++,k++) {
            double sx=*u+i,sy=*v+j;
            residual[k]=motion_sample(q,w,h,sx,sy)-p[((y+j)*w+x+i)*4];
            gx[k]=motion_sample(q,w,h,sx+.5,sy)-motion_sample(q,w,h,sx-.5,sy);
            gy[k]=motion_sample(q,w,h,sx,sy+.5)-motion_sample(q,w,h,sx,sy-.5);
            mean+=residual[k]; mx+=gx[k]; my+=gy[k];
        }
        mean/=25; mx/=25; my/=25;
        double xx=0,xy=0,yy=0,ex=0,ey=0;
        for(k=0;k<25;k++) {
            double dx=gx[k]-mx,dy=gy[k]-my,e=residual[k]-mean;
            xx+=dx*dx; xy+=dx*dy; yy+=dy*dy; ex+=e*dx; ey+=e*dy;
        }
        double det=xx*yy-xy*xy; if(det<1e-6) break;
        double dx=(yy*ex-xy*ey)/det,dy=(xx*ey-xy*ex)/det;
        *u-=fmax(-.5,fmin(.5,dx)); *v-=fmax(-.5,fmin(.5,dy));
        if(dx*dx+dy*dy<.00001) break;
    }
}
static bool motion_fit(struct motion_point *p, int n, double *a, double *b, double *tx, double *ty, int *inliers)
{
    if (n<8) return false;
    int best=0;
    double ba=1, bb=0, bx=0, by=0, best_error=1e20;
    // Deterministic RANSAC: moving foreground objects must not drag the camera.
    uint32_t rng=0x736b6574;
    for (int k=0;k<160;k++) {
        rng=rng*1664525u+1013904223u; int i=(int)((rng>>8)%(unsigned)n);
        rng=rng*1664525u+1013904223u; int j=(int)((rng>>8)%(unsigned)n);
        double dx=p[j].x-p[i].x, dy=p[j].y-p[i].y, norm=dx*dx+dy*dy;
        if (norm<225) continue;
        double du=p[j].u-p[i].u, dv=p[j].v-p[i].v;
        double ca=(du*dx+dv*dy)/norm, cb=(dv*dx-du*dy)/norm;
        double scale=ca*ca+cb*cb;
        if (scale<.85*.85 || scale>1.18*1.18 || fabs(cb)>.15) continue;
        double cx=p[i].u-ca*p[i].x+cb*p[i].y, cy=p[i].v-cb*p[i].x-ca*p[i].y;
        int count=0; double error=0;
        for (int z=0;z<n;z++) { double e=motion_error(p[z],ca,cb,cx,cy); if(e<1.44) { count++; error+=e; } }
        if (count>best || (count==best && error<best_error)) { best=count; best_error=error; ba=ca; bb=cb; bx=cx; by=cy; }
    }
    if (best<8 || best<n*.45) return false;
    // Least-squares refinement over the consensus, centred to avoid ill conditioning.
    for (int pass=0;pass<2;pass++) {
        double x=0,y=0,u=0,v=0; int count=0;
        for(int i=0;i<n;i++) if(motion_error(p[i],ba,bb,bx,by)<1.44) { x+=p[i].x; y+=p[i].y; u+=p[i].u; v+=p[i].v; count++; }
        if(count<8) return false;
        x/=count; y/=count; u/=count; v/=count;
        double aa=0, ab=0, norm=0;
        for(int i=0;i<n;i++) if(motion_error(p[i],ba,bb,bx,by)<1.44) {
            double dx=p[i].x-x,dy=p[i].y-y,du=p[i].u-u,dv=p[i].v-v;
            aa+=dx*du+dy*dv; ab+=dx*dv-dy*du; norm+=dx*dx+dy*dy;
        }
        if(norm<400) return false;
        ba=aa/norm; bb=ab/norm; bx=u-ba*x+bb*y; by=v-bb*x-ba*y; best=count;
    }
    *a=ba; *b=bb; *tx=bx; *ty=by; *inliers=best;
    return true;
}
static void motion_update(struct motion *m, const uint8_t *frame, int w, int h)
{
    if(w<32 || h<32 || w>MOTION_MAX || h>MOTION_MAX) { motion_reset(m); return; }
    size_t bytes=(size_t)w*h*4;
    if(!m->ready || m->w!=w || m->h!=h) {
        motion_reset(m); m->w=w; m->h=h; m->ready=true;
        memcpy(m->previous,frame,bytes); return;
    }
    if(!memcmp(m->previous,frame,bytes)) {
        m->a=m->anchor_a; m->b=m->anchor_b; m->tx=m->anchor_tx; m->ty=m->anchor_ty;
        return;
    }
    struct motion_point points[MOTION_POINTS]; int n=0;
    const uint8_t *prev=m->previous;
    // Pick a strong corner inside each cell, not a single edge (aperture problem).
    for(int cy=14;cy<h-14;cy+=12) for(int cx=14;cx<w-14;cx+=12) {
        int x=cx,y=cy; double quality=30;
        for(int oy=-3;oy<=3;oy+=2) for(int ox=-3;ox<=3;ox+=2) {
            double xx=0,xy=0,yy=0;
            for(int j=-1;j<=1;j++) for(int i=-1;i<=1;i++) {
                int k=((cy+oy+j)*w+cx+ox+i)*4;
                double gx=(double)prev[k+4]-prev[k-4], gy=(double)prev[k+w*4]-prev[k-w*4];
                xx+=gx*gx; xy+=gx*gy; yy+=gy*gy;
            }
            double q=(xx+yy-sqrt((xx-yy)*(xx-yy)+4*xy*xy))/18;
            if(q>quality) {quality=q; x=cx+ox; y=cy+oy;}
        }
        if(quality<=30 || prev[(y*w+x)*4+3]<128) continue;
        double best=1e9; int dx=0,dy=0;
        for(int j=-9;j<=9;j++) for(int i=-9;i<=9;i++) {
            double e=motion_cost(prev,frame,w,x,y,x+i,y+j);
            if(e<best) {best=e; dx=i; dy=j;}
        }
        if(best>400 || abs(dx)==9 || abs(dy)==9) continue;
        double second=1e9;
        for(int j=-9;j<=9;j+=2) for(int i=-9;i<=9;i+=2) {
            if(abs(i-dx)<=2 && abs(j-dy)<=2) continue;
            double e=motion_cost(prev,frame,w,x,y,x+i,y+j);
            if(e<second) second=e;
        }
        if(second<best*1.35+2) continue;
        double l=motion_cost(prev,frame,w,x,y,x+dx-1,y+dy), r=motion_cost(prev,frame,w,x,y,x+dx+1,y+dy);
        double t=motion_cost(prev,frame,w,x,y,x+dx,y+dy-1), b=motion_cost(prev,frame,w,x,y,x+dx,y+dy+1);
        double sx=l+r-2*best, sy=t+b-2*best;
        double fx=sx>1e-6 ? .5*(l-r)/sx : 0, fy=sy>1e-6 ? .5*(t-b)/sy : 0;
        fx=fmax(-.5,fmin(.5,fx)); fy=fmax(-.5,fmin(.5,fy));
        double u=x+dx+fx,v=y+dy+fy;
        motion_refine(prev,frame,w,h,x,y,&u,&v);
        if(fabs(u-x)>9 || fabs(v-y)>9) continue;
        if(n<MOTION_POINTS) points[n++]=(struct motion_point){x,y,u,v};
    }
    double a,b,tx,ty; m->matched=n; m->inliers=0;
    if(motion_fit(points,n,&a,&b,&tx,&ty,&m->inliers)) {
        double na=a*m->anchor_a-b*m->anchor_b, nb=b*m->anchor_a+a*m->anchor_b;
        double nx=a*m->anchor_tx-b*m->anchor_ty+tx, ny=b*m->anchor_tx+a*m->anchor_ty+ty;
        // Bound accumulated scale to keep the pattern numerically well behaved.
        if(na*na+nb*nb>.04 && na*na+nb*nb<25) {m->a=na; m->b=nb; m->tx=nx; m->ty=ny;}
        else motion_reset(m);
        double travel=0;
        for(int y=0;y<=h;y+=h) for(int x=0;x<=w;x+=w) {
            double dx=a*x-b*y+tx-x,dy=b*x+a*y+ty-y;
            travel=fmax(travel,sqrt(dx*dx+dy*dy));
        }
        // Reuse a keyframe for small moves instead of accumulating fractional
        // interpolation errors at every frame. Refresh before the search limit.
        if(m->ready && travel<4) return;
    } else {
        double change=0;
        for(size_t i=0;i<bytes;i+=4) change+=abs((int)prev[i]-(int)frame[i]);
        // Scene cut: re-anchor. Weak/flat images otherwise hold the last transform.
        if(change/(w*h)>35) motion_reset(m);
    }
    memcpy(m->previous,frame,bytes); m->ready=true; m->w=w; m->h=h;
    m->anchor_a=m->a; m->anchor_b=m->b; m->anchor_tx=m->tx; m->anchor_ty=m->ty;
}
#endif
