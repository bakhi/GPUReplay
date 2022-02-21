import sys

def ioctl_run(name):
    f_regs = open(name + "_integrated", "r")

    ts_a = 0
    ts_b = 0

    ts_a, ts_b = ts_b, ts_a
    ts_prev = 0
    ts_cur = 0

    count = 0   # general count
    runtime_count = 0   # runtime ops count
    while True:
        line = f_regs.readline() # get next line
        tmp = line.split()
        if not line: # the end of trace
            break

        if tmp[1] == 'RBUF':
            rbuf_code = tmp[3].replace(',','')
            # IOCTL
            if rbuf_code.startswith('JIN_KBASE_IOCTL_') or rbuf_code.startswith('JIN_KBASE_FOPS_'):

                ts_cur = long(tmp[0])
                if runtime_count > 0:
                    delay = long(tmp[0]) - ts_runtime
                    if runtime_count % 2 == 1: # running delay
                        delay = delay
                        #  print "running delay", delay
                    else:
                        print count, "runtime delay", delay # runtime delay
                #  if runtime_count % 2 == 0: # running delay

    #              else: # user-runtime delay
                    #  print ''
                ts_runtime = ts_cur
                runtime_count += 1

                #  print count + 1, line.replace('\n','')
        count += 1
        if count > 10000: break


# we have three phase of GPU execution
# preparation: prepare GPU context and allocate and map memory; the compilation is not invovled in
# run: the kenrel is executed; especially, after preparation all the weight transform in case of ML network
# close: after all the kernels are done, closing context time which seems not included in the ACL bench test

# NOTE: so the when the user space gets the result? after sync?

def run_skip_prep(name, start_job_num):
    print("start")  
    f_regs = open(name + "_integrated", "r")
#  f_out = open(name + "_calc_delay", "w")

    count = 0
    count_end = 0

    job_num = 1
    job_num_end = 0

### FOPS related
    ts_fops_prev  = 0
    fops_delay    = 0 
    mem_delay     = 0
    mmu_delay     = 0
    jd_done_delay = 0

    tot_gpu_prep_time   = 0
    tot_gpu_close_time = 0

    tot_job_exec_time_prep      = 0 # only actual job exec time from the GPU
    tot_job_exec_time_run       = 0
    tot_ioctl_exec_time_prep    = 0 # include IOCTL interaction
    tot_ioctl_exec_time_run     = 0

    tot_delay           = 0

    ## FOPS delay ##
    tot_fops_delay          = 0
    tot_fops_delay_prep     = 0
    tot_fops_delay_run_prep = 0
    tot_fops_delay_run_run  = 0
    tot_fops_dleay_close    = 0

    ## MEM delay ##
    tot_mem_delay           = 0
    tot_mem_delay_prep      = 0
    tot_mem_delay_run_prep  = 0
    tot_mem_delay_run_run   = 0
    tot_mem_delay_close     = 0

    tot_mmu_delay           = 0
    tot_mmu_delay_prep      = 0
    tot_mmu_delay_run_prep  = 0
    tot_mmu_delay_run_run   = 0
    tot_mmu_delay_close     = 0

    tot_jd_done_delay   = 0

    tot_ioctl_time      = 0

    init_delay = 0

    ## USER delay ##
    uer_start = 0
    user_end = 0
    user_delay = 0

    tot_user_delay_prep     = 0
    tot_user_delay_run_prep = 0
    tot_user_delay_run_run  = 0
    tot_user_delay_close    = 0


    # etc
    tot_delays_until_real_submit_run_prep	= 0
    tot_delays_until_real_submit_run_run	= 0
   
    is_in_user = 0

    while True:
        line = f_regs.readline() # get next line
        tmp = line.split()
        if not line: # the end of trace
            break
        if tmp[1] == 'RBUF':
            rbuf_code = tmp[3].replace(',','')
            if rbuf_code == 'JM_SUBMIT':
                job_num_end += 1
        count_end += 1
        tmp_prev = tmp

    f_regs.seek(0,0)

    print("Number of jobs in the trace: {0}".format(job_num_end))

    while True:
        line = f_regs.readline() # get next line
        tmp = line.split()
        if not line: # the end of trace
            break

        # we assume only one job at a given time
        if tmp[1] == 'RBUF':
            rbuf_code = tmp[3].replace(',','')
            ts_cur    = long(tmp[0])

            if rbuf_code == 'JM_SUBMIT':
                ts_job_submit = ts_cur
    
#                  print("FOPS_DELAY_JOB: {0}".format(fops_delay))
                #  print("MEM_DELAY_JOB: {0}".format(mem_delay))
                #  print("MMU_DELAY_JOB: {0}".format(mmu_delay))
   
#                  if job_num == start_job_num:
                   #  tot_gpu_prep_time = ts_job_submit - ts_gpu_start
                #  elif job_num > start_job_num:
                if job_num > start_job_num:
                    print("TOTAL DELAY between job{0} and job{1}: {2}".format(job_num - 1, job_num, ts_job_submit - ts_job_irq))
                    tot_delay += (ts_job_submit - ts_job_irq) # last JOB IRQ - current submit
                    tot_fops_delay += fops_delay
                    tot_mem_delay += mem_delay
                    tot_mmu_delay += mmu_delay
                    tot_jd_done_delay += jd_done_delay
    
    #              print("EXEC_TIME_TOTAL: {0}".format(tot_exec_time))
                #  print("FOPS_DELAY_TOTAL: {0}".format(tot_fops_delay))
                #  print("MEM_DELAY_TOTAL: {0}".format(tot_mem_delay))
                #  print("MMU_DELAY_TOTAL: {0}".format(tot_mmu_delay))
   

                jd_done_delay   = 0

                if job_num >= start_job_num:
                    print("delays until real submit {0} (ns)".format(ts_job_submit - ts_ioctl_job_submit))
                    tot_delays_until_real_submit_run_run += (ts_job_submit - ts_ioctl_job_submit)
    
                print("-------------------- JOB SUBMIT {0} --------------------".format(job_num))
    
            elif rbuf_code == 'JM_IRQ':
                ts_job_irq = long(tmp[0])
                if job_num < start_job_num:
                    tot_job_exec_time_prep += (ts_job_irq - ts_job_submit)
                else:
                    tot_job_exec_time_run += (ts_job_irq - ts_job_submit)
                print("-------------------- JOB {0} DONE ({1} ns) -------------".format(job_num, ts_job_irq - ts_job_submit))
                job_num += 1
                if is_in_user != 1 :
                    print("JOB is executed beyond IOCTL_JOB_SUBMIT")
                    user_delay -= (ts_job_irq - ts_job_submit)
            elif rbuf_code == 'JM_JOB_DONE':
                ts_job_irq_done = long(tmp[0])

                #  print("\tJOB_IRQ delay: {0}".format(ts_job_irq_done - ts_job_irq))

    
            elif rbuf_code == 'CORE_GPU_IRQ':
                ts_gpu_irq = long(tmp[0])
            elif rbuf_code == 'CORE_GPU_IRQ_DONE':
                ts_gpu_irq_done = long(tmp[0])
                #  print("\tGPU_IRQ delay: {0}".format(ts_gpu_irq_done - ts_gpu_irq))
            elif rbuf_code == 'JD_DONE_WORKER':
                ts_jd_done_worker_start = long(tmp[0])
            elif rbuf_code == 'JD_DONE_WORKER_END':
                ts_jd_done_worker_end = long(tmp[0])
                jd_done_delay += (ts_jd_done_worker_end - ts_jd_done_worker_start)
                if is_in_user != 1 :
                    print("JOB DONE_WORKER is executed beyond IOCTL_JOB_SUBMIT")
                    user_delay -= (ts_jd_done_worker_end - ts_jd_done_worker_start)

                #  print("\tJD_WORKER delay: {0}".format(ts_jd_done_worker_end - ts_jd_done_worker_start))
    
            # User-space interaction

            ####################### IOCTL ####################### 
            elif rbuf_code.startswith('JIN_KBASE_IOCTL_'):
                if rbuf_code != 'JIN_KBASE_IOCTL_DONE' and user_end != 0:
                    print("USER DELAY: {0}".format(ts_cur - user_end))
                    user_delay += (ts_cur - user_end)
                    is_in_user = 1
                else:
                    user_end = ts_cur
                    is_in_user = 0

                if rbuf_code == 'JIN_KBASE_IOCTL_VERSION_CHECK':
                    ts_ioctl_version_check = ts_cur
                elif rbuf_code == 'JIN_KBASE_IOCTL_SET_FLAGS':
                    ts_ioctl_set_flags = ts_cur
                elif rbuf_code == 'JIN_KBASE_IOCTL_GET_GPUPROPS':
                    ts_ioctl_get_gpuprops = ts_cur
                elif rbuf_code == 'JIN_KBASE_IOCTL_GET_CONTEXT_ID':
                    ts_ioctl_get_context_id = ts_cur
                elif rbuf_code == 'JIN_KBASE_IOCTL_MEM_ALLOC':
                    ts_ioctl_mem_alloc = ts_cur
                elif rbuf_code == 'JIN_KBASE_IOCTL_MEM_SYNC':
                    ts_ioctl_mem_sync = ts_cur
                elif rbuf_code == 'JIN_KBASE_IOCTL_MEM_PROFILE_ADD':
                    ts_ioctl_mem_profile_add = ts_cur
                elif rbuf_code == 'JIN_KBASE_IOCTL_MEM_COMMIT':
                    ts_ioctl_mem_commit = ts_cur
                elif rbuf_code == 'JIN_KBASE_IOCTL_JOB_SUBMIT':
                    ts_ioctl_job_submit = ts_cur
                    if job_num == 1:
                        ts_job_start_prep = ts_cur
                        tot_gpu_prep_time = ts_ioctl_job_submit - ts_gpu_start
                    if job_num == start_job_num:
                        ts_job_start_run = ts_cur
                        user_end    = 0
                        user_delay  = 0
                        fops_delay  = 0
                        mem_delay   = 0
                        mmu_delay   = 0

                    print("================================== [IOCTL] JOB {0} SUBMIT ".format(job_num))

                elif rbuf_code == 'JIN_KBASE_IOCTL_POST_TERM':
                    ts_ioctl_post_term= ts_cur
                    tot_ioctl_exec_time_run = ts_ioctl_post_term - ts_job_start_run
                    tot_fops_delay_run_run  += fops_delay
                    tot_mem_delay_run_run   += mem_delay
                    tot_mmu_delay_run_run   += mmu_delay
                    tot_user_delay_run_run  += user_delay
                elif rbuf_code == 'JIN_KBASE_IOCTL_DONE':
                    done_value = int(tmp[4].split(',')[4], 16)
                    if done_value == 0x0:   # IOCTL_VERSION_CHECK
                        ts_ioctl_version_check_end = ts_cur
                        tot_ioctl_time += (ts_cur - ts_ioctl_version_check)
                    elif done_value == 0x1: # IOCTL_SET_FLAGS
                        ts_ioctl_set_flags_end = ts_cur
                        tot_ioctl_time += (ts_cur - ts_ioctl_set_flags)
                    elif done_value == 0x2:   # IOCTL_JOB_SUBMIT
                        ts_ioctl_job_submit_end = ts_cur
                        tot_ioctl_time += (ts_cur - ts_ioctl_job_submit)
                        print("====================================== [IOCTL] JOB {0} SUBMIT DONE {1}".format(job_num, ts_ioctl_job_submit_end - ts_ioctl_job_submit))

                        #### NOTE Calculate delay here ####
                        if job_num == start_job_num:
                            tot_ioctl_exec_time_prep = ts_ioctl_job_submit_end - ts_job_start_prep
#                          elif job_num == job_num_end:
                            #  tot_ioctl_exec_time_run = ts_ioctl_job_submit_end - ts_job_start_run

                        if job_num < start_job_num :
                            tot_fops_delay_run_prep += fops_delay
                            tot_mem_delay_run_prep  += mem_delay
                            tot_mmu_delay_run_prep  += mmu_delay
                            tot_user_delay_run_prep  += user_delay
                        else:
                            tot_fops_delay_run_run  += fops_delay
                            tot_mem_delay_run_run   += mem_delay
                            tot_mmu_delay_run_run   += mmu_delay
                            tot_user_delay_run_run  += user_delay
                            print("FOPS delay: {0} MEM delay: {1} MMU delay: {2} USER delay: {3}".format(fops_delay, mem_delay, mmu_delay, user_delay))
                        print("-------------- [SUB TOTAL] user delay: {0}".format(user_delay))
                        user_delay  = 0
                        fops_delay  = 0
                        mem_delay   = 0
                        mmu_delay   = 0

                    elif done_value == 0x3:   # IOCTL_GET_GPUPROPS
                        ts_ioctl_get_gpuprops_end = ts_cur
                        tot_ioctl_time += (ts_cur - ts_ioctl_version_check)
                    elif done_value == 0x4:   # IOCTL_POST_TERM
                        ts_ioctl_post_term_end = ts_cur
                        tot_ioctl_time += (ts_cur - ts_ioctl_post_term)
                    elif done_value == 0x5:   # IOCTL_MEM_ALLOC
                        ts_ioctl_mem_alloc_end = ts_cur
                        delay = ts_ioctl_mem_alloc_end - ts_ioctl_mem_alloc
                        print("\t[IOCTL] mem alloc: {0}".format(delay))
                        mem_delay += delay
                        tot_ioctl_time += (ts_cur - ts_ioctl_mem_alloc)
                    elif done_value == 0xd:  # IOCTL_MEM_SYNC
                        ts_ioctl_mem_sync_end = ts_cur
                        delay = ts_ioctl_mem_sync_end - ts_ioctl_mem_sync
                        mem_delay += delay
                        tot_ioctl_time += (ts_cur - ts_ioctl_mem_sync)
                    elif done_value == 0x10:  # IOCTL_GET_CONTEXT_ID
                        ts_ioctl_get_context_id_end = ts_cur
                        tot_ioctl_time += (ts_cur - ts_ioctl_get_context_id)
                    elif done_value == 0x13:  # IOCTL_MEM_COMMIT
                        ts_ioctl_mem_commit_end = ts_cur
                        tot_ioctl_time += (ts_cur - ts_ioctl_mem_commit)
                        print("\t[IOCTL] mem commit: {0}".format(ts_ioctl_mem_commit_end - ts_ioctl_mem_commit))
                    elif done_value == 0x19:  # IOCTL_MEM_PROFILE_ADD
                        ts_ioctl_mem_profile_add_end = ts_cur
                        tot_ioctl_time += (ts_cur - ts_ioctl_mem_profile_add)
                        
            ##################################################### 

            # FOPS
    
            # sometimes no time difference between FOPS and FOPS_DONE (e.g. POLL)
            # NOTE we should handle this issue
            # method: read two FOPS value then calculate absolut different between them
            # assumtion: FOPS is done by one by one; not concurrent running
            elif rbuf_code.startswith('JIN_KBASE_FOPS_'):
                ts_fops = ts_cur
                if rbuf_code != 'JIN_KBASE_FOPS_DONE':
                    fops_op = rbuf_code
                else:
                    fops_done_value = int(tmp[4].split(',')[4], 16)

                if rbuf_code == 'JIN_KBASE_FOPS_OPEN' :
                    ts_gpu_start = long(tmp[0])

                if ts_fops_prev == 0:  # no prior fops
                    ts_fops_prev = ts_fops
                    if user_end != 0:
                        print("USER DELAY: {0}".format(ts_cur - user_end))
                        user_delay += (ts_fops - user_end)
                        is_in_user = 1
                else:  # there is prior fops
                    user_end = ts_cur
                    is_in_user = 0
                    delay = abs(ts_fops - ts_fops_prev)
                    if fops_op == 'JIN_KBASE_FOPS_GET_UNMAPPED_AREA':
                        print("\t[FOPS] get_unmapped: {0}".format(delay))
                        assert fops_done_value == 0x6
                        mem_delay += delay
                    elif fops_op == 'JIN_KBASE_FOPS_MMAP':
                        print("\t[FOPS] mmap: {0}".format(delay))
                        assert fops_done_value == 0x4
                        mem_delay += delay
                    elif fops_op == 'JIN_KBASE_FOPS_READ':
                        print("\t[FOPS] read: {0}".format(delay))
                        assert fops_done_value == 0x2
                        fops_delay += delay
                    elif fops_op == 'JIN_KBASE_FOPS_POLL':
                        print("\t[FOPS] poll: {0}".format(delay))
                        if fops_done_value != 0x3:
                            print(tmp)
                            print("ERROR line num:{0}".format(count))
                        #  assert fops_done_value == 0x3    # FIXME sometimes multiple threads work for generating FOPS trace
                        fops_delay += delay

                    ts_fops_prev = 0
                    fops_op = ''


#                  if rbuf_code != 'JIN_KBASE_FOPS_DONE' and user_end !=0:
                #      print("USER DELAY: {0}".format(ts_cur - user_end))
                #      user_delay += (ts_cur - user_end)
                #  else:
                #      user_end = ts_cur
                #
                #  if rbuf_code == 'JIN_KBASE_FOPS_OPEN' :
                #      ts_gpu_start = long(tmp[0])
                #
                #  ts_fops = long(tmp[0])
                #  if ts_fops_prev != 0:
                #      delay = ts_fops - ts_fops_prev
                #      value = int(tmp[4].split(',')[4], 16)
                #      if value == 0x2:    # FOPS_READ
                #          print("\t[FOPS] read: {0}".format(delay))
                #      elif value == 0x3:    # FOPS_POLL
                #          print("\t[FOPS] poll: {0}".format(delay))
                #      if value == 0x4:    #FOPS_MMAP
                #          print("\t[FOPS] mmap: {0}".format(delay))
                #          mem_delay += delay
                #      elif value == 0x6:  # FOPS_GET_UNMAPPED_AREA
                #          print("\t[FOPS] get_unmapped: {0}".format(delay))
                #          mem_delay += delay
                #      else:
                #          #  print("\t[FOPS] poll or read delay: {0}".format(delay))
                #          fops_delay += delay
                #      ts_fops_prev = 0
                #  else:
                    #  ts_fops_prev = ts_fops
    
    #          elif rbuf_code.startswith('JIN_KBASE_FOPS_'):
                #  if rbuf_code == 'JIN_KBASE_FOPS_GET_UNMAPPED_AREA':
                #      ts_fops_get_unmapped = long(tmp[0])
                #  elif rbuf_code == 'JIN_KBASE_FOPS_MMAP':
                #      ts_fops_mmap = long(tmp[0])
                #  elif rbuf_code == 'JIN_KBASE_FOPS_READ':
                #      ts_fops_read = long(tmp[0])
                #  elif rbuf_code == 'JIN_KBASE_FOPS_POLL':
                #      ts_fops_poll = long(tmp[0])
                #  elif rbuf_code == 'JIN_KBASE_FOPS_DONE':
                #      done_value = int(tmp[4].split(',')[4], 16)
                #      delay = 0
                #      if done_value == 0x2:
                #          delay = long(tmp[0]) - ts_fops_read
                #          fops_delay += delay
                #          print("\t[FOPS] read: {0}".format(delay))
                #      elif done_value == 0x3:
                #          delay = long(tmp[0]) - ts_fops_poll
                #          fops_delay += delay
                #          print("\t[FOPS] poll: {0}".format(delay))
                #      elif done_value == 0x4:
                #          delay = long(tmp[0]) - ts_fops_mmap
                #          print("\t[FOPS] mmap: {0}".format(delay))
                #          mem_delay += delay
                #      elif done_value == 0x6:
                #          delay = long(tmp[0]) - ts_fops_get_unmapped
                #          print("\t[FOPS] get_unmapped: {0}".format(delay))
                        #  mem_delay += delay

            # MMU update
            elif rbuf_code.startswith('JIN_MMU_OP_START'):
                ts_mmu_update_start = ts_cur
            elif rbuf_code.startswith('JIN_MMU_OP_DONE'):   # includes both AS_COMMAND_FLUSH_MEM and FLUSH_PT
                ts_mmu_update_done = ts_cur
                delay = ts_mmu_update_done - ts_mmu_update_start
                mmu_delay += delay
                #  print("\t[MMU] update: {0}".format(delay))


            ####################### VM_OPS #######################
            elif rbuf_code.startswith('JIN_KBASE_VM_OPS_'):
                if rbuf_code == 'JIN_KBASE_VM_OPS_FAULT': 
                    ts_vm_ops_fault = ts_cur
                elif rbuf_code == 'JIN_KBASE_VM_OPS_DONE':
                    vm_ops_done_value = int(tmp[4].split(',')[4], 16)
                    if vm_ops_done_value == 0x2:
                        ts_vm_ops_fault_end = ts_cur
                        mem_delay += (ts_vm_ops_fault_end - ts_vm_ops_fault)
                        #  print("********************************* VM fault delay {0}".format(ts_vm_ops_fault_end - ts_vm_ops_fault))
                        

        elif tmp[1] == 'IO':
            io_type = tmp[9]

            if io_type == 'MEM_MGMT':
                # when the L2 is about to slow down, the driver turned off L2 and tiler core
                # Later, the driver restore MMU from next or previous context
                # jin: does it actually result from power state change?
                if tmp[11].startswith('MMU_AS') and tmp[13] == 'AS_TRANSCFG_LO':
                #  if tmp[11] == 'MMU_AS0' and tmp[13] == 'AS_TRANSCFG_LO':
                    ts_restore_mmu = long(tmp[0])
                elif tmp[11].startswith('MMU_AS') and tmp[13] == 'AS_COMMAND' and tmp[16] == 'AS_COMMAND_UPDATE':
                #  elif tmp[11] == 'MMU_AS7' and tmp[13] == 'AS_COMMAND' and tmp[16] == 'AS_COMMAND_UPDATE':
                    delay = long(tmp[0]) - ts_restore_mmu
                    #  print("\t[MMU] restore context: {0}".format(delay))   # restore context when power is on
                    mmu_delay += delay
#          elif tmp[1] == 'AUX':
        #      continue
        #  elif tmp[1] == 'TL':
            #  continue
    
        count += 1

        if count == count_end:
            tot_gpu_close_time = long(tmp[0]) - ts_ioctl_post_term_end

    #  if count > 2000: break
#      reg_io = []
    #  reg_io.append(tmp[0])	# timestamp
    #  reg_io.append(tmp[4])	# R/W
    #  reg_io.append(tmp[6])	# reg
    #  reg_io.append(tmp[8])	# val
    #  f_out.write(",".join(reg_io) + '\n')
  #  f_regs.close()
    print("Done")

    print("========== [Phase 1] Preparation ==========")
    print("TOTAL general prep time (until first IOCTL_JOB_SUBMIT): {0} (ns)".format(tot_gpu_prep_time))
    print("")

    print("========== [Phase 2] Run ==================")
    print("TOTAL job_exec_time_prep: {0} (ns)".format(tot_job_exec_time_prep))
    print("TOTAL ioctl_exec_time_prep: {0} (ns)".format(tot_ioctl_exec_time_prep))

    print("[DELAY FOPS] tot_fops_delay_run_prep: {0} (ns)".format(tot_fops_delay_run_prep))
    print("[DELAY MEM] tot_mem_delay_run_prep: {0} (ns)".format(tot_mem_delay_run_prep))
    print("[DELAY MMU] tot_mmu_delay_run_prep: {0} (ns)".format(tot_mmu_delay_run_prep))
    print("[DELAY USER] tot_user_delay_run_prep: {0} (ns)".format(tot_user_delay_run_prep))
    print("")

    print("TOTAL job_exec_time_run: {0} (ns)".format(tot_job_exec_time_run))
    print("TOTAL ioctl_exec_time_run: {0} (ns)".format(tot_ioctl_exec_time_run))

    print("[DELAY FOPS] tot_fops_delay_run_run: {0} (ns)".format(tot_fops_delay_run_run))
    print("[DELAY MEM] tot_mem_delay_run_run: {0} (ns)".format(tot_mem_delay_run_run))
    print("[DELAY MMU] tot_mmu_delay_run_run: {0} (ns)".format(tot_mmu_delay_run_run))
    print("[DELAY USER] tot_user_delay_run_run: {0} (ns)".format(tot_user_delay_run_run))
    print("[DELAY UNTIL REAL SUBMIT] delay_bet_ioctl_and_exec: {0} (ns)".format(tot_delays_until_real_submit_run_run))
    print("")

    print("========== [Phase 3] Close ================")
    print("TOTAL general close time (after post_term is done): {0} (ns)".format(tot_gpu_close_time))

    print("TOTAL delay : {0}".format(tot_delay))
    print("TOTAL IOCTL time : {0}".format(tot_ioctl_time))
    print("FOPS delay : {0} (ns) => {1}% removable".format(tot_fops_delay, round(float(tot_fops_delay) / tot_delay, 4) * 100))
    print("MEM delay : {0} (ns) => {1}% removable".format(tot_mem_delay, round(float(tot_mem_delay) / tot_delay, 4) * 100))
    print("MMU delay : {0} (ns) => {1}% removable".format(tot_mmu_delay, round(float(tot_mmu_delay) / tot_delay, 4) * 100))
    print("JD_DONE delay : {0} (ns) => {1}% removable".format(tot_jd_done_delay, round(float(tot_jd_done_delay) / tot_delay, 4) * 100))

if __name__ == "__main__" :
    if len(sys.argv) != 2:
        print ("too many or no arguments")
    #  run(sys.argv[1])
    #  ioctl_run(sys.argv[1])

    # start job num of ML inference
    # MNIST     6
    # AlexNet   16
    # MobileNet 30
    # ResNet12  26

    run_skip_prep(sys.argv[1], 26)
