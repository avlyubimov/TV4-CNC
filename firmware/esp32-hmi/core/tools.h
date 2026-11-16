#pragma once
#include <stdbool.h>
#include <stdint.h>
#define TOOL_COUNT 4u
typedef struct { int32_t x_um,z_um,radius_um; } tool_geometry_t;
typedef struct { tool_geometry_t geometry[TOOL_COUNT]; unsigned active; uint32_t revision; } tools_t;
void tools_init(tools_t *tools);
bool tools_geometry_valid(const tool_geometry_t *geometry);
bool tools_save(tools_t *tools,unsigned index,const tool_geometry_t *geometry);
bool tools_activate(tools_t *tools,unsigned index);
/* Exact decimal mm, comma or dot, at most 3 decimals; bounded 16-byte input.
   On failure, output is untouched. No rounding or locale-dependent parsing. */
bool tools_parse_mm(const char *text,int32_t *um);
