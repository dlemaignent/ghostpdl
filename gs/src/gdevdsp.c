/* Copyright (C) 2001, Ghostgum Software Pty Ltd.  All rights reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.
   
   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.
   
   For more information about licensing, please refer to
   http://www.ghostscript.com/licensing/. For information on
   commercial licensing, go to http://www.artifex.com/licensing/ or
   contact Artifex Software, Inc., 101 Lucas Valley Road #110,
   San Rafael, CA  94903, U.S.A., +1(415)492-9861.
 */

/* gdevdsp.c */
/* $Id$ */

/*
 * DLL based display device driver.
 *
 * by Russell Lang, Ghostgum Software Pty Ltd
 *
 * This device is intended to be used for displays when
 * Ghostscript is loaded as a DLL/shared library/static library.  
 * It is intended to work for Windows, OS/2, Linux, Mac OS 9 and 
 * hopefully others.
 *
 * Before this device is opened, the address of a structure must 
 * be provided using gsapi_set_display_callback(minst, callback);
 * This structure contains callback functions to notify the 
 * caller when the device is opened, closed, resized, showpage etc.
 * The structure is defined in gdevdsp.h.
 *
 * Not all combinatinos of display formats have been tested.
 * At the end of this file is some example code showing which
 * formats have been tested.
 */

#include "string_.h"
#include "gx.h"
#include "gserrors.h"
#include "gsdevice.h"		/* for gs_copydevice */
#include "gxdevice.h"

#include "gp.h"
#include "gpcheck.h"
#include "gsparam.h"

#include "gdevpccm.h"		/* 4-bit PC color */
#include "gxdevmem.h"
#include "gdevdsp.h"
#include "gdevdsp2.h"

/* Initial values for width and height */
#define INITIAL_RESOLUTION 96
#define INITIAL_WIDTH ((INITIAL_RESOLUTION * 85 + 5) / 10)
#define INITIAL_HEIGHT ((INITIAL_RESOLUTION * 110 + 5) / 10)

/* Device procedures */

/* See gxdevice.h for the definitions of the procedures. */
private dev_proc_open_device(display_open);
private dev_proc_get_initial_matrix(display_get_initial_matrix);
private dev_proc_sync_output(display_sync_output);
private dev_proc_output_page(display_output_page);
private dev_proc_close_device(display_close);

private dev_proc_map_rgb_color(display_map_rgb_color_device4);
private dev_proc_map_color_rgb(display_map_color_rgb_device4);
private dev_proc_map_rgb_color(display_map_rgb_color_device8);
private dev_proc_map_color_rgb(display_map_color_rgb_device8);
private dev_proc_map_rgb_color(display_map_rgb_color_device16);
private dev_proc_map_color_rgb(display_map_color_rgb_device16);
private dev_proc_map_rgb_color(display_map_rgb_color_rgb);
private dev_proc_map_color_rgb(display_map_color_rgb_rgb);
private dev_proc_map_rgb_color(display_map_rgb_color_bgr24);
private dev_proc_map_color_rgb(display_map_color_rgb_bgr24);

private dev_proc_fill_rectangle(display_fill_rectangle);
private dev_proc_copy_mono(display_copy_mono);
private dev_proc_copy_color(display_copy_color);
private dev_proc_get_bits(display_get_bits);
private dev_proc_get_params(display_get_params);
private dev_proc_put_params(display_put_params);
private dev_proc_finish_copydevice(display_finish_copydevice);

private const gx_device_procs display_procs =
{
    display_open,
    display_get_initial_matrix,
    display_sync_output,
    display_output_page,
    display_close,
    gx_default_w_b_map_rgb_color,
    gx_default_w_b_map_color_rgb,
    display_fill_rectangle,
    NULL,				/* tile rectangle */
    display_copy_mono,
    display_copy_color,
    NULL,				/* draw line */
    display_get_bits,
    display_get_params,
    display_put_params,
    gx_default_cmyk_map_cmyk_color, 	/* map_cmyk_color */
    gx_default_get_xfont_procs,
    NULL,				/* get_xfont_device */
    NULL,				/* map_rgb_alpha_color */
    gx_page_device_get_page_device,
    /* extra entries */
    NULL,				/* get_alpha_bits */
    NULL,				/* copy_alpha */
    NULL,				/* get_band */
    NULL,				/* copy_rop */
    NULL,				/* fill_path */
    NULL,				/* stroke_path */
    NULL,				/* fill_mask */
    NULL,				/* fill_trapezoid */
    NULL,				/* fill_parallelogram */
    NULL,				/* fill_triangle */
    NULL,				/* draw_thin_line */
    NULL,				/* begin_image */
    NULL,				/* image_data */
    NULL,				/* end_image */
    NULL,				/* strip_tile_rectangle */
    NULL,				/* strip_copy_rop */
    NULL,				/* get_clipping_box */
    NULL,				/* begin_typed_image */
    NULL,				/* get_bits_rectangle */
    NULL,				/* map_color_rgb_alpha */
    NULL,				/* create_compositor */
    NULL,				/* get_hardware_params */
    NULL,				/* text_begin */
    display_finish_copydevice,		/* finish_copydevice */
    NULL,				/* begin_transparency_group */
    NULL,				/* end_transparency_group */
    NULL,				/* begin_transparency_mask */
    NULL,				/* end_transparency_mask */
    NULL				/* discard_transparency_layer */
};

/* GC descriptor */
public_st_device_display();

private 
ENUM_PTRS_WITH(display_enum_ptrs, gx_device_display *ddev) return 0;
    case 0: 
	if (ddev->mdev) {
	    return ENUM_OBJ(gx_device_enum_ptr((gx_device *)ddev->mdev));
	}
	return 0;
ENUM_PTRS_END

private 
RELOC_PTRS_WITH(display_reloc_ptrs, gx_device_display *ddev)
    if (ddev->mdev) {
	ddev->mdev = (gx_device_memory *)
	    gx_device_reloc_ptr((gx_device *)ddev->mdev, gcst);
    }
RELOC_PTRS_END


const gx_device_display gs_display_device =
{
    std_device_std_body_type(gx_device_display, &display_procs, "display",
			&st_device_display,
			INITIAL_WIDTH, INITIAL_HEIGHT,
			INITIAL_RESOLUTION, INITIAL_RESOLUTION),
    {0},			/* std_procs */
    NULL,			/* mdev */
    NULL,			/* callback */
    NULL,			/* pHandle */
    0,				/* nFormat */
    NULL,			/* pBitmap */
    0, 				/* ulBitmapSize */
};



/* prototypes for internal procedures */
private int display_check_structure(gx_device_display *dev);
private void display_free_bitmap(gx_device_display * dev);
private int display_alloc_bitmap(gx_device_display *, gx_device *);
private int display_set_color_format(gx_device_display *dev, int nFormat);

/* Open the display driver. */
private int
display_open(gx_device * dev)
{
    gx_device_display *ddev = (gx_device_display *) dev;
    int ccode;

    /* Erase these, in case we are opening a copied device. */
    ddev->mdev = NULL;
    ddev->pBitmap = NULL;
    ddev->ulBitmapSize = 0;

    /* Allow device to be opened "disabled" without a callback. */
    /* The callback will be set later and the device re-opened. */
    if (ddev->callback == NULL)
	return 0;

    /* Make sure we have been passed a valid callback structure. */
    if ((ccode = display_check_structure(ddev)) < 0)
	return_error(ccode);

    /* set color info */
    if ((ccode = display_set_color_format(ddev, ddev->nFormat)) < 0)
	return_error(ccode);

    /* Tell caller that the device is open. */
    /* This is always the first callback */
    ccode = (*(ddev->callback->display_open))(ddev->pHandle, dev);
    if (ccode < 0)
	return_error(ccode);

    /* Tell caller the proposed device parameters */
    ccode = (*(ddev->callback->display_presize)) (ddev->pHandle, dev,
	dev->width, dev->height, gdev_mem_raster(dev), ddev->nFormat);
    if (ccode < 0) {
	(*(ddev->callback->display_close))(ddev->pHandle, dev);
	return_error(ccode);
    }

    /* allocate the image */
    ccode = display_alloc_bitmap(ddev, dev);
    if (ccode < 0) {
	(*(ddev->callback->display_close))(ddev->pHandle, dev);
	return_error(ccode);
    }

    /* Tell caller the device parameters */
    ccode = (*(ddev->callback->display_size)) (ddev->pHandle, dev,
	dev->width, dev->height, gdev_mem_raster(dev), ddev->nFormat, 
	ddev->mdev->base);
    if (ccode < 0) {
	display_free_bitmap(ddev);
	(*(ddev->callback->display_close))(ddev->pHandle, dev);
	return_error(ccode);
    }

    return 0;
}

private void
display_get_initial_matrix(gx_device * dev, gs_matrix * pmat)
{
    gx_device_display *ddev = (gx_device_display *) dev;
    if ((ddev->nFormat & DISPLAY_FIRSTROW_MASK) == DISPLAY_TOPFIRST)
	gx_default_get_initial_matrix(dev, pmat);
    else
	gx_upright_get_initial_matrix(dev, pmat);  /* Windows / OS/2 */
}

/* Update the display. */
int
display_sync_output(gx_device * dev)
{
    gx_device_display *ddev = (gx_device_display *) dev;
    if (ddev->callback == NULL)
	return 0;
    (*(ddev->callback->display_sync))(ddev->pHandle, dev);
    return (0);
}

/* Update the display, bring to foreground. */
/* If you want to pause on showpage, delay your return from callback */
int
display_output_page(gx_device * dev, int copies, int flush)
{
    gx_device_display *ddev = (gx_device_display *) dev;
    int code;
    if (ddev->callback == NULL)
	return 0;

    code = (*(ddev->callback->display_page))
			(ddev->pHandle, dev, copies, flush);

    if (code >= 0)
	code = gx_finish_output_page(dev, copies, flush);
    return code;
}

/* Close the display driver */
private int
display_close(gx_device * dev)
{
    gx_device_display *ddev = (gx_device_display *) dev;
    if (ddev->callback == NULL)
	return 0;

    /* Tell caller that device is about to be closed. */
    (*(ddev->callback->display_preclose))(ddev->pHandle, dev);

    /* Release memory. */
    display_free_bitmap(ddev);

    /* Tell caller that device is closed. */
    /* This is always the last callback */
    (*(ddev->callback->display_close))(ddev->pHandle, dev);

    return 0;
}

/*
 * This routine will encode a 1 Black on white color.
 */
private gx_color_index
gx_b_w_gray_encode(gx_device * dev, const gx_color_value cv[])
{
    return 1 - (cv[0] >> (gx_color_value_bits - 1));
}

/* DISPLAY_COLORS_NATIVE, 4bit/pixel */
/* Map a r-g-b color to a color code */
private gx_color_index
display_map_rgb_color_device4(gx_device * dev, const gx_color_value cv[])
{
    return pc_4bit_map_rgb_color(dev, cv);
}

/* Map a color code to r-g-b. */
private int
display_map_color_rgb_device4(gx_device * dev, gx_color_index color,
		 gx_color_value prgb[3])
{
    pc_4bit_map_color_rgb(dev, color, prgb);
    return 0;
}

/* DISPLAY_COLORS_NATIVE, 8bit/pixel */
/* Map a r-g-b color to a color code */
private gx_color_index
display_map_rgb_color_device8(gx_device * dev, const gx_color_value cv[])
{
    /* palette of 96 colors */
    /* 0->63 = 00RRGGBB, 64->95 = 010YYYYY */
    gx_color_value r = cv[0];
    gx_color_value g = cv[1];
    gx_color_value b = cv[2];
    if ((r == g) && (g == b))
	return ((r >> (gx_color_value_bits - 5)) + 0x40);
    return ((r >> (gx_color_value_bits - 2)) << 4) +
	((g >> (gx_color_value_bits - 2)) << 2) +
	((b >> (gx_color_value_bits - 2)));
}

/* Map a color code to r-g-b. */
private int
display_map_color_rgb_device8(gx_device * dev, gx_color_index color,
		 gx_color_value prgb[3])
{
    gx_color_value one;
    /* palette of 96 colors */
    /* 0->63 = 00RRGGBB, 64->95 = 010YYYYY */
    if (color < 64) {
	one = (gx_color_value) (gx_max_color_value / 3);
	prgb[0] = (gx_color_value) (((color >> 4) & 3) * one);
	prgb[1] = (gx_color_value) (((color >> 2) & 3) * one);
	prgb[2] = (gx_color_value) (((color) & 3) * one);
    }
    else if (color < 96) {
	one = (gx_color_value) (gx_max_color_value / 31);
	prgb[0] = prgb[1] = prgb[2] = 
	    (gx_color_value) ((color & 0x1f) * one);
    }
    else {
	prgb[0] = prgb[1] = prgb[2] = 0;
    }
    return 0;
}


/* DISPLAY_COLORS_NATIVE, 16bit/pixel */
/* Map a r-g-b color to a color code */
private gx_color_index
display_map_rgb_color_device16(gx_device * dev, const gx_color_value cv[])
{
    gx_device_display *ddev = (gx_device_display *) dev;
    gx_color_value r = cv[0];
    gx_color_value g = cv[1];
    gx_color_value b = cv[2];
    if ((ddev->nFormat & DISPLAY_ENDIAN_MASK) == DISPLAY_BIGENDIAN) {
	if ((ddev->nFormat & DISPLAY_555_MASK) == DISPLAY_NATIVE_555)
	    /* byte0=0RRRRRGG byte1=GGGBBBBB */
	    return ((r >> (gx_color_value_bits - 5)) << 10) +
		((g >> (gx_color_value_bits - 5)) << 5) +
		(b >> (gx_color_value_bits - 5));
	else
	    /* byte0=RRRRRGGG byte1=GGGBBBBB */
	    return ((r >> (gx_color_value_bits - 5)) << 11) +
		((g >> (gx_color_value_bits - 6)) << 5) +
		(b >> (gx_color_value_bits - 5));
    }

    if ((ddev->nFormat & DISPLAY_555_MASK) == DISPLAY_NATIVE_555)
	/* byte0=GGGBBBBB byte1=0RRRRRGG */
	return ((r >> (gx_color_value_bits - 5)) << 2) +
	    (((g >> (gx_color_value_bits - 5)) & 0x7) << 13) +
	    (((g >> (gx_color_value_bits - 5)) & 0x18) >> 3) +
	    ((b >> (gx_color_value_bits - 5)) << 8);

    /* byte0=GGGBBBBB byte1=RRRRRGGG */
    return ((r >> (gx_color_value_bits - 5)) << 3) +
	(((g >> (gx_color_value_bits - 6)) & 0x7) << 13) +
	(((g >> (gx_color_value_bits - 6)) & 0x38) >> 3) +
	((b >> (gx_color_value_bits - 5)) << 8);
}



/* Map a color code to r-g-b. */
private int
display_map_color_rgb_device16(gx_device * dev, gx_color_index color,
		 gx_color_value prgb[3])
{
    gx_device_display *ddev = (gx_device_display *) dev;
    ushort value;

    if ((ddev->nFormat & DISPLAY_ENDIAN_MASK) == DISPLAY_BIGENDIAN) {
	if ((ddev->nFormat & DISPLAY_555_MASK) == DISPLAY_NATIVE_555) {
	    /* byte0=0RRRRRGG byte1=GGGBBBBB */
	    value = (ushort) (color >> 10);
	    prgb[0] = (gx_color_value)
		(((value << 11) + (value << 6) + (value << 1) + 
		(value >> 4)) >> (16 - gx_color_value_bits));
	    value = (ushort) ((color >> 5) & 0x1f);
	    prgb[1] = (gx_color_value)
		(((value << 11) + (value << 6) + (value << 1) + 
		(value >> 4)) >> (16 - gx_color_value_bits));
	    value = (ushort) (color & 0x1f);
	    prgb[2] = (gx_color_value)
		(((value << 11) + (value << 6) + (value << 1) + 
		(value >> 4)) >> (16 - gx_color_value_bits));
	}
	else {
	    /* byte0=RRRRRGGG byte1=GGGBBBBB */
	    value = (ushort) (color >> 11);
	    prgb[0] = ((value << 11) + (value << 6) + (value << 1) + 
		(value >> 4)) >> (16 - gx_color_value_bits);
	    value = (ushort) ((color >> 5) & 0x3f);
	    prgb[1] = (gx_color_value)
		((value << 10) + (value << 4) + (value >> 2))
		      >> (16 - gx_color_value_bits);
	    value = (ushort) (color & 0x1f);
	    prgb[2] = (gx_color_value)
		((value << 11) + (value << 6) + (value << 1) + 
		(value >> 4)) >> (16 - gx_color_value_bits);
	}
    }
    else {
	if ((ddev->nFormat & DISPLAY_555_MASK) == DISPLAY_NATIVE_555) {
	    /* byte0=GGGBBBBB byte1=0RRRRRGG */
	    value = (ushort) ((color >> 2) & 0x1f);
	    prgb[0] = (gx_color_value)
		((value << 11) + (value << 6) + (value << 1) + 
		(value >> 4)) >> (16 - gx_color_value_bits);
	    value = (ushort)
		(((color << 3) & 0x18) +  ((color >> 13) & 0x7));
	    prgb[1] = (gx_color_value)
		((value << 11) + (value << 6) + (value << 1) + 
		(value >> 4)) >> (16 - gx_color_value_bits);
	    value = (ushort) ((color >> 8) & 0x1f);
	    prgb[2] = (gx_color_value)
		((value << 11) + (value << 6) + (value << 1) + 
		(value >> 4)) >> (16 - gx_color_value_bits);
	}
	else {
	    /* byte0=GGGBBBBB byte1=RRRRRGGG */
	    value = (ushort) ((color >> 3) & 0x1f);
	    prgb[0] = (gx_color_value)
		(((value << 11) + (value << 6) + (value << 1) + 
		(value >> 4)) >> (16 - gx_color_value_bits));
	    value = (ushort)
		(((color << 3) & 0x38) +  ((color >> 13) & 0x7));
	    prgb[1] = (gx_color_value)
		(((value << 10) + (value << 4) + (value >> 2))
		      >> (16 - gx_color_value_bits));
	    value = (ushort) ((color >> 8) & 0x1f);
	    prgb[2] = (gx_color_value)
		(((value << 11) + (value << 6) + (value << 1) + 
		(value >> 4)) >> (16 - gx_color_value_bits));
	}
    }
    return 0;
}


/* Map a r-g-b color to a color code */
private gx_color_index
display_map_rgb_color_rgb(gx_device * dev, const gx_color_value cv[])
{
    gx_device_display *ddev = (gx_device_display *) dev;
    gx_color_value r = cv[0];
    gx_color_value g = cv[1];
    gx_color_value b = cv[2];
    int drop;
    gx_color_value red, green, blue;

    if ((ddev->nFormat & DISPLAY_ALPHA_MASK) == DISPLAY_ALPHA_NONE)
	drop = gx_color_value_bits - (dev->color_info.depth / 3); 
    else
	drop = gx_color_value_bits - (dev->color_info.depth / 4); 
    red  = r >> drop;
    green = g >> drop;
    blue = b >> drop;

    switch (ddev->nFormat & DISPLAY_ALPHA_MASK) {
	case DISPLAY_ALPHA_NONE:
	    if ((ddev->nFormat & DISPLAY_ENDIAN_MASK) == DISPLAY_BIGENDIAN) {
                gx_color_value rgb[3];
                rgb[0] = r; rgb[1] = g; rgb[2] = b;
		return gx_default_rgb_map_rgb_color(dev, rgb); /* RGB */
            }
	    else
		return (blue<<16) + (green<<8) + red;		/* BGR */
	case DISPLAY_ALPHA_FIRST:
	case DISPLAY_UNUSED_FIRST:
	    if ((ddev->nFormat & DISPLAY_ENDIAN_MASK) == DISPLAY_BIGENDIAN)
		return ((gx_color_index)red<<16) + (green<<8) + blue;		/* xRGB */
	    else
		return ((gx_color_index)blue<<16) + (green<<8) + red;		/* xBGR */
	case DISPLAY_ALPHA_LAST:
	case DISPLAY_UNUSED_LAST:
	    if ((ddev->nFormat & DISPLAY_ENDIAN_MASK) == DISPLAY_BIGENDIAN)
		return ((gx_color_index)red<<24) + (green<<16) + (blue<<8);	/* RGBx */
	    else
		return ((gx_color_index)blue<<24) + (green<<16) + (red<<8);	/* BGRx */
    }
    return 0;
}

/* Map a color code to r-g-b. */
private int
display_map_color_rgb_rgb(gx_device * dev, gx_color_index color,
		 gx_color_value prgb[3])
{
    gx_device_display *ddev = (gx_device_display *) dev;
    uint bits_per_color;
    uint color_mask;
    
    if ((ddev->nFormat & DISPLAY_ALPHA_MASK) == DISPLAY_ALPHA_NONE)
	bits_per_color = dev->color_info.depth / 3; 
    else
	bits_per_color = dev->color_info.depth / 4; 
    color_mask = (1 << bits_per_color) - 1;

    switch (ddev->nFormat & DISPLAY_ALPHA_MASK) {
	case DISPLAY_ALPHA_NONE:
	    if ((ddev->nFormat & DISPLAY_ENDIAN_MASK) == DISPLAY_BIGENDIAN)
		return gx_default_rgb_map_color_rgb(dev, color, prgb); /* RGB */
	    else {
		/* BGR */
		prgb[0] = (gx_color_value) (((color) & color_mask) * 
			(ulong) gx_max_color_value / color_mask);
		prgb[1] = (gx_color_value)
			(((color >> bits_per_color) & color_mask) * 
			(ulong) gx_max_color_value / color_mask);
		prgb[2] = (gx_color_value)
			(((color >> 2*bits_per_color) & color_mask) * 
			(ulong) gx_max_color_value / color_mask);
	    }
	    break;
	case DISPLAY_ALPHA_FIRST:
	case DISPLAY_UNUSED_FIRST:
	    if ((ddev->nFormat & DISPLAY_ENDIAN_MASK) == DISPLAY_BIGENDIAN) {
		/* xRGB */
		prgb[0] = (gx_color_value)
			(((color >> 2*bits_per_color) & color_mask) * 
			(ulong) gx_max_color_value / color_mask);
		prgb[1] = (gx_color_value) 
			(((color >> bits_per_color) & color_mask) * 
			(ulong) gx_max_color_value / color_mask);
		prgb[2] = (gx_color_value) (((color) & color_mask) * 
			(ulong) gx_max_color_value / color_mask);
	    }
	    else {
		/* xBGR */
		prgb[0] = (gx_color_value)
			(((color) & color_mask) * 
			(ulong) gx_max_color_value / color_mask);
		prgb[1] = (gx_color_value)
			(((color >> bits_per_color)   & color_mask) * 
			(ulong) gx_max_color_value / color_mask);
		prgb[2] = (gx_color_value)
			(((color >> 2*bits_per_color) & color_mask) * 
			(ulong) gx_max_color_value / color_mask);
	    }
	    break;
	case DISPLAY_ALPHA_LAST:
	case DISPLAY_UNUSED_LAST:
	    if ((ddev->nFormat & DISPLAY_ENDIAN_MASK) == DISPLAY_BIGENDIAN) {
		/* RGBx */
		prgb[0] = (gx_color_value)
			(((color >> 3*bits_per_color) & color_mask) * 
			(ulong) gx_max_color_value / color_mask);
		prgb[1] = (gx_color_value)
			(((color >> 2*bits_per_color) & color_mask) * 
			(ulong) gx_max_color_value / color_mask);
		prgb[2] = (gx_color_value)
			(((color >> bits_per_color)   & color_mask) * 
			(ulong) gx_max_color_value / color_mask);
	    }
	    else {
		/* BGRx */
		prgb[0] = (gx_color_value)
			(((color >> bits_per_color)   & color_mask) * 
			(ulong) gx_max_color_value / color_mask);
		prgb[1] = (gx_color_value)
			(((color >> 2*bits_per_color) & color_mask) * 
			(ulong) gx_max_color_value / color_mask);
		prgb[2] = (gx_color_value)
			(((color >> 3*bits_per_color) & color_mask) * 
			(ulong) gx_max_color_value / color_mask);
	    }
    }
    return 0;
}

/* Map a r-g-b color to a color code */
private gx_color_index
display_map_rgb_color_bgr24(gx_device * dev, const gx_color_value cv[])
{
    gx_color_value r = cv[0];
    gx_color_value g = cv[1];
    gx_color_value b = cv[2];
    return (gx_color_value_to_byte(b)<<16) +
           (gx_color_value_to_byte(g)<<8) +
            gx_color_value_to_byte(r);
}

/* Map a color code to r-g-b. */
private int
display_map_color_rgb_bgr24(gx_device * dev, gx_color_index color,
		 gx_color_value prgb[3])
{
    prgb[0] = gx_color_value_from_byte(color & 0xff);
    prgb[1] = gx_color_value_from_byte((color >> 8) & 0xff);
    prgb[2] = gx_color_value_from_byte((color >> 16) & 0xff);
    return 0;
}

/* Fill a rectangle */
private int
display_fill_rectangle(gx_device * dev, int x, int y, int w, int h,
		  gx_color_index color)
{
    gx_device_display *ddev = (gx_device_display *) dev;
    if (ddev->callback == NULL)
	return 0;
    dev_proc(ddev->mdev, fill_rectangle)((gx_device *)ddev->mdev, 
	x, y, w, h, color);
    if (ddev->callback->display_update)
	(*(ddev->callback->display_update))(ddev->pHandle, dev, x, y, w, h);
    return 0;
}

/* Copy a monochrome bitmap */
private int
display_copy_mono(gx_device * dev,
	     const byte * base, int sourcex, int raster, gx_bitmap_id id,
	     int x, int y, int w, int h,
	     gx_color_index zero, gx_color_index one)
{
    gx_device_display *ddev = (gx_device_display *) dev;
    if (ddev->callback == NULL)
	return 0;
    dev_proc(ddev->mdev, copy_mono)((gx_device *)ddev->mdev, 
	base, sourcex, raster, id, x, y, w, h, zero, one);
    if (ddev->callback->display_update)
	(*(ddev->callback->display_update))(ddev->pHandle, dev, x, y, w, h);
    return 0;
}

/* Copy a color pixel map  */
private int
display_copy_color(gx_device * dev,
	      const byte * base, int sourcex, int raster, gx_bitmap_id id,
	      int x, int y, int w, int h)
{
    gx_device_display *ddev = (gx_device_display *) dev;
    if (ddev->callback == NULL)
	return 0;
    dev_proc(ddev->mdev, copy_color)((gx_device *)ddev->mdev, 
	base, sourcex, raster, id, x, y, w, h);
    if (ddev->callback->display_update)
	(*(ddev->callback->display_update))(ddev->pHandle, dev, x, y, w, h);
    return 0;
}

private int
display_get_bits(gx_device * dev, int y, byte * str, byte ** actual_data)
{
    gx_device_display *ddev = (gx_device_display *) dev;
    if (ddev->callback == NULL)
	return 0;
    return dev_proc(ddev->mdev, get_bits)((gx_device *)ddev->mdev, 
	y, str, actual_data);
}

private int
display_get_params(gx_device * dev, gs_param_list * plist)
{
    gx_device_display *ddev = (gx_device_display *) dev;
    int code = gx_default_get_params(dev, plist);
    (void)(code < 0 ||
	(code = param_write_long(plist, 
	    "DisplayHandle", (long *)(&ddev->pHandle))) < 0 ||
	(code = param_write_int(plist, 
	    "DisplayFormat", &ddev->nFormat)) < 0 );
    return code;
}

/* Put parameters. */
/* The parameters "DisplayHandle" and "DisplayFormat"
 * can be changed when the device is closed, but not when open.
 * The device width and height can be changed when open.
 */
private int
display_put_params(gx_device * dev, gs_param_list * plist)
{
    gx_device_display *ddev = (gx_device_display *) dev;
    int ecode = 0, code;
    bool is_open = dev->is_open;

    int old_width = dev->width;
    int old_height = dev->height;
    int old_format = ddev->nFormat;
    void *old_handle = ddev->pHandle;

    int format;
    void *handle;

    /* Handle extra parameters */

    switch (code = param_read_int(plist, "DisplayFormat", &format)) {
	case 0:
	    if (dev->is_open) {
 		if (ddev->nFormat != format)
		    ecode = gs_error_rangecheck;
		else
		    break;
	    }
	    else {
		code = display_set_color_format(ddev, format);
		if (code < 0)
		    ecode = code;
		else
		    break;
	    }
	    goto cfe;
	default:
	    ecode = code;
	  cfe:param_signal_error(plist, "DisplayFormat", ecode);
	case 1:
	    break;
    }

    switch (code = param_read_long(plist, "DisplayHandle", (long *)(&handle))) {
	case 0:
	    if (dev->is_open) {
		if (ddev->pHandle != handle)
		    ecode = gs_error_rangecheck;
		else
		    break;
	    }
	    else
		ddev->pHandle = handle;
		break;
	    goto hdle;
	default:
	    ecode = code;
	  hdle:param_signal_error(plist, "DisplayHandle", ecode);
	case 1:
	    break;
    }

    if (ecode >= 0) {
	/* Prevent gx_default_put_params from closing the device. */
	dev->is_open = false;
	ecode = gx_default_put_params(dev, plist);
	dev->is_open = is_open;
    }
    if (ecode < 0) {
	if (format != old_format)
	    display_set_color_format(ddev, old_format);
	ddev->pHandle = old_handle;
	dev->width = old_width;
	dev->height = old_height;
	return ecode;
    }


    if ( is_open && ddev->callback &&
	((old_width != dev->width) || (old_height != dev->height)) ) {
	/* We can resize this device while it is open, but we cannot
	 * change the color format or handle.
	 */
	/* Tell caller we are about to change the device parameters */
	if ((*ddev->callback->display_presize)(ddev->pHandle, dev,
	    dev->width, dev->height, gdev_mem_raster(dev),
	    ddev->nFormat) < 0) {
	    /* caller won't let us change the size */
	    /* restore parameters then return an error */
	    display_set_color_format(ddev, old_format);
	    ddev->nFormat = old_format;
	    ddev->pHandle = old_handle;
	    dev->width = old_width;
	    dev->height = old_height;
	    return_error(gs_error_rangecheck);
	}

	display_free_bitmap(ddev);

	code = display_alloc_bitmap(ddev, dev);
	if (code < 0) {
	    /* No bitmap, so tell the caller it is zero size */
	    (*ddev->callback->display_size)(ddev->pHandle, dev, 
	        0, 0, 0, ddev->nFormat, NULL);
	    return_error(code);
        }
    
	/* tell caller about the new size */
	if ((*ddev->callback->display_size)(ddev->pHandle, dev, 
	    dev->width, dev->height, gdev_mem_raster(dev),
	    ddev->nFormat, ddev->mdev->base) < 0)
	    return_error(gs_error_rangecheck);
    }

    return 0;
}

/* Clean up the instance after making a copy. */
int
display_finish_copydevice(gx_device *dev, const gx_device *from_dev)
{
    gx_device_display *ddev = (gx_device_display *) dev;

    /* Mark the new instance as closed. */
    ddev->is_open = false;

    /* Clear pointers */
    ddev->mdev = NULL;
    ddev->pBitmap = NULL;
    ddev->ulBitmapSize = 0;

    return 0;
}

/* ------ Internal routines ------ */

/* Make sure we have been given a valid structure */
/* Return 0 on success, gs_error_rangecheck on failure */
private int display_check_structure(gx_device_display *ddev)
{
    if (ddev->callback == 0)
	return_error(gs_error_rangecheck);

    if (ddev->callback->size != sizeof(display_callback))
	return_error(gs_error_rangecheck);

    if (ddev->callback->version_major != DISPLAY_VERSION_MAJOR)
	return_error(gs_error_rangecheck);

    /* complain if caller asks for newer features */
    if (ddev->callback->version_minor > DISPLAY_VERSION_MINOR)
	return_error(gs_error_rangecheck);

    if ((ddev->callback->display_open == NULL) ||
	(ddev->callback->display_close == NULL) ||
	(ddev->callback->display_presize == NULL) ||
	(ddev->callback->display_size == NULL) ||
	(ddev->callback->display_sync == NULL) ||
	(ddev->callback->display_page == NULL))
	return_error(gs_error_rangecheck);

    /* don't test display_update, display_memalloc or display_memfree
     * since these may be NULL if not provided
     */

    return 0;
}

private void
display_free_bitmap(gx_device_display * ddev)
{
    if (ddev->callback == NULL)
	return;
    if (ddev->pBitmap) {
	if (ddev->callback->display_memalloc 
	    && ddev->callback->display_memfree
	    && ddev->pBitmap) {
	    (*ddev->callback->display_memfree)(ddev->pHandle, ddev, 
		ddev->pBitmap);
	}
	else {
	    gs_free_object(&gs_memory_default,
		ddev->pBitmap, "display_free_bitmap");
	}
	ddev->pBitmap = NULL;
	if (ddev->mdev)
	    ddev->mdev->base = NULL;
    }
    if (ddev->mdev) {
	dev_proc(ddev->mdev, close_device)((gx_device *)ddev->mdev);
        gx_device_retain((gx_device *)(ddev->mdev), false);
	ddev->mdev = NULL;
    }
}

/* Allocate the backing bitmap. */
private int
display_alloc_bitmap(gx_device_display * ddev, gx_device * param_dev)
{
    int ccode;
    const gx_device_memory *mdproto;
    if (ddev->callback == NULL)
	return 0;

    /* free old bitmap (if any) */
    display_free_bitmap(ddev);

    /* allocate a memory device for rendering */
    mdproto = gdev_mem_device_for_bits(ddev->color_info.depth);
    if (mdproto == 0)
	return_error(gs_error_rangecheck);
    
    ddev->mdev = gs_alloc_struct(gs_memory_stable(ddev->memory), 
	    gx_device_memory, &st_device_memory, "display_memory_device");
    if (ddev->mdev == 0)
        return_error(gs_error_VMerror);

    gs_make_mem_device(ddev->mdev, mdproto, gs_memory_stable(ddev->memory), 
	0, (gx_device *) NULL);
    check_device_separable((gx_device *)(ddev->mdev));
    gx_device_fill_in_procs((gx_device *)(ddev->mdev));
    /* Mark the memory device as retained.  When the bitmap is closed,
     * we will clear this and the memory device will be then be freed.
     */
    gx_device_retain((gx_device *)(ddev->mdev), true);
    
    ddev->mdev->width = param_dev->width;
    ddev->mdev->height = param_dev->height;
    /* Tell the memory device to allocate the line pointers separately
     * so we can place the bitmap in special memory.
     */
    ddev->mdev->line_pointer_memory = ddev->mdev->memory;
    ddev->ulBitmapSize = gdev_mem_bits_size(ddev->mdev,
	ddev->mdev->width, ddev->mdev->height);

    /* allocate bitmap using an allocator not subject to GC */
    if (ddev->callback->display_memalloc 
	&& ddev->callback->display_memfree) {
        ddev->pBitmap = (*ddev->callback->display_memalloc)(ddev->pHandle, 
	    ddev, ddev->ulBitmapSize);
    }
    else {
	ddev->pBitmap = gs_alloc_byte_array_immovable(&gs_memory_default,
		(uint)ddev->ulBitmapSize, 1, "display_alloc_bitmap");
    }

    if (ddev->pBitmap == NULL) {
	ddev->mdev->width = 0;
	ddev->mdev->height = 0;
	return_error(gs_error_VMerror);
    }

    ddev->mdev->base = (byte *) ddev->pBitmap;
    ddev->mdev->foreign_bits = true;

    ccode = dev_proc(ddev->mdev, open_device)((gx_device *)ddev->mdev);
    if (ccode < 0)
	display_free_bitmap(ddev);

    /* erase bitmap - before display gets redrawn */
    if (ccode == 0) {
	int i;
        gx_color_value cv[GX_DEVICE_COLOR_MAX_COMPONENTS];
	for (i=0; i<GX_DEVICE_COLOR_MAX_COMPONENTS; i++)
	    cv[i] = (ddev->color_info.polarity == GX_CINFO_POLARITY_ADDITIVE)
		? gx_max_color_value : 0;
	dev_proc(ddev, fill_rectangle)((gx_device *)ddev,
                 0, 0, ddev->width, ddev->height,
                 ddev->procs.encode_color((gx_device *)ddev, cv));
    }

    return ccode;
}

/*
 * This is a utility routine to build the display device's color_info
 * structure (except for the anti alias info).
 */
private void
set_color_info(gx_device_color_info * pdci, int nc, int depth, int maxgray, int maxcolor)
{
    pdci->num_components = pdci->max_components = nc;
    pdci->depth = depth;
    pdci->gray_index = 0;
    pdci->max_gray = maxgray;
    pdci->max_color = maxcolor;
    pdci->dither_grays = maxgray + 1;
    pdci->dither_colors = maxcolor + 1;
    pdci->separable_and_linear = GX_CINFO_UNKNOWN_SEP_LIN;
    switch (nc) {
	case 1:
	    pdci->polarity = GX_CINFO_POLARITY_ADDITIVE;
	    pdci->cm_name = "DeviceGray";
	    break;
	case 3:
	    pdci->polarity = GX_CINFO_POLARITY_ADDITIVE;
	    pdci->cm_name = "DeviceRGB";
	    break;
	case 4:
	    pdci->polarity = GX_CINFO_POLARITY_SUBTRACTIVE;
	    pdci->cm_name = "DeviceCMYK";
	    break;
	default:
	    break;
    }
}

/*
 * This is an utility routine to set up the color procs for the display
 * device.  The display device can change its setup.
 */
private void
set_color_procs(gx_device * pdev, 
	dev_t_proc_encode_color((*encode_color), gx_device),
	dev_t_proc_decode_color((*decode_color), gx_device),
	dev_t_proc_get_color_mapping_procs((*get_color_mapping_procs), gx_device),
	dev_t_proc_get_color_comp_index((*get_color_comp_index), gx_device))
{
#if 0				/* These procs are no longer used */
    pdev->procs.map_rgb_color = encode_color;
    pdev->procs.map_color_rgb = decode_color;
#endif
    pdev->procs.get_color_mapping_procs = get_color_mapping_procs;
    pdev->procs.get_color_comp_index = get_color_comp_index;
    pdev->procs.encode_color = encode_color;
    pdev->procs.decode_color = decode_color;
}

/*
 * This is an utility routine to set up the color procs for the display
 * device.  This routine is used when the display device is Gray.
 */
private void
set_gray_color_procs(gx_device * pdev, 
	dev_t_proc_encode_color((*encode_color), gx_device),
	dev_t_proc_decode_color((*decode_color), gx_device))
{
    set_color_procs(pdev, encode_color, decode_color,
	gx_default_DevGray_get_color_mapping_procs,
	gx_default_DevGray_get_color_comp_index);
}

/*
 * This is an utility routine to set up the color procs for the display
 * device.  This routine is used when the display device is RGB.
 */
private void
set_rgb_color_procs(gx_device * pdev, 
	dev_t_proc_encode_color((*encode_color), gx_device),
	dev_t_proc_decode_color((*decode_color), gx_device))
{
    set_color_procs(pdev, encode_color, decode_color,
	gx_default_DevRGB_get_color_mapping_procs,
	gx_default_DevRGB_get_color_comp_index);
}

/*
 * This is an utility routine to set up the color procs for the display
 * device.  This routine is used when the display device is CMYK.
 */
private void
set_cmyk_color_procs(gx_device * pdev, 
	dev_t_proc_encode_color((*encode_color), gx_device),
	dev_t_proc_decode_color((*decode_color), gx_device))
{
    set_color_procs(pdev, encode_color, decode_color,
	gx_default_DevCMYK_get_color_mapping_procs,
	gx_default_DevCMYK_get_color_comp_index);
}

/* Set the color_info and mapping functions for this instance of the device */
private int
display_set_color_format(gx_device_display *ddev, int nFormat)
{
    gx_device * pdev = (gx_device *) ddev;
    gx_device_color_info dci = ddev->color_info;
    int bpc;	/* bits per component */
    int bpp;	/* bits per pixel */
    int maxvalue;

    switch (nFormat & DISPLAY_DEPTH_MASK) {
	case DISPLAY_DEPTH_1:
	    bpc = 1;
	    break;
	case DISPLAY_DEPTH_2:
	    bpc = 2;
	    break;
	case DISPLAY_DEPTH_4:
	    bpc = 4;
	    break;
	case DISPLAY_DEPTH_8:
	    bpc = 8;
	    break;
	case DISPLAY_DEPTH_12:
	    bpc = 12;
	    break;
	case DISPLAY_DEPTH_16:
	    bpc = 16;
	    break;
	default:
	    return_error(gs_error_rangecheck);
    }
    maxvalue = (1 << bpc) - 1;

    switch (nFormat & DISPLAY_COLORS_MASK) {
	case DISPLAY_COLORS_NATIVE:
	    switch (nFormat & DISPLAY_DEPTH_MASK) {
		case DISPLAY_DEPTH_1: 
		    /* 1bit/pixel, black is 1, white is 0 */
	    	    set_color_info(&dci, 1, 1, 1, 0);
		    set_gray_color_procs(pdev, gx_b_w_gray_encode,
		    				gx_default_b_w_map_color_rgb);
		    break;
		case DISPLAY_DEPTH_4:
		    /* 4bit/pixel VGA color */
	    	    set_color_info(&dci, 3, 4, 3, 2);
		    set_rgb_color_procs(pdev, display_map_rgb_color_device4,
		    				display_map_color_rgb_device4);
		    break;
		case DISPLAY_DEPTH_8:
		    /* 8bit/pixel 96 color palette */
	    	    set_color_info(&dci, 3, 8, 31, 3);
		    set_rgb_color_procs(pdev, display_map_rgb_color_device8,
		    				display_map_color_rgb_device8);
		    break;
		case DISPLAY_DEPTH_16:
		    /* Windows 16-bit display */
		    /* Is maxgray = maxcolor = 63 correct? */
	    	    set_color_info(&dci, 3, 16, 63, 63);
		    set_rgb_color_procs(pdev, display_map_rgb_color_device16,
		    				display_map_color_rgb_device16);
		    break;
		default:
		    return_error(gs_error_rangecheck);
	    }
	    break;
	case DISPLAY_COLORS_GRAY:
	    set_color_info(&dci, 1, bpc, maxvalue, 0);
	    if (bpc == 1)
	    	set_gray_color_procs(pdev, gx_default_gray_encode,
						gx_default_w_b_map_color_rgb);
	    else
	    	set_gray_color_procs(pdev, gx_default_gray_encode,
						gx_default_gray_map_color_rgb);
	    break;
	case DISPLAY_COLORS_RGB:
	    if ((nFormat & DISPLAY_ALPHA_MASK) == DISPLAY_ALPHA_NONE)
		bpp = bpc * 3;
	    else
		bpp = bpc * 4; 
	    set_color_info(&dci, 3, bpp, maxvalue, maxvalue);
	    if (((nFormat & DISPLAY_DEPTH_MASK) == DISPLAY_DEPTH_8) &&
	 	((nFormat & DISPLAY_ALPHA_MASK) == DISPLAY_ALPHA_NONE)) {
		if ((nFormat & DISPLAY_ENDIAN_MASK) == DISPLAY_BIGENDIAN)
	    	    set_rgb_color_procs(pdev, gx_default_rgb_map_rgb_color,
		    				gx_default_rgb_map_color_rgb);
		else
	    	    set_rgb_color_procs(pdev, display_map_rgb_color_bgr24,
		    				display_map_color_rgb_bgr24);
	    }
	    else {
		/* slower flexible functions for alpha/unused component */
	    	set_rgb_color_procs(pdev, display_map_rgb_color_rgb,
						display_map_color_rgb_rgb);
	    }
	    break;
	case DISPLAY_COLORS_CMYK:
	    bpp = bpc * 4;
	    set_color_info(&dci, 4, bpp, maxvalue, maxvalue);
	    if ((nFormat & DISPLAY_ALPHA_MASK) != DISPLAY_ALPHA_NONE)
		return_error(gs_error_rangecheck);
	    if ((nFormat & DISPLAY_ENDIAN_MASK) != DISPLAY_BIGENDIAN)
		return_error(gs_error_rangecheck);

	    if ((nFormat & DISPLAY_DEPTH_MASK) == DISPLAY_DEPTH_1)
	    	set_cmyk_color_procs(pdev, cmyk_1bit_map_cmyk_color,
						cmyk_1bit_map_color_cmyk);
	    else if ((nFormat & DISPLAY_DEPTH_MASK) == DISPLAY_DEPTH_8)
	    	set_cmyk_color_procs(pdev, cmyk_8bit_map_cmyk_color,
						cmyk_8bit_map_color_cmyk);
	    else
		return_error(gs_error_rangecheck);
	    break;
	default:
	    return_error(gs_error_rangecheck);
    }

    /* restore old anti_alias info */
    dci.anti_alias = ddev->color_info.anti_alias;
    ddev->color_info = dci;
    /* Set the mask bits, etc. even though we are setting linear: unkown */
    set_linear_color_bits_mask_shift(pdev);
    ddev->nFormat = nFormat;

    return 0;
}

/* ------ Begin Test Code ------ */

/*********************************************************************
typedef struct test_mode_s test_mode;
struct test_mode_s {
    char *name;
    unsigned int format;
};

test_mode test_modes[] = {
    {"1bit/pixel native, black is 1, Windows",
     DISPLAY_COLORS_NATIVE | DISPLAY_ALPHA_NONE | DISPLAY_DEPTH_1 | 
     DISPLAY_LITTLEENDIAN | DISPLAY_BOTTOMFIRST},
    {"4bit/pixel native, Windows VGA 16 color palette",
     DISPLAY_COLORS_NATIVE | DISPLAY_ALPHA_NONE | DISPLAY_DEPTH_4 | 
     DISPLAY_LITTLEENDIAN | DISPLAY_BOTTOMFIRST},
    {"8bit/pixel native, Windows SVGA 96 color palette",
     DISPLAY_COLORS_NATIVE | DISPLAY_ALPHA_NONE | DISPLAY_DEPTH_8 | 
     DISPLAY_LITTLEENDIAN | DISPLAY_BOTTOMFIRST},
    {"16bit/pixel native, Windows BGR555",
     DISPLAY_COLORS_NATIVE | DISPLAY_ALPHA_NONE | DISPLAY_DEPTH_16 | 
     DISPLAY_LITTLEENDIAN | DISPLAY_BOTTOMFIRST | DISPLAY_NATIVE_555},
    {"16bit/pixel native, Windows BGR565",
     DISPLAY_COLORS_NATIVE | DISPLAY_ALPHA_NONE | DISPLAY_DEPTH_16 | 
     DISPLAY_LITTLEENDIAN | DISPLAY_BOTTOMFIRST | DISPLAY_NATIVE_565},
    {"1bit/pixel gray, black is 0, topfirst",
     DISPLAY_COLORS_GRAY | DISPLAY_ALPHA_NONE | DISPLAY_DEPTH_1 | 
     DISPLAY_BIGENDIAN | DISPLAY_TOPFIRST},
    {"4bit/pixel gray, bottom first",
     DISPLAY_COLORS_GRAY | DISPLAY_ALPHA_NONE | DISPLAY_DEPTH_4 | 
     DISPLAY_BIGENDIAN | DISPLAY_BOTTOMFIRST},
    {"8bit/pixel gray, bottom first",
     DISPLAY_COLORS_GRAY | DISPLAY_ALPHA_NONE | DISPLAY_DEPTH_8 | 
     DISPLAY_BIGENDIAN | DISPLAY_BOTTOMFIRST},
    {"24bit/pixel color, bottom first, Windows BGR24",
     DISPLAY_COLORS_RGB | DISPLAY_ALPHA_NONE | DISPLAY_DEPTH_8 | 
     DISPLAY_LITTLEENDIAN | DISPLAY_BOTTOMFIRST},
    {"24bit/pixel color, bottom first, RGB24",
     DISPLAY_COLORS_RGB | DISPLAY_ALPHA_NONE | DISPLAY_DEPTH_8 | 
     DISPLAY_BIGENDIAN | DISPLAY_BOTTOMFIRST},
    {"24bit/pixel color, top first, GdkRgb RGB24",
     DISPLAY_COLORS_RGB | DISPLAY_ALPHA_NONE | DISPLAY_DEPTH_8 | 
     DISPLAY_BIGENDIAN | DISPLAY_TOPFIRST},
    {"32bit/pixel color, top first, Macintosh xRGB",
     DISPLAY_COLORS_RGB | DISPLAY_UNUSED_FIRST | DISPLAY_DEPTH_8 | 
     DISPLAY_BIGENDIAN | DISPLAY_TOPFIRST},
    {"32bit/pixel color, bottom first, xBGR",
     DISPLAY_COLORS_RGB | DISPLAY_UNUSED_FIRST | DISPLAY_DEPTH_8 | 
     DISPLAY_LITTLEENDIAN | DISPLAY_BOTTOMFIRST},
    {"32bit/pixel color, bottom first, Windows BGRx",
     DISPLAY_COLORS_RGB | DISPLAY_UNUSED_LAST | DISPLAY_DEPTH_8 | 
     DISPLAY_LITTLEENDIAN | DISPLAY_BOTTOMFIRST},
    {"32bit/pixel color, bottom first, RGBx",
     DISPLAY_COLORS_RGB | DISPLAY_UNUSED_LAST | DISPLAY_DEPTH_8 | 
     DISPLAY_BIGENDIAN | DISPLAY_BOTTOMFIRST},
    {"32bit/pixel CMYK, bottom first",
     DISPLAY_COLORS_CMYK | DISPLAY_ALPHA_NONE | DISPLAY_DEPTH_8 | 
     DISPLAY_BIGENDIAN | DISPLAY_BOTTOMFIRST}
};

void
test(int index)
{
    char buf[1024];
    sprintf(buf, "gs -dDisplayFormat=16#%x examples/colorcir.ps -c quit", test_modes[index].format);
    system(buf);
}

int main(int argc, char *argv[]) 
{
    int i;
    int dotest = 0;
    if (argc >=2) {
	if (strcmp(argv[1], "-t") == 0)
	    dotest = 1;
	else {
	    fprintf(stdout, "To show modes: disp\nTo run test: disp -t\n");
	    return 1;
	}
    }
    for (i=0; i < sizeof(test_modes)/sizeof(test_mode); i++) {
	fprintf(stdout, "16#%x or %d: %s\n", test_modes[i].format,
		test_modes[i].format, test_modes[i].name);
	if (dotest)
	    test(i);
    }
    return 0;
}
*********************************************************************/

/* ------ End Test Code ------ */
