import sys
import numpy as np

def gen_matrix(N, M):
    data = np.random.uniform(-1, 1, [N, M]).astype(np.float32)
    print (data)
    print(type(data[0][0]))
    np.save('input.npy', data)

if __name__ == "__main__":
    if len(sys.argv) != 3:
        print ("need argument a b that generates a x b matrix")
    else:
        gen_matrix(int(sys.argv[1]), int(sys.argv[2]))
