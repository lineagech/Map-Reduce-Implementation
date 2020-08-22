OUT = mr-wordc mr-grep
OUTS = mr-wordc mr-grep mapreduce.o
CFLAGS = -Wall -g -std=gnu99 -pthread
SOURCES = mapreduce.o
LIBS =

INPUT_PATH=input
OUTPUT_PATH=output
TRUTH_PATH=output_compare

.PHONY: default

default: $(OUTS)

mr-wordc: $(SOURCES) bin/mr-wordc.o
	gcc $(CFLAGS) $^ $(LIBS) -o $@

mr-grep: $(SOURCES) bin/mr-grep.o
	gcc $(CFLAGS) $^ $(LIBS) -o $@

mapreduce.o: mapreduce.c
	gcc $(CFLAGS) -c $^ $(LIBS) -o $@

clean:
	rm -f $(OUTS)
