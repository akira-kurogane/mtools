mlogparser:	mlogparser.c loglines_logevents.h \
	loglines_logevents.c
	gcc -O2 -g -c loglines_logevents.c
	gcc -O2 -g -c mlogparser.c
	gcc -o mlogparser mlogparser.o loglines_logevents.o

#Case imagining two more files loglines_logevents.c and rpi_pmu, each with a header
#mlogparser:	mlogparser.c loglines_logevents.h \
#	loglines_logevents.c \
#	rpi_pmu.h  rpi_pmu.c
#	gcc -c rpi_pmu.c 
#	gcc -c loglines_logevents.c
#	gcc -O2 -c mlogparser.c
#	gcc -o mlogparser mlogparser.o loglines_logevents.o rpi_pmu.o
 
clean:
	rm mlogparser *.o
