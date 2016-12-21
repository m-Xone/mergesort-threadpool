mergeSort: mergeSort.o tp.o
	gcc -o mergeSort mergeSort.o tp.o -pthread -lm -Wall
	rm -f *~ *.o *.h.gch

mergeSort.o: mergeSort.c tp.h
	gcc -c mergeSort.c tp.h -pthread -lm -Wall

tp.o: tp.c tp.h
	gcc -c tp.c -Wall
