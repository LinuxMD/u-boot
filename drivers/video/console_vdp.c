// SPDX-License-Identifier: GPL-2.0+
/*
 *
 */

#include <asm/io.h>
#include <asm/vdp.h>
#include <charset.h>
#include <dm.h>
#include <video.h>
#include <video_console.h>
#include "vidconsole_internal.h"

static char vdp_console_cache[VDP_PLANE_AB_HEIGHT][VDP_PLANE_AB_WIDTH] = { 0 };

/* Tiles go in the free region between the font (0x0000-0x1FFF) and
 * VRAM_WINDOW (0xB000); the sprite table is at its canonical address. */
#define LOGO_TILE_VRAM   0x2000
#define LOGO_TILE_INDEX  (LOGO_TILE_VRAM / 32)   /* 0x100 */

static inline void vdp_ctrl(u16 v) { *VDP_CTRL_PORT = v; }
static inline void vdp_data(u16 v) { *VDP_DATA_PORT = v; }

static inline void vdp_set_reg(u8 reg, u8 val)
{
	vdp_ctrl(0x8000 | (reg << 8) | val);
}

static void vdp_set_addr(u8 code, u16 addr)
{
	vdp_ctrl((u16)((code & 0x03) << 14) | (addr & 0x3FFF));
	vdp_ctrl((u16)((code & 0x3C) << 2) | ((addr >> 14) & 0x03));
}

/* One 32x32 solid-blue sprite in the top-left corner. */
static void vdp_setup_logo_sprite(void)
{
	int i;

	vdp_set_reg(15, 2);             /* auto-increment by 2 */

	/* Blue into palette line 1, colour 1 (CRAM index 17) */
	vdp_set_addr(VDP_CD_CRAM_WR, 17 * 2);
	vdp_data(0x0E00);

	/* 16 tiles (4x4), every pixel = colour index 1 */
	vdp_set_addr(VDP_CD_VRAM_WR, LOGO_TILE_VRAM);
	for (i = 0; i < 16 * 16; i++)   /* 16 tiles * 16 words */
		vdp_data(0x1111);

	/* Sprite attribute table: a single sprite at (128,128) = screen 0,0 */
	vdp_set_addr(VDP_CD_VRAM_WR, VRAM_SPRITE_TABLE);
	vdp_data(0x0080);               /* Y = 128 */
	vdp_data(0x0F00);               /* size 4x4 tiles, link 0 */
	vdp_data(0xA000 | LOGO_TILE_INDEX); /* priority, palette 1, tile */
	vdp_data(0x0080);               /* X = 128 */

	vdp_set_reg(5, VRAM_SPRITE_TABLE >> 9);
}

static int vdp_console_probe(struct udevice *dev)
{
	int ret = console_probe(dev);

	if (ret)
		return ret;

	vdp_setup_logo_sprite();

	return 0;
}

static int console_set_row(struct udevice *dev, uint row, int clr)
{
	//printf("%s:%d\n", __func__, __LINE__);
	/* Since we don't have normal colours assume that "clr" is just
	 * the colour needed to clear anything that is on the line.
	 *
	 * All of the code seems to use the background colour when calling
	 * this so that assumption seems correct.
	 */
	vdp_set_tiles(VRAM_PLANE_B, (row * VDP_PLANE_AB_WIDTH), 0, VDP_PLANE_AB_WIDTH);
	memset(&vdp_console_cache[row], 0, VDP_PLANE_AB_WIDTH);

	return 0;
}

static inline void vdp_putc(u16 tile, const char ch)
{
	vdp_set_tile(VRAM_PLANE_B, tile, ch);
}

static int console_move_rows(struct udevice *dev, uint rowdst, uint rowsrc, uint count)
{
	unsigned row = 0, col = 0;
	memmove(&vdp_console_cache[rowdst][0], &vdp_console_cache[rowsrc][0],
		count * VDP_PLANE_AB_WIDTH);

	for (row = 0; row < VDP_PLANE_AB_HEIGHT; row++) {
		for(col = 0; col < VDP_PLANE_AB_WIDTH; col++)
			vdp_putc((row * VDP_PLANE_AB_WIDTH) + col, vdp_console_cache[row][col]);
	}

	return 0;
}


static int console_putc_xy(struct udevice *dev, uint x_frac, uint y, int cp)
{
	//printf("%s:%d %d %d\n", __func__, __LINE__, (unsigned) x_frac, (unsigned) y);

	unsigned row = 0;
	unsigned col = 0;

	if (x_frac)
		col = x_frac / VDP_TILE_WIDTH;

	/* Return EAGAIN to trigger a new line? */
	if (col >= VDP_PLANE_AB_WIDTH)
		return -EAGAIN;

	if (y)
		row = y / 8;

	vdp_putc((row * VDP_PLANE_AB_WIDTH) + col, (char) cp);
	vdp_console_cache[row][col] = (char) cp;

	/* width of one tile */
	return VDP_TILE_WIDTH;
}

static int console_set_cursor_visible(struct udevice *dev, bool visible,
				      uint x, uint y, uint index)
{
	//printf("%s:%d\n", __func__, __LINE__);

	return 0;
}

struct vidconsole_ops console_ops = {
	.putc_xy		= console_putc_xy,
	.move_rows		= console_move_rows,
	.set_row		= console_set_row,
	.get_font_size		= console_simple_get_font_size,
	.get_font		= console_simple_get_font,
	.select_font		= console_simple_select_font,
	.set_cursor_visible	= console_set_cursor_visible,
};

U_BOOT_DRIVER(vidconsole_vdp) = {
	.name		= "vdp_console",
	.id		= UCLASS_VIDEO_CONSOLE,
	.ops		= &console_ops,
	.probe		= vdp_console_probe,
	.priv_auto	= sizeof(struct console_simple_priv),
};
