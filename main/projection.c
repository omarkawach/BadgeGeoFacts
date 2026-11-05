#include <math.h>
#include "projection.h"
#define GEO_PI 3.14159265358979323846
static double authalic_q(double sine){
 const double es=0.0066943799901413165,e=0.08181919084262149;
 return (1-es)*(sine/(1-es*sine*sine)-log((1-e*sine)/(1+e*sine))/(2*e));
}
void equal_earth_wgs84(double latitude,double longitude,double *x,double *y){
 const double a1=1.340264,a2=-0.081106,a3=0.000893,a4=0.003796,m=0.86602540378443864676;
 double qp=authalic_q(1),beta=authalic_q(sin(latitude*GEO_PI/180))/qp;
 if(beta>1){beta=1;}
 if(beta<-1){beta=-1;}
 double theta=asin(m*beta),t2=theta*theta,t6=t2*t2*t2;
 double radius=6378137.0*sqrt(qp/2);
 *x=radius*(longitude*GEO_PI/180)*cos(theta)/(m*(a1+3*a2*t2+t6*(7*a3+9*a4*t2)));
 *y=radius*theta*(a1+a2*t2+t6*(a3+a4*t2));
}
void world_map_point(double latitude,double longitude,double *x,double *y){
 double xmax,dummy,ymax,px,py;equal_earth_wgs84(0,180,&xmax,&dummy);equal_earth_wgs84(90,0,&dummy,&ymax);
 double scale=fmin(339/(2*xmax),169/(2*ymax));equal_earth_wgs84(latitude,longitude,&px,&py);
 *x=169.5+scale*px;*y=84.5-scale*py;
}
