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

def run(name):
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
    tot_gpu_finish_time = 0
    tot_exec_time       = 0
    tot_delay           = 0
    tot_fops_delay      = 0
    tot_mem_delay       = 0
    tot_mmu_delay       = 0
    tot_jd_done_delay   = 0

    init_delay = 0

    
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

    f_regs.seek(0,0)

    while True:
        line = f_regs.readline() # get next line
        tmp = line.split()
        if not line: # the end of trace
            break

        # we assume only one job at a given time
        if tmp[1] == 'RBUF':
            rbuf_code = tmp[3].replace(',','')

            if rbuf_code == 'JM_SUBMIT':
                ts_job_submit = long(tmp[0])
    
                print("FOPS_DELAY_JOB: {0}".format(fops_delay))
                print("MEM_DELAY_JOB: {0}".format(mem_delay))
                print("MMU_DELAY_JOB: {0}".format(mmu_delay))
   
                if job_num == 1:
                   tot_gpu_prep_time = long(tmp[0]) - ts_gpu_start
                elif job_num > 1:
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
    
                fops_delay      = 0
                mem_delay       = 0
                mmu_delay       = 0
                jd_done_delay   = 0
    
                print("-------------------- JOB SUBMIT {0} --------------------".format(job_num))
    
                #  print(line.replace('\n',''))
            elif rbuf_code == 'JM_IRQ':
                ts_job_irq = long(tmp[0])
                tot_exec_time += (ts_job_irq - ts_job_submit)
                print("-------------------- JOB DONE {0} ----------------------".format(job_num))
                print("#### JOB {0} exec time: {1}".format(job_num, ts_job_irq - ts_job_submit))
                job_num += 1
            elif rbuf_code == 'JM_JOB_DONE':
                ts_job_irq_done = long(tmp[0])
                print("\tJOB_IRQ delay: {0}".format(ts_job_irq_done - ts_job_irq))
                if job_num == job_num_end:
                    ts_gpu_end = long(tmp[0])   ## when the last job is done
    
            elif rbuf_code == 'CORE_GPU_IRQ':
                ts_gpu_irq = long(tmp[0])
            elif rbuf_code == 'CORE_GPU_IRQ_DONE':
                ts_gpu_irq_done = long(tmp[0])
                print("\tGPU_IRQ delay: {0}".format(ts_gpu_irq_done - ts_gpu_irq))
            elif rbuf_code == 'JD_DONE_WORKER':
                ts_jd_done_worker_start = long(tmp[0])
            elif rbuf_code == 'JD_DONE_WORKER_END':
                ts_jd_done_worker_end = long(tmp[0])
                jd_done_delay += (ts_jd_done_worker_end - ts_jd_done_worker_start)
                print("\tJD_WORKER delay: {0}".format(ts_jd_done_worker_end - ts_jd_done_worker_start))
    
            # User-space interaction
            # IOCTL - Mem
            elif rbuf_code.startswith('JIN_KBASE_IOCTL_'):
                if rbuf_code == 'JIN_KBASE_IOCTL_MEM_ALLOC':
                    ts_ioctl_mem_alloc = long(tmp[0])
                elif rbuf_code == 'JIN_KBASE_IOCTL_DONE':
                    done_value = int(tmp[4].split(',')[4], 16)
                    if done_value == 0x5:
                        delay = long(tmp[0]) - ts_ioctl_mem_alloc
                        ts_ioctl_mem_alloc_end = long(tmp[0])
                        print("\t[IOCTL] mem alloc: {0}".format(delay))
                        mem_delay += delay
    
            # FOPS
    
            elif rbuf_code.startswith('JIN_KBASE_FOPS_'):
                if rbuf_code == 'JIN_KBASE_FOPS_OPEN' :
                    ts_gpu_start = long(tmp[0])

                ts_fops = long(tmp[0])
                if ts_fops_prev != 0:
                    delay = ts_fops - ts_fops_prev
                    value = int(tmp[4].split(',')[4], 16)
                    if value == 0x4:
                        print("\t[FOPS] mmap: {0}".format(delay))
                        mem_delay += delay
                    elif value == 0x6:
                        print("\t[FOPS] get_unmapped: {0}".format(delay))
                        mem_delay += delay
                    else:
                        #  print("\t[FOPS] poll or read delay: {0}".format(delay))
                        fops_delay += delay
                    ts_fops_prev = 0
                else: ts_fops_prev = ts_fops
    
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
                ts_mmu_update = long(tmp[0])
            elif rbuf_code.startswith('JIN_MMU_OP_DONE'):
                delay = long(tmp[0]) - ts_mmu_update
                mmu_delay += delay
                print("\t[MMU] update: {0}".format(delay))

        elif tmp[1] == 'IO':
            io_type = tmp[9]

            if io_type == 'MEM_MGMT':
                # when the L2 is about to slow down, the driver turned off L2 and tiler core
                # Later, the driver restore MMU from next or previous context
                if tmp[11].startswith('MMU_AS') and tmp[13] == 'AS_TRANSCFG_LO':
                #  if tmp[11] == 'MMU_AS0' and tmp[13] == 'AS_TRANSCFG_LO':
                    ts_restore_mmu = long(tmp[0])
                elif tmp[11].startswith('MMU_AS') and tmp[13] == 'AS_COMMAND' and tmp[16] == 'AS_COMMAND_UPDATE':
                #  elif tmp[11] == 'MMU_AS7' and tmp[13] == 'AS_COMMAND' and tmp[16] == 'AS_COMMAND_UPDATE':
                    delay = long(tmp[0]) - ts_restore_mmu
                    print("\t[MMU] restore context: {0}".format(delay))   # restore context when power is on
                    mmu_delay += delay
#          elif tmp[1] == 'AUX':
        #      continue
        #  elif tmp[1] == 'TL':
            #  continue
    
        count += 1

        if count == count_end:
            tot_gpu_finish_time = long(tmp[0]) - ts_gpu_end

    #  if count > 2000: break
#      reg_io = []
    #  reg_io.append(tmp[0])	# timestamp
    #  reg_io.append(tmp[4])	# R/W
    #  reg_io.append(tmp[6])	# reg
    #  reg_io.append(tmp[8])	# val
    #  f_out.write(",".join(reg_io) + '\n')
  #  f_regs.close()
    print("Done")

    print("TOTAL preparation time: {0} (ns)".format(tot_gpu_prep_time))
    print("TOTAL finish time: {0} (ns)".format(tot_gpu_finish_time))
    print("TOTAL exec time: {0} (ns)".format(tot_exec_time))
    print("TOTAL delay : {0}".format(tot_delay))
    print("FOPS delay : {0} (ns) => {1}% removable".format(tot_fops_delay, round(float(tot_fops_delay) / tot_delay, 4) * 100))
    print("MEM delay : {0} (ns) => {1}% removable".format(tot_mem_delay, round(float(tot_mem_delay) / tot_delay, 4) * 100))
    print("MMU delay : {0} (ns) => {1}% removable".format(tot_mmu_delay, round(float(tot_mmu_delay) / tot_delay, 4) * 100))
    print("JD_DONE delay : {0} (ns) => {1}% removable".format(tot_jd_done_delay, round(float(tot_jd_done_delay) / tot_delay, 4) * 100))

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
    tot_gpu_finish_time = 0
    tot_exec_time       = 0
    tot_delay           = 0
    tot_fops_delay      = 0
    tot_mem_delay       = 0
    tot_mmu_delay       = 0
    tot_jd_done_delay   = 0

    init_delay = 0

    
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

    f_regs.seek(0,0)

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
    
                print("FOPS_DELAY_JOB: {0}".format(fops_delay))
                print("MEM_DELAY_JOB: {0}".format(mem_delay))
                print("MMU_DELAY_JOB: {0}".format(mmu_delay))
   
                if job_num == start_job_num:
                   tot_gpu_prep_time = ts_job_submit - ts_gpu_start
                elif job_num > start_job_num:
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
    
                fops_delay      = 0
                mem_delay       = 0
                mmu_delay       = 0
                jd_done_delay   = 0
    
                print("-------------------- JOB SUBMIT {0} --------------------".format(job_num))
    
                #  print(line.replace('\n',''))
            elif rbuf_code == 'JM_IRQ':
                ts_job_irq = long(tmp[0])
                tot_exec_time += (ts_job_irq - ts_job_submit)
                print("-------------------- JOB DONE {0} ----------------------".format(job_num))
                print("#### JOB {0} exec time: {1}".format(job_num, ts_job_irq - ts_job_submit))
                job_num += 1
            elif rbuf_code == 'JM_JOB_DONE':
                ts_job_irq_done = long(tmp[0])
                print("\tJOB_IRQ delay: {0}".format(ts_job_irq_done - ts_job_irq))
                if job_num == job_num_end:
                    ts_gpu_end = long(tmp[0])   ## when the last job is done
    
            elif rbuf_code == 'CORE_GPU_IRQ':
                ts_gpu_irq = long(tmp[0])
            elif rbuf_code == 'CORE_GPU_IRQ_DONE':
                ts_gpu_irq_done = long(tmp[0])
                print("\tGPU_IRQ delay: {0}".format(ts_gpu_irq_done - ts_gpu_irq))
            elif rbuf_code == 'JD_DONE_WORKER':
                ts_jd_done_worker_start = long(tmp[0])
            elif rbuf_code == 'JD_DONE_WORKER_END':
                ts_jd_done_worker_end = long(tmp[0])
                jd_done_delay += (ts_jd_done_worker_end - ts_jd_done_worker_start)
                print("\tJD_WORKER delay: {0}".format(ts_jd_done_worker_end - ts_jd_done_worker_start))
    
            # User-space interaction
            # IOCTL - Mem
            elif rbuf_code.startswith('JIN_KBASE_IOCTL_'):
                if rbuf_code == 'JIN_KBASE_IOCTL_MEM_ALLOC':
                    ts_ioctl_mem_alloc_start = ts_cur
                elif rbuf_code == 'JIN_KBASE_IOCTL_JOB_SUBMIT':
                    ts_ioctl_job_submit = ts_cur
                elif rbuf_code == 'JIN_KBASE_IOCTL_DONE':
                    done_value = int(tmp[4].split(',')[4], 16)
                    if done_value == 0x2:
                        ts_ioctl_job_submit_end = ts_cur
                    if done_value == 0x5:
                        ts_ioctl_mem_alloc_end = ts_cur
                        delay = ts_ioctl_mem_alloc_end - ts_ioctl_mem_alloc_start
                        print("\t[IOCTL] mem alloc: {0}".format(delay))
                        mem_delay += delay
    
            # FOPS
    
            elif rbuf_code.startswith('JIN_KBASE_FOPS_'):
                if rbuf_code == 'JIN_KBASE_FOPS_OPEN' :
                    ts_gpu_start = long(tmp[0])

                ts_fops = long(tmp[0])
                if ts_fops_prev != 0:
                    delay = ts_fops - ts_fops_prev
                    value = int(tmp[4].split(',')[4], 16)
                    if value == 0x4:
                        print("\t[FOPS] mmap: {0}".format(delay))
                        mem_delay += delay
                    elif value == 0x6:
                        print("\t[FOPS] get_unmapped: {0}".format(delay))
                        mem_delay += delay
                    else:
                        #  print("\t[FOPS] poll or read delay: {0}".format(delay))
                        fops_delay += delay
                    ts_fops_prev = 0
                else: ts_fops_prev = ts_fops
    
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
            elif rbuf_code.startswith('JIN_MMU_OP_DONE'):
                ts_mmu_update_done = ts_cur
                delay = ts_mmu_update_done - ts_mmu_update_start
                mmu_delay += delay
                print("\t[MMU] update: {0}".format(delay))

        elif tmp[1] == 'IO':
            io_type = tmp[9]

            if io_type == 'MEM_MGMT':
                # when the L2 is about to slow down, the driver turned off L2 and tiler core
                # Later, the driver restore MMU from next or previous context
                if tmp[11].startswith('MMU_AS') and tmp[13] == 'AS_TRANSCFG_LO':
                #  if tmp[11] == 'MMU_AS0' and tmp[13] == 'AS_TRANSCFG_LO':
                    ts_restore_mmu = long(tmp[0])
                elif tmp[11].startswith('MMU_AS') and tmp[13] == 'AS_COMMAND' and tmp[16] == 'AS_COMMAND_UPDATE':
                #  elif tmp[11] == 'MMU_AS7' and tmp[13] == 'AS_COMMAND' and tmp[16] == 'AS_COMMAND_UPDATE':
                    delay = long(tmp[0]) - ts_restore_mmu
                    print("\t[MMU] restore context: {0}".format(delay))   # restore context when power is on
                    mmu_delay += delay
#          elif tmp[1] == 'AUX':
        #      continue
        #  elif tmp[1] == 'TL':
            #  continue
    
        count += 1

        if count == count_end:
            tot_gpu_finish_time = long(tmp[0]) - ts_gpu_end

    #  if count > 2000: break
#      reg_io = []
    #  reg_io.append(tmp[0])	# timestamp
    #  reg_io.append(tmp[4])	# R/W
    #  reg_io.append(tmp[6])	# reg
    #  reg_io.append(tmp[8])	# val
    #  f_out.write(",".join(reg_io) + '\n')
  #  f_regs.close()
    print("Done")

    print("TOTAL preparation time: {0} (ns)".format(tot_gpu_prep_time))
    print("TOTAL finish time: {0} (ns)".format(tot_gpu_finish_time))
    print("TOTAL exec time: {0} (ns)".format(tot_exec_time))
    print("TOTAL delay : {0}".format(tot_delay))
    print("FOPS delay : {0} (ns) => {1}% removable".format(tot_fops_delay, round(float(tot_fops_delay) / tot_delay, 4) * 100))
    print("MEM delay : {0} (ns) => {1}% removable".format(tot_mem_delay, round(float(tot_mem_delay) / tot_delay, 4) * 100))
    print("MMU delay : {0} (ns) => {1}% removable".format(tot_mmu_delay, round(float(tot_mmu_delay) / tot_delay, 4) * 100))
    print("JD_DONE delay : {0} (ns) => {1}% removable".format(tot_jd_done_delay, round(float(tot_jd_done_delay) / tot_delay, 4) * 100))

if __name__ == "__main__" :
    if len(sys.argv) != 2:
        print ("too many or no arguments")
    #  run(sys.argv[1])
    #  ioctl_run(sys.argv[1])
    run_skip_prep(sys.argv[1], 16)
