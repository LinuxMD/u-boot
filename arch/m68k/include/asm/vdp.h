#ifndef _VDP_H
#define _VDP_H

#define VRAM_WINDOW			0xB000
#define VRAM_PLANE_A			0xC000
#define VRAM_PLANE_B			0xE000
#define VRAM_SPRITE_TABLE		0xF000
#define VRAM_HSCROLL			0xF800

#define VDP_PLANE_AB_WIDTH  32
#define VDP_PLANE_AB_HEIGHT 28
#define VDP_TILE_WIDTH 8

/* CPU port addresses */
#define VDP_PORT_DATA			0xC00000
#define VDP_PORT_CTRL			0xC00004

/* Data-port command codes (CD) */
#define VDP_CD_VRAM_WR			0x01
#define VDP_CD_CRAM_WR			0x03
#define VDP_CD_VSRAM_WR			0x05

#ifndef __ASSEMBLY__
#define VDP_DATA_PORT			((volatile u16 *)VDP_PORT_DATA)
#define VDP_CTRL_PORT			((volatile u16 *)VDP_PORT_CTRL)

void vdp_set_tile(u16 plane_addr, u16 plane_index, u8 tile_index);
void vdp_set_tiles(u16 plane_addr, u16 plane_index, u8 tile_index, u16 num);
#endif


#endif /* _VDP_H */
