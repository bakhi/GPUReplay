import sys

def run(name):
  lst = []
  
  f_rbuf = open(name + "_rbuf_history", "r")
  f_regs = open(name + "_regs_history" , "r")
  f_time = open(name + "_timeline_history", "r")
  f_out = open(name + "_integrated", "w")
  
  while True:
    line = f_rbuf.readline()
    if not line: break
    tmp = line.split(']', 1)
    tmp[0] = tmp[0].replace('[', '')
    lst.append(tmp)
  f_rbuf.close()
  
  while True:
    line = f_regs.readline()
    if not line: break
    tmp = line.split(']', 1)
    tmp[0] = tmp[0].replace('[', '')
    lst.append(tmp)
  f_regs.close()
  
  while True:
    line = f_time.readline()
    if not line: break
    tmp = line.split(']', 1)
    tmp[0] = tmp[0].replace('[', '')
    lst.append(tmp)
  f_time.close()
  
  lst.sort()

  cnt = 0
  for i in range(len(lst)) :
    a = "\t".join(lst[i]).split()
    if cnt > 0:
        print(i, lst[i])
        break
    if i != len(lst) - 1:
        b = "\t".join(lst[i+1]).split()
        if a[0] == b[0] and (a[3].startswith('JIN_KBASE_FOPS_DONE')) and (b[3].startswith('JIN_KBASE_FOPS_POLL')):
            print(i, lst[i])
            print(i, lst[i+1])
            continue
    if i != 0:
        b = "\t".join(lst[i-1]).split()
        if a[0] == b[0] and a[3].startswith('JIN_KBASE_FOPS_POLL') and b[3].startswith('JIN_KBASE_FOPS_DONE'):
            print(i, lst[i-1])
            print(i, lst[i])
            print()
            f_out.write("\t".join(lst[i]))
            f_out.write("\t".join(lst[i-1]))
            continue

    f_out.write("\t".join(lst[i]))
    #print "".join(lst[i])
  
if __name__ == "__main__" :
  if len(sys.argv) != 2:
    print ("too many or no arguments... use prefix of the file as first argument. \"e.g. python integrate.py va32k_a\"")
  else :
    run(sys.argv[1])
