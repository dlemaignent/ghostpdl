/* Copyright (C) 2002 Aladdin Enterprises.  All rights reserved.
  
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

/* $Id$ */
/* Text processing for pdfwrite. */
#include "math_.h"
#include "string_.h"
#include "gx.h"
#include "gserrors.h"
#include "gxfcache.h"		/* for orig_fonts list */
#include "gxfont.h"
#include "gxfont0.h"
#include "gxfcid.h"
#include "gxfcopy.h"
#include "gxfcmap.h"
#include "gxpath.h"		/* for getting current point */
#include "gxchar.h"		
#include "gdevpdfx.h"
#include "gdevpdfg.h"
#include "gdevpdtx.h"
#include "gdevpdtd.h"
#include "gdevpdtf.h"
#include "gdevpdts.h"
#include "gdevpdtt.h"
#include "gdevpdti.h"

/* ================ Text enumerator ================ */

/* GC descriptor */
private_st_pdf_text_enum();

/* Define the auxiliary procedures for text processing. */
private int
pdf_text_resync(gs_text_enum_t *pte, const gs_text_enum_t *pfrom)
{
    pdf_text_enum_t *const penum = (pdf_text_enum_t *)pte;

    if ((pte->text.operation ^ pfrom->text.operation) & ~TEXT_FROM_ANY)
	return_error(gs_error_rangecheck);
    if (penum->pte_default) {
	int code = gs_text_resync(penum->pte_default, pfrom);

	if (code < 0)
	    return code;
    }
    pte->text = pfrom->text;
    gs_text_enum_copy_dynamic(pte, pfrom, false);
    return 0;
}
private bool
pdf_text_is_width_only(const gs_text_enum_t *pte)
{
    const pdf_text_enum_t *const penum = (const pdf_text_enum_t *)pte;

    if (penum->pte_default)
	return gs_text_is_width_only(penum->pte_default);
    return false;
}
private int
pdf_text_current_width(const gs_text_enum_t *pte, gs_point *pwidth)
{
    const pdf_text_enum_t *const penum = (const pdf_text_enum_t *)pte;

    if (penum->pte_default)
	return gs_text_current_width(penum->pte_default, pwidth);
    return_error(gs_error_rangecheck); /* can't happen */
}
private int
pdf_text_set_cache(gs_text_enum_t *pte, const double *pw,
		   gs_text_cache_control_t control)
{
    pdf_text_enum_t *const penum = (pdf_text_enum_t *)pte;
    gx_device_pdf *pdev = (gx_device_pdf *)pte->dev;

    switch (control) {
    case TEXT_SET_CHAR_WIDTH:
    case TEXT_SET_CACHE_DEVICE:
	pdev->char_width.x = pw[0];
	pdev->char_width.y = pw[1];
	break;
    case TEXT_SET_CACHE_DEVICE2:
	/*
	 * pdev->char_width is used with synthesized Type 3 fonts only.
	 * Since they are simple fonts, we only need the horisontal
	 * width for Widths array. Therefore we don't check 
	 * gs_rootfont(pgs)->WMode and don't use pw[6:7].
	 */
	pdev->char_width.x = pw[0];
	pdev->char_width.y = pw[1];
	break;
    default:
	return_error(gs_error_rangecheck);
    }
    if (penum->current_font->FontType == ft_user_defined && 
	    !(penum->pte_default->text.operation & TEXT_DO_CHARWIDTH)) {
	int code;
	gs_font *font = penum->orig_font;
	gs_char ch;
	gs_glyph glyph;
	gs_const_string gnstr;
	gs_matrix m;

	if (penum->text.operation & TEXT_FROM_SINGLE_GLYPH) {
	    byte buf[1];
	    int char_code_length;

	    glyph = pte->text.data.d_glyph;
	    code = pdf_encode_glyph((gs_font_base *)font, glyph, 
			buf, sizeof(buf), &char_code_length);
	    if (code < 0) {
		/* Must not happen, becuse pdf_encode_glyph was passed in process_plain_text.*/
		ch = GS_NO_CHAR;
	    } else if (char_code_length != 1) {
		/* Must not happen with type 3 fonts.*/
		ch = GS_NO_CHAR;
	    } else
		ch = buf[0];
	} else {
	    ch = penum->text.data.bytes[penum->index];
	    glyph = font->procs.encode_char(font, ch, GLYPH_SPACE_NAME);
	    /*
	     * If glyph == GS_NO_GLYPH, we should replace it with 
	     * a notdef glyph, but we don't know how to do with Type 3 fonts.
	     */
	}
	if (glyph != GS_NO_GLYPH && ch != GS_NO_CHAR) {
	    gs_fixed_rect clip_box;
	    gs_show_enum *const penum_s = (gs_show_enum *)penum->pte_default;

	    code = font->procs.glyph_name(font, glyph, &gnstr);
	    if (code < 0)
		return_error(gs_error_unregistered); /* Must not happen. */
	    gs_make_identity(&m);
	    gs_matrix_fixed_from_matrix(&penum->pis->ctm, &m);
	    if (control != TEXT_SET_CHAR_WIDTH) {
		clip_box.p.x = float2fixed(pw[2]);
		clip_box.p.y = float2fixed(pw[3]);
		clip_box.q.x = float2fixed(pw[4]);
		clip_box.q.y = float2fixed(pw[5]);
	    } else {
		gs_font_base *bfont = (gs_font_base *)font;

		clip_box.p.x = float2fixed(bfont->FontBBox.p.x);
		clip_box.p.y = float2fixed(bfont->FontBBox.p.y);
		clip_box.q.x = float2fixed(bfont->FontBBox.q.x);
		clip_box.q.y = float2fixed(bfont->FontBBox.q.y);
	    }
	    code = gx_clip_to_rectangle(penum_s->pgs, &clip_box);
	    if (code < 0)
		return code;
	    penum->charproc_accum = true;
	    code = pdf_install_charproc_accum(pdev, pte->orig_font, 
			pw, control, ch, &gnstr);
	    if (code < 0)
		return code;
	    penum->charproc_accum = true;
	    return code;
	}
    }
    if (penum->pte_default) {
	if (penum->pte_default->text.operation & TEXT_DO_CHARWIDTH /* See process_cmap_text.*/)
	    return gs_text_set_cache(penum->pte_default, pw, TEXT_SET_CHAR_WIDTH);
	else
	    return gs_text_set_cache(penum->pte_default, pw, control);
    }
    return_error(gs_error_unregistered); /* can't happen */
}
private int
pdf_text_retry(gs_text_enum_t *pte)
{
    pdf_text_enum_t *const penum = (pdf_text_enum_t *)pte;

    if (penum->pte_default)
	return gs_text_retry(penum->pte_default);
    return_error(gs_error_rangecheck); /* can't happen */
}
private void
pdf_text_release(gs_text_enum_t *pte, client_name_t cname)
{
    pdf_text_enum_t *const penum = (pdf_text_enum_t *)pte;

    if (penum->pte_default) {
	gs_text_release(penum->pte_default, cname);
	penum->pte_default = 0;
    }
    gx_default_text_release(pte, cname);
}

/* Begin processing text. */
private text_enum_proc_process(pdf_text_process);
private const gs_text_enum_procs_t pdf_text_procs = {
    pdf_text_resync, pdf_text_process,
    pdf_text_is_width_only, pdf_text_current_width,
    pdf_text_set_cache, pdf_text_retry,
    pdf_text_release
};

int
gdev_pdf_text_begin(gx_device * dev, gs_imager_state * pis,
		    const gs_text_params_t *text, gs_font * font,
		    gx_path * path0, const gx_device_color * pdcolor,
		    const gx_clip_path * pcpath,
		    gs_memory_t * mem, gs_text_enum_t ** ppte)
{
    gx_device_pdf *const pdev = (gx_device_pdf *)dev;
    gx_path *path = path0;
    pdf_text_enum_t *penum;
    gs_fixed_point cpt;
    bool new_clip;
    int code;

    /* Track the dominant text rotation. */
    {
	gs_matrix tmat;
	int i;

	gs_matrix_multiply(&font->FontMatrix, &ctm_only(pis), &tmat);
	if (is_xxyy(&tmat))
	    i = (tmat.xx >= 0 ? 0 : 2);
	else if (is_xyyx(&tmat))
	    i = (tmat.xy >= 0 ? 1 : 3);
	else
	    i = 4;
	pdf_current_page(pdev)->text_rotation.counts[i] += text->size;
    }

    if (font->FontType == ft_user_defined &&
	(text->operation & TEXT_DO_NONE) && (text->operation & TEXT_RETURN_WIDTH)) {
	/* This is stringwidth, see gx_default_text_begin.
	 * We need to prevent writing characters to PS cache,
	 * otherwise the font converts to bitmaps.
	 * So pass through even with stringwidth.
	 */
	code = gx_hld_stringwidth_begin(pis, &path);
	if (code < 0)
	    return code;
    } else if ((!(text->operation & TEXT_DO_DRAW) && pis->text_rendering_mode != 3) 
		|| path == 0 || gx_path_current_point(path, &cpt) < 0
	    )
	return gx_default_text_begin(dev, pis, text, font, path, pdcolor,
					 pcpath, mem, ppte);

    if (!(text->operation & TEXT_DO_NONE)) {
	new_clip = pdf_must_put_clip_path(pdev, pcpath);
	if (new_clip)
	    code = pdf_unclip(pdev);
	else if (pdev->context == PDF_IN_NONE)
	    code = pdf_open_page(pdev, PDF_IN_STREAM);
	else
	    code = 0;
	if (code < 0)
	    return code;
	code = pdf_prepare_fill(pdev, pis);
	if (code < 0)
	    return code;
    }
    if (text->operation & TEXT_DO_DRAW) {
	/*
	 * Set the clipping path and drawing color.  We set both the fill
	 * and stroke color, because we don't know whether the fonts will be
	 * filled or stroked, and we can't set a color while we are in text
	 * mode.  (This is a consequence of the implementation, not a
	 * limitation of PDF.)
	 */

	if (new_clip) {
	    code = pdf_put_clip_path(pdev, pcpath);
	    if (code < 0)
		return code;
	}

	if ((code =
	     pdf_set_drawing_color(pdev, pdcolor, &pdev->saved_stroke_color,
				   &psdf_set_stroke_color_commands)) < 0 ||
	    (code =
	     pdf_set_drawing_color(pdev, pdcolor, &pdev->saved_fill_color,
				   &psdf_set_fill_color_commands)) < 0
	    )
	    return code;
    }

    /* Allocate and initialize the enumerator. */

    rc_alloc_struct_1(penum, pdf_text_enum_t, &st_pdf_text_enum, mem,
		      return_error(gs_error_VMerror), "gdev_pdf_text_begin");
    penum->rc.free = rc_free_text_enum;
    penum->pte_default = 0; 
    penum->charproc_accum = false;
    penum->returned.total_width.x = penum->returned.total_width.y = 0;
    code = gs_text_enum_init((gs_text_enum_t *)penum, &pdf_text_procs,
			     dev, pis, text, font, path, pdcolor, pcpath, mem);
    if (code < 0) {
	gs_free_object(mem, penum, "gdev_pdf_text_begin");
	return code;
    }

    *ppte = (gs_text_enum_t *)penum;

    return 0;
}

/* ================ Font cache element ================ */

/* GC descriptor */
private_st_pdf_font_cache_elem();

/*
 * Compute id for a font cache element.
 */
private ulong 
pdf_font_cache_elem_id(gs_font *font)
{
#if 0
    /*
     *	For compatibility with Ghostscript rasterizer's
     *	cache logic we use UniqueID to identify fonts.
     *  Note that with buggy documents, which don't
     *	undefine UniqueID redefining a font,
     *	Ghostscript PS interpreter can occasionaly
     *	replace cache elements on insufficient cache size,
     *	taking glyphs from random fonts with random metrics,
     *	therefore the compatibility isn't complete.
     */
    /*
     *	This branch is incompatible with pdf_notify_remove_font.
     */
    if (font->FontType == ft_composite || font->PaintType != 0 ||
	!uid_is_valid(&(((gs_font_base *)font)->UID)))
	return font->id;
    else
	return ((gs_font_base *)font)->UID.id; 
#else
    return font->id;
#endif
}

private pdf_font_cache_elem_t **
pdf_locate_font_cache_elem(gx_device_pdf *pdev, gs_font *font)
{
    pdf_font_cache_elem_t **e = &pdev->font_cache;
    long id = pdf_font_cache_elem_id(font);

    for (; *e != 0; e = &(*e)->next)
	if ((*e)->font_id == id) {
	    return e;
	}
    return 0;
}

private void
pdf_remove_font_cache_elem(pdf_font_cache_elem_t *e0)
{
    gx_device_pdf *pdev = e0->pdev;
    pdf_font_cache_elem_t **e = &pdev->font_cache;

    for (; *e != 0; e = &(*e)->next)
	if (*e == e0) {
	    *e = e0->next;
	    gs_free_object(pdev->pdf_memory, e0->glyph_usage, 
				"pdf_remove_font_cache_elem");
	    gs_free_object(pdev->pdf_memory, e0->real_widths, 
				"pdf_remove_font_cache_elem");
	    e0->glyph_usage = 0;
	    e0->real_widths = 0;
	    gs_free_object(pdev->pdf_memory, e0, 
				"pdf_remove_font_cache_elem");
	    return;
	}
}

private void
font_cache_elem_array_sizes(gx_device_pdf *pdev, gs_font *font,
			    int *num_widths, int *num_chars) 
{
    switch (font->FontType) {
    case ft_composite:
	*num_widths = 0; /* Unused for Type 0 */
	*num_chars = 65536; /* No chance to determine, use max. */
	break;
    case ft_encrypted:
    case ft_encrypted2:
    case ft_user_defined:
    case ft_disk_based:
    case ft_Chameleon:
    case ft_TrueType:
	*num_widths = *num_chars = 256; /* Assuming access to glyph_usage by character codes */
	break;
    case ft_CID_encrypted:
	*num_widths = *num_chars = ((gs_font_cid0 *)font)->cidata.common.CIDCount;
	break;
    case ft_CID_TrueType:
	*num_widths = *num_chars = ((gs_font_cid2 *)font)->cidata.common.CIDCount;
	break;
    default:
	*num_widths = *num_chars = 65536; /* No chance to determine, use max. */
    }
}

private int 
alloc_font_cache_elem_arrays(gx_device_pdf *pdev, pdf_font_cache_elem_t *e,
			     gs_font *font)
{
    int num_widths, num_chars, len;

    font_cache_elem_array_sizes(pdev, font, &num_widths, &num_chars);
    len = (num_chars + 7) / 8;
    e->glyph_usage = gs_alloc_bytes(pdev->pdf_memory, 
			len, "alloc_font_cache_elem_arrays");

    e->real_widths = (num_widths > 0 ? (double *)gs_alloc_bytes(pdev->pdf_memory, 
			num_widths * sizeof(*e->real_widths) *
			    (font->FontType == ft_user_defined ? 2 : 1),
			"alloc_font_cache_elem_arrays") : NULL);
    if (e->glyph_usage == NULL || (num_widths !=0 && e->real_widths == NULL)) {
	gs_free_object(pdev->pdf_memory, e->glyph_usage, 
			    "pdf_attach_font_resource");
	gs_free_object(pdev->pdf_memory, e->real_widths, 
			    "alloc_font_cache_elem_arrays");
	return_error(gs_error_VMerror);
    }
    e->num_chars = num_chars;
    e->num_widths = num_widths;
    memset(e->glyph_usage, 0, len);
    memset(e->real_widths, 0, num_widths * sizeof(*e->real_widths));
    return 0;
}

/*
 * Retrive font resource attached to a font,
 * allocating glyph_usage and real_widths on request.
 */
int
pdf_attached_font_resource(gx_device_pdf *pdev, gs_font *font, 
			    pdf_font_resource_t **pdfont, byte **glyph_usage, 
			    double **real_widths, int *num_chars, int *num_widths)
{
    pdf_font_cache_elem_t **e = pdf_locate_font_cache_elem(pdev, font);

    if (e != NULL && (((*e)->glyph_usage == NULL && glyph_usage !=NULL) ||
		      ((*e)->real_widths == NULL && real_widths !=NULL))) {
	int code = alloc_font_cache_elem_arrays(pdev, *e, font);

	if (code < 0)
	    return code;
    }
    *pdfont = (e == NULL ? NULL : (*e)->pdfont);
    if (glyph_usage != NULL)
	*glyph_usage = (e == NULL ? NULL : (*e)->glyph_usage);
    if (real_widths != NULL)
	*real_widths = (e == NULL ? NULL : (*e)->real_widths);
    if (num_chars != NULL)
	*num_chars = (e == NULL ? 0 : (*e)->num_chars);
    if (num_widths != NULL)
	*num_widths = (e == NULL ? 0 : (*e)->num_widths);
    return 0;
}

private int 
pdf_notify_remove_font(void *proc_data, void *event_data)
{   /* gs_font_finalize passes event_data == NULL, so check it here. */
    if (event_data == NULL)
	pdf_remove_font_cache_elem((pdf_font_cache_elem_t *)proc_data);
    return 0;
}

/*
 * Attach font resource to a font.
 */
int
pdf_attach_font_resource(gx_device_pdf *pdev, gs_font *font, 
			 pdf_font_resource_t *pdfont)
{
    int num_chars, num_widths, len;
    pdf_font_cache_elem_t *e, **pe = pdf_locate_font_cache_elem(pdev, font);

    if (pdfont->FontType != font->FontType)
	return_error(gs_error_unregistered); /* Must not happen. */
    font_cache_elem_array_sizes(pdev, font, &num_widths, &num_chars);
    len = (num_chars + 7) / 8;
    if (pe != NULL) {
	e = *pe;
	if (e->pdfont == pdfont)
	    return 0;
	e->pdfont = pdfont;
	/* Reset glyph cache because e->pdfont had changed. */
	memset(e->glyph_usage, 0, len);
	memset(e->real_widths, 0, num_widths * sizeof(*e->real_widths));
    } else {
	int code;
	e = (pdf_font_cache_elem_t *)gs_alloc_struct(pdev->pdf_memory,
		pdf_font_cache_elem_t, &st_pdf_font_cache_elem,
			    "pdf_attach_font_resource");
	if (e == NULL)
	    return_error(gs_error_VMerror);
	e->pdfont = pdfont;
	e->font_id = pdf_font_cache_elem_id(font);
	e->num_chars = 0;
	e->glyph_usage = NULL;
	e->real_widths = NULL;
	e->pdev = pdev;
	e->next = pdev->font_cache;
	pdev->font_cache = e;
	code = gs_notify_register(&font->notify_list, pdf_notify_remove_font, e);
	if (code < 0)
	    return code;
    }
    return 0;
}

/* ================ Process text ================ */

/* ---------------- Internal utilities ---------------- */

/*
 * Compute and return the orig_matrix of a font.
 */
int
pdf_font_orig_matrix(const gs_font *font, gs_matrix *pmat)
{
    switch (font->FontType) {
    case ft_composite:		/* subfonts have their own FontMatrix */
    case ft_TrueType:
    case ft_CID_TrueType:
	/* The TrueType FontMatrix is 1 unit per em, which is what we want. */
	gs_make_identity(pmat);
	return 0;
    case ft_encrypted:
    case ft_encrypted2:
    case ft_CID_encrypted:
    case ft_user_defined:
	/*
         * Type 1 fonts are supposed to use a standard FontMatrix of
         * [0.001 0 0 0.001 0 0], with a 1000-unit cell.  However,
         * Windows NT 4.0 creates Type 1 fonts, apparently derived from
         * TrueType fonts, that use a 2048-unit cell and corresponding
         * FontMatrix.  Also, some PS programs perform font scaling by
         * replacing FontMatrix like this :
         *
         *   /f12 /Times-Roman findfont
         *   copyfont	  % (remove FID)
         *   dup /FontMatrix [0.012 0 0 0.012 0 0] put
         *   definefont
         *   /f12 1 selectfont
         *
         * Such fonts are their own "base font", but the orig_matrix
         * must still be set to 0.001, not 0.012 .
         *
         * The old code used a heuristic to detect and correct for this here.
	 * Unfortunately it doesn't work properly when it meets a font 
	 * with FontMatrix like this : 
	 *
	 *   /FontMatrix [1 2288 div 0 0 1 2288 div 0 0 ] def
	 *
	 * (the bug 686970). Also comparefiles\455690.pdf appears to
	 * have similar problem. Therefore we added a support to lib/gs_fonts.ps,
	 * src/zbfont.c, src/gsfont.c that provides an acces to the original
	 * font via a special key .OrigFont added to the font dictionary while definefont.
	 * Now we work through this access with PS interpreter, 
	 * but keep the old heuristic for other clients.
	 */
	{
	    const gs_font *base_font = font;

	    while (base_font->base != base_font)
		base_font = base_font->base;
	    if (font->FontType == ft_user_defined)
		*pmat = base_font->FontMatrix;
	    else if (base_font->orig_FontMatrix.xx != 0 || base_font->orig_FontMatrix.xy != 0 ||
	        base_font->orig_FontMatrix.yx != 0 || base_font->orig_FontMatrix.yy != 0)
		*pmat = base_font->orig_FontMatrix;
	    else {
		/*  Must not happen with PS interpreter. 
		    Provide a hewuristic for other clients.
		*/
		if (base_font->FontMatrix.xx == 1.0/2048 &&
		    base_font->FontMatrix.xy == 0 &&
		    base_font->FontMatrix.yx == 0 &&
		    any_abs(base_font->FontMatrix.yy) == 1.0/2048
		    )
		    *pmat = base_font->FontMatrix;
		else
		    gs_make_scaling(0.001, 0.001, pmat);
	    }
	}
	return 0;
    default:
	return_error(gs_error_rangecheck);
    }
}

int
font_orig_scale(const gs_font *font, double *sx)
{   
    gs_matrix mat;
    int code = pdf_font_orig_matrix(font, &mat);

    if (code < 0)
	return code;
    *sx = mat.xx;
    return 0;
}


/*
 * Check font resource for encoding compatibility.
 */
private bool
pdf_is_compatible_encoding(gx_device_pdf *pdev, pdf_font_resource_t *pdfont,
			   gs_font *font,
			   gs_glyph *glyphs, gs_char *chars, int num_chars)
{   
    int i;

    /*
     * This crude version of the code ignores
     * the possibility of re-encoding characters.
     */
    switch (pdfont->FontType) {
    case ft_composite:
	{   /*
	     * We assume that source document don't redefine CMap
	     * resources and that incremental CMaps do not exist.
	     * Therefore we don't maintain stable CMap copies,
	     * but just compare CMap names for equality.
	     * A better implementation should compare the chars->glyphs
	     * translation against the stable copy of CMap,
	     * which to be handled with PDF CMap resource.
	     */
	    gs_font_type0 *pfont = (gs_font_type0 *)font;

	    if (pfont->data.FMapType == fmap_CMap) {
		const gs_cmap_t *pcmap = pfont->data.CMap;
		const gs_const_string *s0 = &pdfont->u.type0.CMapName;
		const gs_const_string *s1 = &pcmap->CMapName;

		return (s0->size == s1->size &&
			!memcmp(s0->data, s1->data, s0->size));
	    }
	}
	return false;
    case ft_user_defined:
	if (pdfont->u.simple.Encoding == NULL)
	    return false; /* Not sure. Happens with 020-01.ps . */
	/* fall through */
    case ft_encrypted:
    case ft_encrypted2:
    case ft_TrueType:
	for (i = 0; i < num_chars; ++i) {
	    gs_char ch = chars[i];
	    pdf_encoding_element_t *pet = &pdfont->u.simple.Encoding[ch];

	    if (glyphs[i] == pet->glyph)
		continue;
	    if (pet->glyph != GS_NO_GLYPH) /* encoding conflict */
		return false;
	}
	return true;
    case ft_CID_encrypted:
    case ft_CID_TrueType:
	{
	    gs_font *font1 = (gs_font *)pdf_font_resource_font(pdfont, false);

	    return gs_is_CIDSystemInfo_compatible( 
				gs_font_cid_system_info(font), 
				gs_font_cid_system_info(font1));
	}
    default:
	return false;
    }
}

/* 
 * Find a font resource compatible with a given font. 
 */
private int
pdf_find_font_resource(gx_device_pdf *pdev, gs_font *font, 
		       pdf_resource_type_t type,
		       pdf_font_resource_t **ppdfont, 
		       gs_glyph *glyphs, gs_char *chars, int num_chars)
{
    pdf_resource_t **pchain = pdev->resources[type].chains;
    pdf_resource_t *pres;
    int i;
    
    for (i = 0; i < NUM_RESOURCE_CHAINS; i++) {
	for (pres = pchain[i]; pres != 0; pres = pres->next) {
	    pdf_font_resource_t *pdfont = (pdf_font_resource_t *)pres;
	    const gs_font_base *cfont;
	    gs_font *ofont = font;
	    int code;

	    if (font->FontType != pdfont->FontType)
		continue;
	    if (pdfont->FontType == ft_composite) {
		gs_font_type0 *font0 = (gs_font_type0 *)font;

		ofont = font0->data.FDepVector[0]; /* See pdf_make_font_resource. */
		cfont = pdf_font_resource_font(pdfont->u.type0.DescendantFont, false);
		if (font0->data.CMap->WMode != pdfont->u.type0.WMode)
		    continue;
	    } else 
		cfont = pdf_font_resource_font(pdfont, false);
	    if (chars != NULL &&
		!pdf_is_compatible_encoding(pdev, pdfont, font, glyphs, chars, num_chars))
		continue;
	    if (cfont == 0)
		continue;
	    code = gs_copied_can_copy_glyphs((const gs_font *)cfont, ofont, 
					     glyphs, num_chars, true);
	    if (code == gs_error_unregistered) /* Debug purpose only. */
		return code;
	    if(code > 0) {
		*ppdfont = pdfont;
		return 1;
	    } 
	}
    }
    return 0;
}

/* 
 * Find a type0 font resource for a gived descendent name and CMap name. 
 */
private int
pdf_find_type0_font_resource(gx_device_pdf *pdev, const pdf_font_resource_t *pdsubf, 
	    const gs_const_string *CMapName, pdf_font_resource_t **ppdfont)
{
    pdf_resource_t **pchain = pdev->resources[resourceFont].chains;
    pdf_resource_t *pres;
    int i;
    
    for (i = 0; i < NUM_RESOURCE_CHAINS; i++) {
	for (pres = pchain[i]; pres != 0; pres = pres->next) {
	    pdf_font_resource_t *pdfont = (pdf_font_resource_t *)pres;

	    if (pdfont->FontType != ft_composite)
		continue;
	    if (pdfont->u.type0.DescendantFont != pdsubf)
		continue;
	    if (pdfont->BaseFont.size != pdsubf->BaseFont.size + CMapName->size + 1)
		continue;
	    if (memcmp(pdfont->BaseFont.data + pdsubf->BaseFont.size + 1, 
			CMapName->data, CMapName->size))
		continue;
	    *ppdfont = pdfont;
	    return 1;
	}
    }
    return 0;
}


private int pdf_make_font_resource(gx_device_pdf *pdev, gs_font *font,
		       pdf_font_resource_t **ppdfont, 
		       gs_glyph *glyphs, gs_char *chars, int num_chars);

/*
 * Create or find a CID font resource object for a glyph set.
 */
int
pdf_obtain_cidfont_resource(gx_device_pdf *pdev, gs_font *subfont, 
			    pdf_font_resource_t **ppdsubf, 
			    gs_glyph *glyphs, int num_glyphs)
{
    int code = 0;

    pdf_attached_font_resource(pdev, subfont, ppdsubf, NULL, NULL, NULL, NULL);
    if (*ppdsubf == NULL) {
	code = pdf_find_font_resource(pdev, subfont, 
				      resourceCIDFont, ppdsubf, 
				      glyphs, NULL, num_glyphs); 
	if (code < 0)
	    return code;
	if (*ppdsubf == NULL) {
	    code = pdf_make_font_resource(pdev, subfont, ppdsubf, 
					  NULL, NULL, 0);
	    if (code < 0)
		return code;
	}
	code = pdf_attach_font_resource(pdev, subfont, *ppdsubf);
    } else {
	/* If composite fonts share subfonts, their 
	 * subfont resources to be shared as well.
	 */
    }
    return code;
}

/*
 * Refine index of BaseEncoding.
 */
private int 
pdf_refine_encoding_index(int index, bool is_standard)
{
    /*
     * Per the PDF 1.3 documentation, there are only 3 BaseEncoding
     * values allowed for non-embedded fonts.  Pick one here.
     */
    switch (index) {
    case ENCODING_INDEX_WINANSI:
    case ENCODING_INDEX_MACROMAN:
    case ENCODING_INDEX_MACEXPERT:
	return index;
    case ENCODING_INDEX_STANDARD:
	if (is_standard)
	    return index;
	/* Falls through. */
    default:
	return ENCODING_INDEX_WINANSI;
    }
}

/*
 * Create a font resource object for a gs_font.  Return 1 iff the
 * font was newly created (it's a roudiment, keeping reverse compatibility).
 * This procedure is only intended to be called
 * from a few places in the text code.
 */
private int
pdf_make_font_resource(gx_device_pdf *pdev, gs_font *font,
		       pdf_font_resource_t **ppdfont, 
		       gs_glyph *glyphs, gs_char *chars, int num_chars)
{
    int index = -1;
    int BaseEncoding = ENCODING_INDEX_UNKNOWN;
    pdf_font_embed_t embed;
    pdf_font_descriptor_t *pfd = 0;
    int (*font_alloc)(gx_device_pdf *, pdf_font_resource_t **,
		      gs_id, pdf_font_descriptor_t *);
    gs_font *base_font = font; /* A roudiment from old code. Keep it for a while. */
    pdf_font_resource_t *pdfont;
    pdf_standard_font_t *const psfa =
	pdev->text->outline_fonts->standard_fonts;
    int code = 0;

    embed = pdf_font_embed_status(pdev, base_font, &index, glyphs, num_chars);
    if (embed == FONT_EMBED_STANDARD) {
	pdf_standard_font_t *psf = &psfa[index];

	if (psf->pdfont == NULL ||
		!pdf_is_compatible_encoding(pdev, psf->pdfont, font,
			glyphs, chars, num_chars)) {
	    code = pdf_font_std_alloc(pdev, ppdfont, (psf->pdfont == NULL), base_font->id,
				      (gs_font_base *)base_font, index);
	    if (code < 0)
		return code;
	    if (psf->pdfont == NULL)
		psf->pdfont = *ppdfont;
	    (*ppdfont)->u.simple.BaseEncoding = pdf_refine_encoding_index(
		((const gs_font_base *)base_font)->nearest_encoding_index, true);
	    code = 1;
	} else
	    *ppdfont = psf->pdfont;
	return code;
    } 

    switch (font->FontType) {
    case ft_CID_encrypted:
    case ft_CID_TrueType:
	font_alloc = pdf_font_cidfont_alloc;
	break;
    case ft_encrypted:
    case ft_encrypted2:
    case ft_TrueType:
	font_alloc = pdf_font_simple_alloc;
	break;
    case ft_user_defined:
	{
	    const gs_font_base *bfont = (const gs_font_base *)font;

	    code = font_resource_encoded_alloc(pdev, &pdfont, gs_no_id, 
			    ft_user_defined, pdf_write_contents_bitmap);
	    if (code < 0)
		return code;
	    pdfont->u.simple.s.type3.bitmap_font = false;
	    pdfont->u.simple.BaseEncoding = pdf_refine_encoding_index(
				bfont->nearest_encoding_index, true);
	    pdfont->u.simple.s.type3.FontBBox.p.x = (int)bfont->FontBBox.p.x;
	    pdfont->u.simple.s.type3.FontBBox.p.y = (int)bfont->FontBBox.p.y;
	    pdfont->u.simple.s.type3.FontBBox.q.x = (int)bfont->FontBBox.q.x;
	    pdfont->u.simple.s.type3.FontBBox.q.y = (int)bfont->FontBBox.q.y;
	    pdfont->u.simple.s.type3.FontMatrix = bfont->FontMatrix;
	    *ppdfont = pdfont;
	    return 1;
	}
    default:
	return_error(gs_error_invalidfont);
    }

    /* Create an appropriate font resource and descriptor. */
    if (embed == FONT_EMBED_YES) {
	/*
	 * HACK: Acrobat Reader 3 has a bug that makes cmap formats 4
	 * and 6 not work in embedded TrueType fonts.  Consequently, it
	 * can only handle embedded TrueType fonts if all the glyphs
	 * referenced by the Encoding have numbers 0-255.  Check for
	 * this now.
	 */
	if (font->FontType == ft_TrueType &&
	    pdev->CompatibilityLevel <= 1.2
	    ) {
	    int i;

	    for (i = 0; i <= 0xff; ++i) {
		gs_glyph glyph =
		    font->procs.encode_char(font, (gs_char)i,
					    GLYPH_SPACE_INDEX);

		if (glyph == GS_NO_GLYPH ||
		    (glyph >= GS_MIN_GLYPH_INDEX &&
		     glyph <= GS_MIN_GLYPH_INDEX + 0xff)
		    )
		    continue;
		/* Can't embed, punt. */
		return_error(gs_error_rangecheck);
	    }
	}
    }
    if (font->FontType == ft_encrypted || font->FontType == ft_encrypted2 || 
	font->FontType == ft_TrueType) {
        /*
	 * We write True Types with Symbolic flag set.
	 * PDF spec says that "symbolic font should not specify Encoding entry"
	 * (see section 5.5, the article "Encodings for True Type fonts", paragraph 3).
	 * However Acrobat Reader 4,5,6 fail when TT font with no Encoding
	 * appears in a document together with a CID font with a non-standard CMap
	 * (AR 4 and 5 claim "The encoding (CMap) specified by a font is corrupted."
	 * (we read it as "The encoding or CMap specified by a font is corrupted.",
	 * and apply the 1st alternative)). We believe that AR is buggy, 
	 * and therefore we write an Encoding with non-CID True Type fonts.
	 * Hopely other viewers can ignore Encoding in such case. Actually in this case 
	 * an Encoding doesn't add an useful information.
	 */
	BaseEncoding = pdf_refine_encoding_index(
	    ((const gs_font_base *)base_font)->nearest_encoding_index, false);
    }
    if ((code = pdf_font_descriptor_alloc(pdev, &pfd,
					  (gs_font_base *)base_font,
					  embed == FONT_EMBED_YES)) < 0 ||
	(code = font_alloc(pdev, &pdfont, base_font->id, pfd)) < 0
	)
	return code;
    code = 1;

    if (!pdf_is_CID_font(font))
	pdfont->u.simple.BaseEncoding = BaseEncoding;

    *ppdfont = pdfont;
    return 1;
}

/*
 * Check for simple font.
 */
bool 
pdf_is_simple_font(gs_font *font)
{ 
    return (font->FontType == ft_encrypted ||
	    font->FontType == ft_encrypted2 ||
	    font->FontType == ft_TrueType ||
	    font->FontType == ft_user_defined);
}

/*
 * Check for CID font.
 */
bool 
pdf_is_CID_font(gs_font *font)
{ 
    return (font->FontType == ft_CID_encrypted ||
	    font->FontType == ft_CID_TrueType);
}

/*
 * Enumerate glyphs for a text.
 */
private int
pdf_next_char_glyph(gs_text_enum_t *penum, const gs_string *pstr, 
	       /* const */ gs_font *font, bool font_is_simple, 
	       gs_char *char_code, gs_char *cid, gs_glyph *glyph)
{
    int code = font->procs.next_char_glyph(penum, char_code, glyph);

    if (code == 2)		/* end of string */
	return code;
    if (code < 0)
	return code;
    if (font_is_simple) {
	*cid = *char_code;
	*glyph = font->procs.encode_char(font, *char_code, GLYPH_SPACE_NAME);
	if (*glyph == GS_NO_GLYPH)
	    return 3;
    } else {
	if (*glyph < GS_MIN_CID_GLYPH)
	    return 3; /* Not sure why, copied from scan_cmap_text. */
	*cid = *glyph - GS_MIN_CID_GLYPH; /* CID */
    }
    return 0;
}

private void
store_glyphs(gs_glyph *glyphs, int glyphs_offset,
	     gs_char *chars, int chars_offset,
	     byte *glyph_usage, int char_cache_size,
	     int *num_all_chars, int *num_unused_chars,
	     gs_char char_code, gs_char cid, gs_glyph glyph)
{
    int j;

    for (j = 0; j < *num_all_chars; j++)
	if (chars[j] == cid)
	    break;
    if (j < *num_all_chars)
	return;
    glyphs[*num_all_chars] = glyph;
    chars[*num_all_chars] = char_code;
    (*num_all_chars)++;
    if (glyph_usage == 0 || !(glyph_usage[cid / 8] & (0x80 >> (cid & 7)))) {
	glyphs[glyphs_offset + *num_unused_chars] = glyph;
    	chars[glyphs_offset + *num_unused_chars] = char_code;
	(*num_unused_chars)++;
    }
    /* We are disliked that gs_copied_can_copy_glyphs can get redundant
     * glyphs, if Encoding specifies several codes for same glyph. 
     * But we need the positional correspondence
     * of glyphs to codes for pdf_is_compatible_encoding.
     * Redundant glyphs isn't a big payment for it
     * because they happen seldom.
     */
}

/*
 * Create or find a font resource object for a text.
 */
int
pdf_obtain_font_resource(const gs_text_enum_t *penum, 
	    const gs_string *pstr, pdf_font_resource_t **ppdfont)
{
    gx_device_pdf *pdev = (gx_device_pdf *)penum->dev;
    gs_font *font = (gs_font *)penum->current_font;
    bool font_is_simple = pdf_is_simple_font(font);
    int num_unused_chars = 0, num_all_chars = 0;
    int glyphs_offset = (pstr != NULL ? pstr->size : penum->text.size);
    const int buf_elem_size = sizeof(gs_glyph) * 2 + sizeof(gs_char) * 2;
    gs_glyph glyphs0[200], *glyphs = glyphs0;
    gs_char *chars;
    byte *glyph_usage = 0;
    double *real_widths;
    int char_cache_size, width_cache_size;
    gs_char char_code, cid;
    gs_glyph glyph;
    gs_text_enum_t scan;
    int code;

    if (font->FontType == ft_composite) {
	/* Must not happen, because we always split composite fonts into descendents. */
	return_error(gs_error_unregistered);
    }
    /* Get attached font resource (maybe NULL) */
    code = pdf_attached_font_resource(pdev, font, ppdfont,
			       &glyph_usage, &real_widths, &char_cache_size, &width_cache_size);
    if (code < 0)
	return code;
    /* Allocate memory for the glyph set : */
    if (glyphs_offset * buf_elem_size > sizeof(glyphs0)) {
	glyphs = (gs_glyph *)gs_alloc_bytes(pdev->memory, 
		buf_elem_size * glyphs_offset, "pdf_encode_string");
	if (glyphs == 0)
	    return_error(gs_error_VMerror);
    }
    chars = (gs_char *)(glyphs + glyphs_offset * 2);
    /* Build the glyph set of the text : */
    scan = *penum;
    if (pstr != NULL) {
	scan.text.data.bytes = pstr->data;
	scan.text.size = pstr->size;
    }
    for (;;) {
	code = pdf_next_char_glyph(&scan, pstr, font, font_is_simple, 
				   &char_code, &cid, &glyph);
	if (code == 2)		/* end of string */
	    break;
	if (code == 3)		/* no glyph */
	    continue;
	if (code < 0)
	    return code;
	if (num_all_chars > glyphs_offset)
	    return_error(gs_error_unregistered); /* Must not happen. */
	if (glyph_usage != 0 && cid > char_cache_size)
	    continue;
	store_glyphs(glyphs, glyphs_offset,	chars, glyphs_offset,
		     glyph_usage, char_cache_size,
		     &num_all_chars, &num_unused_chars,
		     char_code, cid, glyph);
    }
    /* Get/make font resource for the font : */
    if (*ppdfont != 0) {
	gs_font_base *cfont = pdf_font_resource_font(*ppdfont, false);
        
	if (font->FontType != ft_user_defined) {
	    code = gs_copied_can_copy_glyphs((gs_font *)cfont, font, 
			glyphs + glyphs_offset, num_unused_chars, false);
	    if (code < 0)
		goto out;
	} else
	    code = 1;
	if (code == 0)
	    *ppdfont = 0;
	else if(!pdf_is_compatible_encoding(pdev, *ppdfont, font,
			glyphs + glyphs_offset, chars + glyphs_offset, 
			num_unused_chars))
	    *ppdfont = 0;
    }
    if (*ppdfont == 0) {
	gs_font *base_font = font;
	gs_font *below;
	bool same_encoding = true;

	/* 
	 * Find the "lowest" base font that has the same outlines.
	 * We use its FontName for font resource. 
	 */
	while ((below = base_font->base) != base_font &&
	       base_font->procs.same_font(base_font, below, FONT_SAME_OUTLINES))
	    base_font = below;
	if (base_font != font)
	    same_encoding = ((base_font->procs.same_font(base_font, font, 
	                      FONT_SAME_ENCODING) & FONT_SAME_ENCODING) != 0);
	/* Find or make font resource. */
	pdf_attached_font_resource(pdev, base_font, ppdfont, NULL, NULL, NULL, NULL);
	if (*ppdfont != NULL && base_font != font) {
	    if(!pdf_is_compatible_encoding(pdev, *ppdfont, 
				    base_font, glyphs, chars, num_all_chars))
		*ppdfont = NULL;
	}
	if (*ppdfont == NULL) {
	    pdf_resource_type_t type = 
		(pdf_is_CID_font(base_font) ? resourceCIDFont 
					    : resourceFont);
    	    code = pdf_find_font_resource(pdev, base_font, type, ppdfont, 
					  glyphs, chars, num_all_chars); 
	    if (code < 0)
		goto out;
	    if (*ppdfont == NULL) {
		code = pdf_make_font_resource(pdev, base_font, ppdfont, 
					      glyphs, chars, num_all_chars);
		if (code < 0)
		    goto out;
	    }
	    if (base_font != font && same_encoding) {
		code = pdf_attach_font_resource(pdev, base_font, *ppdfont);
		if (code < 0) 		    
		    goto out;
	    }
	}
	code = pdf_attach_font_resource(pdev, font, *ppdfont);
	if (code < 0)
	    goto out;
    }
    code = pdf_attached_font_resource(pdev, font, ppdfont, 
			       &glyph_usage, &real_widths, &char_cache_size, &width_cache_size);
    if (code < 0)
	return code;
    /* Mark glyphs used in the text with the font resources. */
    scan = *penum;
    if (pstr != NULL) {
	scan.text.data.bytes = pstr->data;
	scan.text.size = pstr->size;
    }
    for (;;) {
	int code = pdf_next_char_glyph(&scan, pstr, font, font_is_simple, 
				       &char_code, &cid, &glyph);

	if (code == 2)		/* end of string */
	    break;
	if (code == 3)		/* no glyph */
	    continue;
	if (code < 0)
	    return code;
	if (glyph_usage != 0 && cid >= char_cache_size)
	    continue;
	glyph_usage[cid / 8] |= 0x80 >> (cid & 7);
    }
out:
    if (glyphs != glyphs0)
	gs_free_object(pdev->memory, glyphs, "pdf_encode_string");
    return code;
}

private inline bool
strings_equal(const gs_const_string *s1, const gs_const_string *s2)
{
    return s1->size == s2->size &&
	    !memcmp(s1->data, s2->data, s1->size);
}

/*
 * Create or find a parent Type 0 font resource object for a CID font resource.
 */
int
pdf_obtain_parent_type0_font_resource(gx_device_pdf *pdev, pdf_font_resource_t *pdsubf, 
		const gs_const_string *CMapName, pdf_font_resource_t **pdfont)
{
    if (pdsubf->u.cidfont.parent != 0 && 
	    strings_equal(CMapName, &pdsubf->u.cidfont.parent->u.type0.CMapName))
	*pdfont = pdsubf->u.cidfont.parent;
    else {
	/*
	 * PDF spec 1.4 section 5.6 "Composite Fonts" says :
	 *
	 * PDF 1.2 introduces a general architecture for composite fonts that theoretically
	 * allows a Type 0 font to have multiple descendants,which might themselves be
	 * Type 0 fonts.However,in versions up to and including PDF 1.4,only a single
	 * descendant is allowed,which must be a CIDFont (not a font).This restriction
	 * may be relaxed in a future PDF version.
	 */

	if (pdsubf->u.cidfont.parent == NULL || 
		pdf_find_type0_font_resource(pdev, pdsubf, CMapName, pdfont) <= 0) {
	    int code = pdf_font_type0_alloc(pdev, pdfont, gs_no_id, pdsubf, CMapName);

	    if (code < 0)
		return code;
	}
	pdsubf->u.cidfont.parent = *pdfont;
    }
    return 0;
}

/*
 * Compute the cached values in the text processing state from the text
 * parameters, current_font, and pis->ctm.  Return either an error code (<
 * 0) or a mask of operation attributes that the caller must emulate.
 * Currently the only such attributes are TEXT_ADD_TO_ALL_WIDTHS and
 * TEXT_ADD_TO_SPACE_WIDTH.  Note that this procedure fills in all the
 * values in ppts->values, not just the ones that need to be set now.
 */
private int
transform_delta_inverse(const gs_point *pdelta, const gs_matrix *pmat,
			gs_point *ppt)
{
    int code = gs_distance_transform_inverse(pdelta->x, pdelta->y, pmat, ppt);
    gs_point delta;

    if (code < 0)
	return code;
    if (ppt->y == 0)
	return 0;
    /* Check for numerical fuzz. */
    code = gs_distance_transform(ppt->x, 0.0, pmat, &delta);
    if (code < 0)
	return 0;		/* punt */
    if (fabs(delta.x - pdelta->x) < 0.01 && fabs(delta.y - pdelta->y) < 0.01) {
	/* Close enough to y == 0: device space error < 0.01 pixel. */
	ppt->y = 0;
    }
    return 0;
}
int
pdf_update_text_state(pdf_text_process_state_t *ppts,
		      const pdf_text_enum_t *penum,
		      pdf_font_resource_t *pdfont, const gs_matrix *pfmat)
{
    gx_device_pdf *const pdev = (gx_device_pdf *)penum->dev;
    gs_font *font = penum->current_font;
    gs_fixed_point cpt;
    gs_matrix orig_matrix, smat, tmat;
    double
	sx = pdev->HWResolution[0] / 72.0,
	sy = pdev->HWResolution[1] / 72.0;
    float size;
    float c_s = 0, w_s = 0;
    int mask = 0;
    int code = gx_path_current_point(penum->path, &cpt);

    if (code < 0)
	return code;

    /* Get the original matrix of the base font. */

    {
	gs_font_base *cfont = pdf_font_resource_font(pdfont, false);

	if (cfont != 0) {
	    /*
	     * The text matrix to be computed relatively to the 
	     * embedded font matrix.
	     */
	    orig_matrix = cfont->FontMatrix;
	} else {
	    /*
	     * We don't embed the font.
	     * The text matrix to be computed relatively to
	     * standard font matrix.
	     */
	    pdf_font_orig_matrix(font, &orig_matrix);
	}
    }

    /* Compute the scaling matrix and combined matrix. */

    gs_matrix_invert(&orig_matrix, &smat);
    gs_matrix_multiply(&smat, pfmat, &smat);
    tmat = ctm_only(penum->pis);
    tmat.tx = tmat.ty = 0;
    gs_matrix_multiply(&smat, &tmat, &tmat);

    /* Try to find a reasonable size value.  This isn't necessary, */
    /* but it's worth a little effort. */

    size = hypot(tmat.yx, tmat.yy) / sy;
    if (size < 0.01)
	size = hypot(tmat.xx, tmat.xy) / sx;
    if (size < 0.01)
	size = 1;

    /* Check for spacing parameters we can handle, and transform them. */

    if (penum->text.operation & TEXT_ADD_TO_ALL_WIDTHS) {
	if (penum->current_font->WMode == 0) {
	    gs_point pt;

	    code = transform_delta_inverse(&penum->text.delta_all, &smat, &pt);
	    if (code >= 0 && pt.y == 0)
		c_s = pt.x * size;
	    else
		mask |= TEXT_ADD_TO_ALL_WIDTHS;
	}
	else
	    mask |= TEXT_ADD_TO_ALL_WIDTHS;
    }
    
    if (penum->text.operation & TEXT_ADD_TO_SPACE_WIDTH) {
	gs_point pt;

	code = transform_delta_inverse(&penum->text.delta_space, &smat, &pt);
	if (code >= 0 && pt.y == 0 && penum->text.space.s_char == 32)
	    w_s = pt.x * size;
	else
	    mask |= TEXT_ADD_TO_SPACE_WIDTH;
    }
    /* Store the updated values. */

    tmat.xx /= size;
    tmat.xy /= size;
    tmat.yx /= size;
    tmat.yy /= size;
    tmat.tx += fixed2float(cpt.x);
    tmat.ty += fixed2float(cpt.y);

    ppts->values.character_spacing = c_s;
    ppts->values.pdfont = pdfont;
    ppts->values.size = size;
    ppts->values.matrix = tmat;
    ppts->values.render_mode = (penum->pis->text_rendering_mode == 3 ? 3 : 
				font->PaintType == 0 ? 0 : 1);
    ppts->values.word_spacing = w_s;
    ppts->font = font;

    code = pdf_set_text_process_state(pdev, (const gs_text_enum_t *)penum,
				      ppts);
    return (code < 0 ? code : mask);
}

/*
 * Set up commands to make the output state match the processing state.
 * General graphics state commands are written now; text state commands
 * are written later.
 */
private double
font_matrix_scaling(const gs_font *font)
{
    return fabs((font->FontMatrix.yy != 0 ? font->FontMatrix.yy :
		 font->FontMatrix.yx));
}
int
pdf_set_text_process_state(gx_device_pdf *pdev,
			   const gs_text_enum_t *pte,	/* for pdcolor, pis */
			   pdf_text_process_state_t *ppts)
{
    /*
     * Setting the stroke parameters may exit text mode, causing the
     * settings of the text parameters to be lost.  Therefore, we set the
     * stroke parameters first.
     */
    if (pdf_render_mode_uses_stroke(pdev, &ppts->values)) {
	/* Write all the parameters for stroking. */
	gs_imager_state *pis = pte->pis;
	float save_width = pis->line_params.half_width;
	const gs_font *font = ppts->font;
	double scaled_width = font->StrokeWidth;
	int code;

	/* Note that we compute pis->line_params.half_width in device space,
	 * even though it logically represents a value in user space.  
	 * The 'scale' value compensates for this.
	 */
	scaled_width *= font_matrix_scaling(font);
	scaled_width *= min(hypot(pte->pis->ctm.xx, pte->pis->ctm.yx) / 
                                pdev->HWResolution[0] * pdev->HWResolution[1],
                            hypot(pte->pis->ctm.xy, pte->pis->ctm.yy));
	pis->line_params.half_width = scaled_width / 2;
	code = pdf_prepare_stroke(pdev, pis);
	if (code >= 0) {
	    /*
	     * See stream_to_text in gdevpdfu.c re the computation of
	     * the scaling value.
	     */
	    double scale = 72.0 / pdev->HWResolution[1];

	    code = gdev_vector_prepare_stroke((gx_device_vector *)pdev,
					      pis, NULL, NULL, scale);
	}
	pis->line_params.half_width = save_width;
	if (code < 0)
	    return code;
    }

    /* Now set all the other parameters. */

    return pdf_set_text_state_values(pdev, &ppts->values);
}

private int
store_glyph_width(pdf_glyph_width_t *pwidth, int wmode, double scale,
		  const gs_glyph_info_t *pinfo)
{
    double w, v;

    pwidth->xy.x = pinfo->width[wmode].x * scale;
    pwidth->xy.y = pinfo->width[wmode].y * scale;
    if (wmode)
	w = pwidth->xy.y, v = pwidth->xy.x;
    else
	w = pwidth->xy.x, v = pwidth->xy.y;
    if (v != 0)
	return 1;
    pwidth->w = w;
    pwidth->v.x = pinfo->v.x * scale;
    pwidth->v.y = pinfo->v.y * scale;
    return 0;
}

private int
get_missing_width(gs_font_base *cfont, int wmode, double scale_c, 
		    pdf_glyph_widths_t *pwidths)
{
    gs_font_info_t finfo;
    int code;

    code = cfont->procs.font_info((gs_font *)cfont, NULL,
				  FONT_INFO_MISSING_WIDTH, &finfo);
    if (code < 0)
	return code;
    if (wmode) {
	pwidths->Width.xy.x = pwidths->real_width.xy.x = 0;
	pwidths->Width.xy.y = pwidths->real_width.xy.y =
		finfo.MissingWidth * scale_c;
	pwidths->Width.w = pwidths->real_width.w =
		pwidths->Width.xy.y;
	pwidths->Width.v.x = pwidths->Width.xy.y / 2;
	pwidths->Width.v.y = pwidths->Width.xy.y;
    } else {
	pwidths->Width.xy.x = pwidths->real_width.xy.x =
		finfo.MissingWidth * scale_c;
	pwidths->Width.w = pwidths->real_width.w =
		pwidths->Width.xy.x;
	pwidths->Width.xy.y = pwidths->real_width.xy.y = 0;
	pwidths->Width.v.x = pwidths->Width.v.y = 0;
    }
    /*
     * Don't mark the width as known, just in case this is an
     * incrementally defined font.
     */
    return 1;
}


/*
 * Get the widths (unmodified from the copied font,
 * and possibly modified from the original font) of a given glyph.
 * Return 1 if the width was defaulted to MissingWidth.
 */
int
pdf_glyph_widths(pdf_font_resource_t *pdfont, int wmode, gs_glyph glyph,
		 gs_font *orig_font, pdf_glyph_widths_t *pwidths)
{
    gs_font_base *cfont = pdf_font_resource_font(pdfont, false);
    gs_font *ofont = orig_font;
    gs_glyph_info_t info;
    /*
     * orig_scale is 1.0 for TrueType, 0.001 or 1.0/2048 for Type 1.
     */
    double sxc, sxo;
    double scale_c, scale_o;
    int code, rcode = 0;
    gs_point v;

    if (ofont->FontType == ft_composite)
	return_error(gs_error_unregistered); /* Must not happen. */
    code = font_orig_scale((const gs_font *)cfont, &sxc);
    if (code < 0)
	return code;
    code = font_orig_scale(ofont, &sxo);
    if (code < 0)
	return code;
    scale_c = sxc * 1000.0;
    scale_o = sxo * 1000.0;
    pwidths->Width.v.x = pwidths->Width.v.y = 0;
    pwidths->real_width.v.x = pwidths->real_width.v.y = 0;
    pwidths->replaced_v = false;
    if (glyph == GS_NO_GLYPH)
	return get_missing_width(cfont, wmode, scale_c, pwidths);
    code = cfont->procs.glyph_info((gs_font *)cfont, glyph, NULL,
				    (GLYPH_INFO_WIDTH0 << wmode) |
				    GLYPH_INFO_OUTLINE_WIDTHS |
				    (GLYPH_INFO_VVECTOR0 << wmode),
				    &info);
    if (code == gs_error_undefined || !(info.members & (GLYPH_INFO_WIDTH0 << wmode))) {
	code = get_missing_width(cfont, wmode, scale_c, pwidths);
	if (code < 0)
	    return code;
	v = pwidths->Width.v;
    } else if (code < 0)
	return code;
    else {
	code = store_glyph_width(&pwidths->Width, wmode, scale_c, &info);
	if (code < 0)
	    return code;
	rcode |= code;
	if (info.members & (GLYPH_INFO_VVECTOR0 | GLYPH_INFO_VVECTOR1)) {
	    v.x = info.v.x * scale_c;
	    v.y = info.v.y * scale_c;
	} else
	    v.x = v.y = 0;
    }
    pwidths->Width.v = v;
    if (code > 0)
	pwidths->Width.xy.x = pwidths->Width.xy.y = pwidths->Width.w = 0;
    code = ofont->procs.glyph_info(ofont, glyph, NULL,
					(GLYPH_INFO_WIDTH0 << wmode) |
					(GLYPH_INFO_VVECTOR0 << wmode),
					&info);
    if (code == gs_error_undefined || !(info.members & (GLYPH_INFO_WIDTH0 << wmode)))
	pwidths->real_width = pwidths->Width;
    else if (code < 0)
	return code;
    else {
	if ((info.members & (GLYPH_INFO_VVECTOR0 | GLYPH_INFO_VVECTOR1)) != 0)
	    pwidths->replaced_v = true;
	else 
	    info.v.x = info.v.y = 0;
	code = store_glyph_width(&pwidths->real_width, wmode, scale_o, &info);
	if (code < 0)
	    return code;
	rcode |= code;
    }
    pwidths->real_width.v.x = info.v.x * scale_o;
    pwidths->real_width.v.y = info.v.y * scale_o;
    return rcode;
}
/* ---------------- Main entry ---------------- */

/*
 * Fall back to the default text processing code when needed.
 */
int
pdf_default_text_begin(gs_text_enum_t *pte, const gs_text_params_t *text,
		       gs_text_enum_t **ppte)
{
    gs_text_params_t text1 = *text;

    if(pte->current_font->FontType == 3 && (text1.operation & TEXT_DO_NONE)) {
	/* We need a real drawing to accumulate charproc. */
	text1.operation &= ~TEXT_DO_NONE;
	text1.operation |= TEXT_DO_DRAW;
    }
    return gx_default_text_begin(pte->dev, pte->pis, &text1, pte->current_font,
				 pte->path, pte->pdcolor, pte->pcpath,
				 pte->memory, ppte);
}

/*
 * Continue processing text.  This is the 'process' procedure in the text
 * enumerator.  Per the check in pdf_text_begin, we know the operation is
 * not a charpath, but it could be anything else.
 */
int
pdf_text_process(gs_text_enum_t *pte)
{
    pdf_text_enum_t *const penum = (pdf_text_enum_t *)pte;
    uint operation = pte->text.operation;
    uint size = pte->text.size - pte->index;
    gs_text_enum_t *pte_default;
    PROCESS_TEXT_PROC((*process));
    int code = -1;		/* to force default implementation */
    gx_device_pdf *pdev = (gx_device_pdf *)penum->dev;
#define BUF_SIZE 100		/* arbitrary > 0 */
    /* Use a union to ensure alignment. */
    union bu_ {
	byte bytes[BUF_SIZE];
	gs_char chars[BUF_SIZE / sizeof(gs_char)];
	gs_glyph glyphs[BUF_SIZE / sizeof(gs_glyph)];
    } buf;

    /*
     * If we fell back to the default implementation, continue using it.
     */
 top:
    pte_default = penum->pte_default;
    if (pte_default) {
	if (penum->charproc_accum) {
	    code = pdf_end_charproc_accum(pdev);
	    if (code < 0)
		return code;
	    penum->charproc_accum = false;
	    code = gx_default_text_restore_state(pte_default);
	    if (code < 0)
		return code;
	    gs_text_release(pte_default, "pdf_text_process");
	    penum->pte_default = 0;
	    goto top;
	}
	pdev->pte = pte_default; /* CAUTION: See comment in gdevpdfx.h . */
	code = gs_text_process(pte_default);
	pdev->pte = NULL;	 /* CAUTION: See comment in gdevpdfx.h . */
	if (pte->current_font->FontType != ft_user_defined)
	    gs_text_enum_copy_dynamic(pte, pte_default, true);
	else {
	    penum->returned.current_char = pte_default->returned.current_char;
	    penum->returned.current_glyph = pte_default->returned.current_glyph;
	}
	if (code)
	    return code;
	gs_text_release(pte_default, "pdf_text_process");
	penum->pte_default = 0;
	return 0;
    }
    {
	gs_font *font = pte->current_font;

	switch (font->FontType) {
	case ft_CID_encrypted:
	case ft_CID_TrueType:
	    process = process_cid_text;
	    break;
	case ft_encrypted:
	case ft_encrypted2:
	case ft_TrueType:
	case ft_user_defined:
	    /* The data may be either glyphs or characters. */
	    process = process_plain_text;
	    break;
	case ft_composite:
	    process =
		(((gs_font_type0 *)font)->data.FMapType == fmap_CMap ?
		 process_cmap_text :
		 process_composite_text);
	    break;
	default:
	    goto skip;
	}
    }

    /*
     * We want to process the entire string in a single call, but we may
     * need to modify it.  Copy it to a buffer.  Note that it may consist
     * of bytes, gs_chars, or gs_glyphs.
     */

    if (operation & (TEXT_FROM_STRING | TEXT_FROM_BYTES))
	DO_NOTHING;
    else if (operation & TEXT_FROM_CHARS)
	size *= sizeof(gs_char);
    else if (operation & TEXT_FROM_SINGLE_CHAR)
	size = sizeof(gs_char);
    else if (operation & TEXT_FROM_GLYPHS)
	size *= sizeof(gs_glyph);
    else if (operation & TEXT_FROM_SINGLE_GLYPH)
	size = sizeof(gs_glyph);
    else
	goto skip;

    if (size <= sizeof(buf)) {
	code = process(pte, buf.bytes, size);
    } else {
	byte *buf = gs_alloc_string(pte->memory, size, "pdf_text_process");

	if (buf == 0)
	    return_error(gs_error_VMerror);
	code = process(pte, buf, size);
	gs_free_string(pte->memory, buf, size, "pdf_text_process");
    }
 skip:
    if (code < 0 || 
	    (pte->current_font->FontType == ft_user_defined && 
	     code != TEXT_PROCESS_INTERVENE &&
	    penum->index < penum->text.size)) {
	if (code == gs_error_unregistered) /* Debug purpose only. */
	    return code;
	if (code == gs_error_VMerror)
	    return code;
	/* Fall back to the default implementation. */
	code = pdf_default_text_begin(pte, &pte->text, &pte_default);
	if (code < 0)
	    return code;
	penum->pte_default = pte_default;
	gs_text_enum_copy_dynamic(pte_default, pte, false);
    }
    /* The 'process' procedure might also have set pte_default itself. */
    if (penum->pte_default && !code)
	goto top;
    return code;
    /*
     * This function uses an unobvious algorithm while handling type 3 fonts.
     * It runs 'process' to copy text until a glyph, which was not copied to
     * output font. Then it installs pte_default and falls back to default
     * implementation with PS interpreter callout. The callout executes 
     * BuildChar/BuildGlyph with setcachedevice. The latter calls
     * pdf_install_charproc_accum, which sets up an accumulator 
     * of graphic objects to a pdf_begin_resource stream.
     * When the callout completes, pdf_text_process calls pdf_end_charproc_accum
     * and later resumes the normal (non-default) text enumeration, repeating the 
     * the "callouted" glyph AT SECOND TIME. We can't do without the second pass
     * becauase in the first pass the glyph widths is unknown.
     */
}
