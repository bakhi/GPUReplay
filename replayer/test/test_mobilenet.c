#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gr_replay.h"
#include "gr_utils.h"
#include "measure.h"

#define INPUT_PATH 		"../recs/mobilenet/"

gr_config config = {
	.io_path    = INPUT_PATH "gr_io_history",
	.pgt_path   = INPUT_PATH "gr_pgt",
	.mc_path    = INPUT_PATH "gr_mem_contents",
	.sas_path   = INPUT_PATH "gr_sync_as",

	.nr_prep_jobs   	= 50,
	.nr_comp_jobs   	= 54,
	.nr_tot_jobs    	= 104,
	.test_checkpoint  	= 0,
	.is_validate    	= 0,
};

int main(int argc, char *argv[]) {
	struct replay_context *rctx;
	struct sync_as *input_as, *output_as;
	char *input_buf, *output_buf;
	uint32_t num_iter = 10;

	k2_measure("startup");
	rctx = gr_rctx_init(&config);
	gr_verification();

	/******************* Load and place a new input  ***********************/
	/** should find the input address with magic */
	input_as = rctx->sas[rctx->sas_cnt - 2];
	output_as = rctx->sas[rctx->sas_cnt - 1];

	input_buf 	= (char *) malloc (sizeof(char) * input_as->size);
	output_buf 	= (char *) malloc (sizeof(char) * output_as->size);

	WW("gm_start = %lx", input_as->gm_start);
	gr_dump_phys("Input", input_as->pm_start, 256);
	WW("gm_start = %lx", output_as->gm_start);
	gr_dump_phys("Output", output_as->pm_start, 256);

	k2_measure("startup end");
	k2_measure_flush();
	/************************* run *************************/
	for (int i = 0; i < num_iter; i++) {
		/** Cleaning before replaying */
		gr_clean_used_atom(rctx);
		memset(output_buf, 0xFF, output_as->size);
		gr_copy_to_gpu(rctx, input_as->gm_start, input_buf, input_as->size);
		gr_copy_to_gpu(rctx, output_as->gm_start, output_buf, output_as->size);

		/*************** Run replay ***************/
		gr_replay_run(rctx);

		/* Get the output */
		gr_copy_from_gpu(rctx, output_as->gm_start, output_buf, output_as->size);
		hex_dump(output_buf, 256);

		/** Cleaning after replaying */
		/** gr_mem_contents_check(rctx, INPUT_PATH "gr_mem_contents"); */
		/** gr_restore_pte(rctx); */
		/** gr_clean_invalid_region(rctx); */
	}

	EE("avg prep time: %u (us)", rctx->tot_prep_time / num_iter);
	EE("avg comp time: %u (us)", rctx->tot_comp_time / num_iter);

	gr_rctx_term(rctx);
	k2_measure_flush();
}
