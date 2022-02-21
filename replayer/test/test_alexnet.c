#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gr_replay.h"
#include "gr_utils.h"
#include "measure.h"

/******** CONFIGURATION ********/
/** #define CONFIG_WITH_PARAM */
/*******************************/

#ifdef CONFIG_WITH_PARAM
	#define INPUT_PATH 		"/shared/GR/alexnet/withparam/"			// with model data
#else
	#define INPUT_PATH		"../recs/alexnet/"						// no model data
#endif

gr_config config = {
	.io_path 	= INPUT_PATH "gr_io_history",
	.pgt_path	= INPUT_PATH "gr_pgt",
	.mc_path	= INPUT_PATH "gr_mem_contents",
	.sas_path	= INPUT_PATH "gr_sync_as",

	.nr_prep_jobs 		= 15,
	.nr_comp_jobs 		= 45,
	.nr_tot_jobs		= 60,

	.test_checkpoint	= 0,
	.is_validate 		= 0,
	.snapshot_path		= INPUT_PATH "gr_reg_snapshot",
};

int main(int argc, char *argv[]) {
	struct replay_context *rctx;
	struct sync_as *input_as, *output_as;
	char *input_buf, *output_buf; 
	uint32_t num_iter = 10;

	k2_measure("startup");
	rctx = gr_rctx_init(&config);
	gr_verification();

#ifdef CONFIG_WITH_PARAM
	/******************* Find input and output mem regions ***********************/
	const char input_magic[8] = {0x3E, 0x0A, 0x4C, 0xC2, 0xEC, 0x51, 0x19, 0x42};
	/** first 8 bytes of input image used when recording stage */
	input_as = gr_find_sync_as(rctx, input_magic, sizeof(char) * 8);

	/******************* Load and place a new input  ***********************/
	FILE *input_fp = fopen(INPUT_PATH "input/input_alexnet", "r");
	xzl_bug_on_msg(!input_fp, "REG input load error");

	input_buf	= (char *) malloc (sizeof(char) * input_as->size);
	fread(input_buf, input_as->size, 1, input_fp);
	fclose(input_fp);
#else
	input_as = rctx->sas[rctx->sas_cnt - 2];
	input_buf	= (char *) malloc (sizeof(char) * input_as->size);
#endif
	/** output region is normally synced at the end of execution */
	output_as = rctx->sas[rctx->sas_cnt - 1];
	output_buf	= (char *) malloc (sizeof(char) * output_as->size);

	k2_measure("startup end");
	k2_measure_flush();

	/************************* run *************************/
	for (uint32_t i = 0; i < num_iter; i++) {
		/** Cleaning before replaying */
		gr_clean_used_atom(rctx);
		memset(output_buf, 0xFF, output_as->size);
		WW("input gm_start = %lx, as_size: %ld", input_as->gm_start, input_as->size);
		gr_copy_to_gpu(rctx, input_as->gm_start, input_buf, input_as->size);
		WW("output gm_start = %lx, as_size: %ld", output_as->gm_start, output_as->size);
		gr_copy_to_gpu(rctx, output_as->gm_start, output_buf, output_as->size);

		WW("gm_start = %lx", input_as->gm_start);
		gr_dump_phys("Input", input_as->pm_start, 256);
		WW("gm_start = %lx", output_as->gm_start);
		gr_dump_phys("Output", output_as->pm_start, 256);

		/*************** Run replay ***************/
		gr_replay_run(rctx);

		/** Get the output */
		gr_copy_from_gpu(rctx, output_as->gm_start, output_buf, output_as->size);
		hex_dump(output_buf, 256);

		/** Cleaning after replaying */
		/** gr_mem_contents_check(rctx, INPUT_PATH "gr_mem_contents"); */
		/** gr_restore_pte(rctx); */
		/** gr_clean_invalid_region(rctx); */
	}

	EE("avg prep time: %u (us)", rctx->tot_prep_time / num_iter);
	EE("avg comp time: %u (us)", rctx->tot_comp_time / num_iter);

	free(input_buf);
	free(output_buf);

	gr_rctx_term(rctx);
	k2_measure_flush();
}
