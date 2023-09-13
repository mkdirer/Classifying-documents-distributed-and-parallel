compile:
	/opt/nfs/mpich-3.2/bin/mpicxx -ggdb -std=c++17 src/main.cpp -o src/main

run:
	/opt/nfs/mpich-3.2/bin/mpiexec -n 7 src/main

clean:
	rm -f src/main
	rm -f result.txt
