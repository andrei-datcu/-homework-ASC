EXENAME=conv
TIMES=times.txt
RUNNING_SCRIPT=run_tests.sh
TEST_COUNT=20
TEST_DIR=tests

GLOBALVSSHARED=globalvsshared.png
CPUVSGPU=cpuvsgpu.png

$(EXENAME): *.cu *.cpp
	nvcc -I$(CUDA_PATH)/samples/common/inc/ $^ -o $(EXENAME)


build: $(EXENAME)

$(TIMES): $(EXENAME) $(RUNNING_SCRIPT)
	./$(RUNNING_SCRIPT) $(EXENAME) 2> $(TIMES)

run : $(TIMES)

clean:
	rm $(EXENAME) $(GLOBALVSSHARED) $(CPUVSGPU) $(TIMES) out.txt

plot: $(TIMES)
	which gnuplot > /dev/null
	tmp_shared=shared_data.txt;\
	tmp_global=global_data.txt;\
	tmp_cpu=cpu_data.txt;\
	rm $$tmp_shared &> /dev/null;\
	rm $$tmp_global &> /dev/null;\
	rm $$tmp_cpu &> /dev/null;\
	for testno in $$(seq 1 $(TEST_COUNT)) ; do\
		((cpuindex=testno*3));\
		((shindex=cpuindex-1));\
		((gindex=shindex-1));\
		gtime=`sed -n "$$gindex"p $(TIMES) | sed 's/.*: \(.*\) ms/\1/'`;\
		shtime=`sed -n "$$shindex"p $(TIMES) | sed 's/.*: \(.*\) ms/\1/'`;\
		cputime=`sed -n "$$cpuindex"p $(TIMES) | sed 's/.*: \(.*\) ms/\1/'`;\
		matrixelc=`awk '{ print $$1*$$2; }' $(TEST_DIR)/test_$$testno.txt`;\
		echo $$matrixelc $$gtime >> $$tmp_global;\
		echo $$matrixelc $$shtime >> $$tmp_shared;\
		echo $$matrixelc $$cputime >> $$tmp_cpu;\
	done;\
	gnuplot -e \
		"set term png; set output '$(GLOBALVSSHARED)'; set xlabel 'Picture size(pixels)';set ylabel 'Performance(msec)'; plot '$$tmp_global' using 1:2 title 'Global memory' with linespoints, '$$tmp_shared' using 1:2 title 'Shared memory' with linespoints;";\
	gnuplot -e \
		"set term png; set output '$(CPUVSGPU)'; set xlabel 'Picture size(pixels)';set ylabel 'Performance(msec)'; plot '$$tmp_global' using 1:2 title 'GPU - Global memory' with linespoints, '$$tmp_shared' using 1:2 title 'GPU - Shared memory' with linespoints, '$$tmp_cpu' using 1:2 title 'CPU' with linespoints;";\
	rm $$tmp_shared $$tmp_global $$tmp_cpu


.PHONY: clean
