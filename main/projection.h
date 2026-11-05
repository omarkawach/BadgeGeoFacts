#pragma once
// Equal Earth on the WGS84 ellipsoid, central meridian 0 (EPSG:8857).
// Returns projected metres or 340 x 170 bitmap coordinates.
void equal_earth_wgs84(double latitude,double longitude,double *x,double *y);
void world_map_point(double latitude,double longitude,double *x,double *y);
