import sys

def run(name):
  print("start")  
  f_regs = open(name + "_regs_history" , "r")
  f_out = open(name + "_input.txt", "w")

  line = f_regs.readline()
  tmp = line.split()

  reg_io = []
  ts_prev = long(tmp[0])  # prev timestamp
  tmp[0] = 0
  reg_io.append(str(tmp[0]))	# timestamp
  reg_io.append(tmp[4])	# R/W
  reg_io.append(tmp[6])	# reg
  reg_io.append(tmp[8])	# val
  f_out.write(",".join(reg_io) + '\n')

  while True:
    line = f_regs.readline()
    if not line: break
    tmp = line.split()

    reg_io = []
    tw = long(tmp[0]) - ts_prev
    ts_prev = long(tmp[0])
#   reg_io.append(tmp[0])	# timestamp
    reg_io.append(str(tw))	    # timewait
    reg_io.append(tmp[4])	# R/W
    reg_io.append(tmp[6])	# reg
    reg_io.append(tmp[8])	# val
    f_out.write(",".join(reg_io) + '\n')
  f_regs.close()
  print("Done")
  
if __name__ == "__main__" :
  if len(sys.argv) != 2:
    print ("too many or no arguments")
  
  run(sys.argv[1])
