/* Copyright (C) 1996, 1997, 1998 Aladdin Enterprises.  All rights
   reserved.  Unauthorized use, copying, and/or distribution
   prohibited.  */

/* pcmain.c - PCL5c main program */

#include "malloc_.h"
#include "math_.h"
#include "memory_.h"
#include "stdio_.h"
#include "scommon.h"			/* for pparse.h */
#include "pcparse.h"
#include "pcstate.h"
#include "pcpage.h"
#include "pcident.h"
#include "gdebug.h"
#include "gp.h"
#include "gscdefs.h"
#include "gslib.h"
#include "gsmemory.h"
#include "gsmalloc.h"
#include "gsmatrix.h"		/* for gsstate.h */
#include "gsnogc.h"
#include "gspaint.h"
#include "gsparam.h"
#include "gsstate.h"
#include "gscoord.h"
#include "gsrop.h"
#include "gspath.h"
#include "gxalloc.h"
#include "gxdevice.h"
#include "gxstate.h"
#include "gdevbbox.h"
#include "pjparse.h"
#include "pgmand.h"
#include "pclver.h"
#include "plmain.h"

#define BUFFER_SIZE 512		/* minimum is 17 */

#ifdef DEBUG
extern  FILE *  gs_debug_out;
#endif

/* Define the table of pointers to initialization data. */
#define init_(init) extern const pcl_init_t init;
#include "pconfig.h"
#undef init_

const pcl_init_t *    pcl_init_table[] = {
#define init_(init) &init,
#include "pconfig.h"
#undef init_
    0
};

/* Built-in symbol set initialization procedure */
extern  bool    pcl_load_built_in_symbol_sets( pcl_state_t * );

/*
 * Define the gstate client procedures.
 */
  private void *
pcl_gstate_client_alloc(
    gs_memory_t *   mem
)
{
    /*
     * We don't want to allocate anything here, but we don't have any way
     * to say we want to share the client data. Since this will only ever
     * be called once, return something random.
     */
    return (void *)1;
}

/*
 * set and get for pcl's target device.  This is the device at the end
 * of the pipeline.  
 */
   void
pcl_set_target_device(pcl_state_t *pcs, gx_device *pdev)
{
    pcs->ptarget_device = pdev;
}

  gx_device *
pcl_get_target_device(pcl_state_t *pcs)
{
    return pcs->ptarget_device;
}

/* NB needs to documentation */
 private void
pcl_set_personality(pcl_state_t *pcs)
{
    /* set up current personality */
    private const struct {
	const char *pjl_pers;
	pcl_personality_t pcl_pers;
    } pjl2pcl_personalities[] = {
	{ "pcl5c", pcl5c },
	{ "pcl5e", pcl5e },
	{ "rtl", rtl },
	{ "hpgl", hpgl }
    };
    int i;
    pcs->personality = pcl5c;
    for (i = 0; i < countof(pjl2pcl_personalities); i++)
	if ( !pjl_compare(pjl_get_envvar(pcs->pjls, "personality"),
			  pjl2pcl_personalities[i].pjl_pers) ) {
	    pcs->personality = pjl2pcl_personalities[i].pcl_pers;
		    break;
	}
}

  private int
pcl_gstate_client_copy_for(
    void *                  to,
    void *                  from,
    gs_state_copy_reason_t  reason
)
{
    return 0;
}

  private void
pcl_gstate_client_free(
    void *          old,
    gs_memory_t *   mem
)
{}

private const gs_state_client_procs pcl_gstate_procs = {
    pcl_gstate_client_alloc,
    0,				/* copy -- superseded by copy_for */
    pcl_gstate_client_free,
    pcl_gstate_client_copy_for
};

/* 
 * Here is the real main program.
 */
  int
main(
    int                 argc,
    char **             argv
)
{
    gs_ref_memory_t *   imem;
#define mem ((gs_memory_t *)imem)
    pl_main_instance_t  inst;
    gs_state *          pgs;
    pcl_state_t *       pcs;
    pcl_parser_state_t  pcl_parser_state;
    hpgl_parser_state_t pgl_parser_state;
    arg_list            args;
    const char *        arg;

    pl_main_init_standard_io();
    /* Initialize the library. */
    gp_init();
    gs_lib_init(gs_stderr);
#ifdef DEBUG
    gs_debug_out = gs_stdout;
#endif

    imem = ialloc_alloc_state((gs_raw_memory_t *)&gs_memory_default, 20000);
    imem->space = 0;		/****** WRONG ******/
    pl_main_init(&inst, mem);
    pl_main_process_options(&inst, &args, argv, argc, PCLVERSION, PCLBUILDDATE);
    /****** SHOULD HAVE A STRUCT DESCRIPTOR ******/
    pcs = (pcl_state_t *)gs_alloc_bytes( mem,
                                          sizeof(pcl_state_t),
    				          "main(pcl_state_t)"
                                          );
    /* start off setting everything to 0.  NB this should not be here
       at all, but I don't have time to analyze the consequences of
       removing it. */
    memset(pcs, 0, sizeof(pcl_state_t));

    /* call once to set up the free list handlers */
    gs_nogc_reclaim(&inst.spaces, true);
    pcl_set_target_device(pcs, inst.device);
    /* Insert a bounding box device so we can detect empty pages. */
    {
        gx_device_bbox *    bdev = gs_alloc_struct_immovable(
                                                    mem,
                                                    gx_device_bbox,
                                                    &st_device_bbox,
    			                            "main(bbox device)"
                                                              );

        gx_device_fill_in_procs(inst.device);
        gx_device_bbox_init(bdev, inst.device);
        inst.device = (gx_device *)bdev;
    }
    pl_main_make_gstate(&inst, &pgs);
    pcl_init_state(pcs, mem);
    gs_state_set_client(pgs, pcs, &pcl_gstate_procs);

    /* PCL no longer uses the graphic library transparency mechanism */
    gs_setsourcetransparent(pgs, false);
    gs_settexturetransparent(pgs, false);

    gs_gsave(pgs);

    gs_erasepage(pgs);
    pcs->client_data = &inst;
    pcs->pgs = pgs;

    /* assume no appletalk configuration routine */
    pcs->configure_appletalk = 0;

    /* initialize pjl */
    pcs->pjls = pjl_process_init(mem);
    pcl_set_personality(pcs);
    pcl_parser_state.hpgl_parser_state = &pgl_parser_state;
    {
	/* Run initialization code. */
	int code = pcl_do_registrations(pcs, &pcl_parser_state);
	if ( code < 0 )
	    exit(code);
    }

    /* glue the initialized gl/2 parser state into pcl's state */
    pcs->parse_data = &pgl_parser_state;
    pcl_do_resets(pcs, pcl_reset_initial);
    pcl_load_built_in_symbol_sets(pcs);

#ifdef DEBUG
    if (gs_debug_c(':'))
        pl_print_usage(mem, &inst, "Start");
#endif

    /* provide a graphic state we can return to */
    pcl_gsave(pcs);

    /* call once more to get rid of any temporary objects */
    gs_nogc_reclaim(&inst.spaces, true);

    while ((arg = arg_next(&args)) != 0) {
        /* Process one input file. */
        FILE *              in = fopen(arg, "rb");
        byte                buf[BUFFER_SIZE];

#define buf_last    (buf + (BUFFER_SIZE - 1))

        int                 len;
        int                 code = 0;
        stream_cursor_read  r;
        bool                in_pjl = true;

#ifdef DEBUG
        if (gs_debug_c(':'))
            dprintf1("%% Reading %s:\n", arg);
#endif

        if (in == 0) {
            fprintf(gs_stderr, "Unable to open %s for reading.\n", arg);
            exit(1);
        }

        r.limit = buf - 1;
        for (;;) {
            if_debug1('i', "[i][file pos=%ld]\n", (long)ftell(in));
            r.ptr = buf - 1;
            len = fread((void *)(r.limit + 1), 1, buf_last - r.limit, in);
            if (len <= 0)
    	        break;
            r.limit += len;
process:
            if (in_pjl) {
                code = pjl_process(pcs->pjls, NULL, &r);
    	        if (code < 0)
    	            break;
    	        else if (code > 0) {
    	            in_pjl = false;
    	            pcl_process_init(&pcl_parser_state);
    	            goto process;
    	        }
    	    } else {
                code = pcl_process(&pcl_parser_state, pcs, &r);
    	        if (code == e_ExitLanguage)
    	            in_pjl = true;
    	        else if (code < 0)
    	            break;
    	    }

            /* Move unread data to the start of the buffer. */
            len = r.limit - r.ptr;
            if (len > 0)
    	        memmove(buf, r.ptr + 1, len);
            r.limit = buf + (len - 1);
        }
#if 0
#ifdef DEBUG
        if (gs_debug_c(':'))
            dprintf3( "Final file position = %ld, exit code = %d, mode = %s\n",
    	              (long)ftell(in) - (r.limit - r.ptr),
                      code,
    	              (in_pjl ? "PJL" : pcs->parse_other ? "HP-GL/2" : "PCL")
                      );
#endif
#endif
        fclose(in);

        /* Read out any status responses. */
        {
            byte    buf[200];
            uint    count;

            while ( (count = pcl_status_read(buf, sizeof(buf), pcs)) != 0 )
                fwrite(buf, 1, count, gs_stdout);
            fflush(gs_stdout);
        }
	gs_nogc_reclaim(&inst.spaces, true);
    }

    /* to help with memory leak detection, issue a reset */
    pcl_do_resets(pcs, pcl_reset_printer);

    /* return to the original graphic state, reclaim memory once more */
    pcl_grestore(pcs);
    gs_nogc_reclaim(&inst.spaces, true);
    pjl_process_destroy(pcs->pjls, mem);
    /* shutdown the parser */
    pcl_parser_shutdown(&pcl_parser_state, pcs->memory);
#ifdef DEBUG
    if ( gs_debug_c(':') ) {
        pl_print_usage(mem, &inst, "Final");
        dprintf1("%% Max allocated = %ld\n", gs_malloc_max);
    }
    if (gs_debug_c('!'))
        debug_dump_memory(imem, &dump_control_default);
#endif

    gs_closedevice(pcl_get_target_device(pcs));
    gs_closedevice(gs_currentdevice(pgs));
    gs_lib_finit(0, 0);
    exit(0);
    /* NOTREACHED */
}
