CC=gcc

ATLAS_PATH=/opt/tools/libraries/atlas
LDFLAGS=-L$(ATLAS_PATH)/$(ARCHITECTURE)/lib -lcblas -latlas
CFLAGS=-I$(ATLAS_PATH)/$(ARCHITECTURE)/include

PROBLEM_DIMS=10000 15000 20000 25000 30000 35000 40000


ARCHS=quad nehalem opteron
RUN_ARCHS=run_quad run_nehalem run_opteron
BUILD_ARCHS=blas_quad blas_nehalem blas_opteron
BUILD_ARCHS_OPT=optim_quad optim_nehalem optim_opteron
GENERATE_PLOT_ARCHS=gen_quad gen_nehalem gen_opteron
PLOT_ARCHS=plot_quad plot_nehalem plot_opteron

blas_quad: ARCHITECTURE=3.10.1-quad-gcc-4.4.6
optim_quad: EXTRA_FLAGS=-O3 --param=l1-cache-size=32 --param=l2-cache-size=12288 -msse4.1  
blas_nehalem: ARCHITECTURE=3.10.1-nehalem-gcc-4.4.6
optim_nehalem: EXTRA_FLAGS=-O3 --param l1-cache-size=32 --param l2-cache-size=12288 -msse4.2 
blas_opteron: ARCHITECTURE=3.10.1-opteron-gcc-4.4.6
optim_opteron: EXTRA_FLAGS=-O3 -fprefetch-loop-arrays -mtune=opteron --param l1-cache-size=64 --param l2-cache-size=6144 --param simultaneous-prefetches=6


neoptim: my_dtrmv.c
	$(CC) my_dtrmv.c -o neoptim
	cp neoptim neoptim_quad
	cp neoptim neoptim_nehalem
	mv neoptim neoptim_opteron

${BUILD_ARCHS}: blas_%: blas_dtrmv.c
	./module_loaders/$*.sh
	$(CC) $(CFLAGS) blas_dtrmv.c $(LDFLAGS) -o blas_$*

${BUILD_ARCHS_OPT}: optim_%: my_dtrmv_optim.c
	$(CC) $(EXTRA_FLAGS) my_dtrmv_optim.c -o optim_$*

${RUN_ARCHS}: run_%: blas_% optim_%
	ln=0;for number in $(PROBLEM_DIMS) ; do \
	   	for tip in blas optim neoptim ; do \
			jobid=`qsub -cwd -q ibm-$*.q -pe openmpi*1 1 -b y ./"$$tip"_$* $$number out_values/"$$tip"_values_$*_$$number | cut -d " " -f 3` ; \
       			echo $$jobid >> "$$tip"_$*_job_ids ; \
			read -p "Am submis "$$tip"_$*_$$number. Repet jobul? " -n 1 -r ; \
			echo ; \
			while [[ $$REPLY = "d" ]] ; do \
				rm *.*o$$jobid *.*e$$jobid ; \
				head -n $$ln "$$tip"_$*_job_ids > temp.txt; mv temp.txt "$$tip"_$*_job_ids; \
				jobid=`qsub -cwd -q ibm-$*.q -pe openmpi*1 1 -b y ./"$$tip"_$* $$number out_values/"$$tip"_values_$*_$$number | cut -d " " -f 3` ; \
       				echo $$jobid >> "$$tip"_$*_job_ids ; \
				read -p "Am submis "$$tip"_$*_$$number. Repet jobul? " -n 1 -r ;\
				echo ;\
			done ; \
		done ; \
		((ln = ln + 1)) ; \
	done

dir_results:
	mkdir out_values

submit-job: neoptim dir_results $(RUN_ARCHS)

${GENERATE_PLOT_ARCHS}: gen_%: blas_%_job_ids neoptim_%_job_ids optim_%_job_ids
	for tip in blas optim neoptim ; do \
		touch $*_"$$tip"_plot_data ; \
		rm $*_"$$tip"_plot_data ; \
		ln=1;for number in $(PROBLEM_DIMS) ; do \
			echo -n "$$number " >> $*_"$$tip"_plot_data; \
			jobid=`sed -n "$$ln p" < "$$tip"_$*_job_ids` ; \
			sed -n "1p" < "$$tip"_$*.o$$jobid >> $*_"$$tip"_plot_data ; \
			rm "$$tip"_$*.*o$$jobid "$$tip"_$*.*e$$jobid ; \
			((ln = ln + 1)) ; \
		done ; \
		rm "$$tip"_$*_job_ids ; \
	done

check_results:
	for number in $(PROBLEM_DIMS) ; do \
		diff -q --from-file `ls out_values/*_$$number* | tr '\n' ' '` ; \
	done
	rm -r out_values

gen_all: $(GENERATE_PLOT_ARCHS) check_results

${PLOT_ARCHS}: plot_%: %_blas_plot_data %_optim_plot_data %_neoptim_plot_data
	gnuplot -e \
		"set term png; set output '$*_plot.png'; set xlabel 'Matrix dimension(elements)'; set ylabel 'Performance (sec)'; plot '$*_blas_plot_data' using 1:2 title 'Blas' with lines, '$*_optim_plot_data' using 1:2 title 'Optim' with lines, '$*_neoptim_plot_data' using 1:2 title 'Neoptim' with lines;"
	rm $*_*_plot_data

draw-plot: gen_all $(PLOT_ARCHS)

clean:
	rm blas_nehalem blas_opteron blas_quad 
	rm neoptim* optim_*

