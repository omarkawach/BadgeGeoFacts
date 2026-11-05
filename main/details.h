#pragma once
#include "display.h"
#define GEO_DETAIL_ROWS 7
typedef struct {char label[24],value[96];} GeoDetail;
typedef struct {GeoDetail rows[GEO_DETAIL_ROWS];unsigned count;} GeoDetails;
void build_details(const Fact *fact,GeoDetails *details);
