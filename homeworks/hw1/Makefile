CC = gcc 

TASK1 = task1
TASK2 = task2
TASK3 = task3
TASK4 = task4

INPUTS = input1.txt input2.txt input3.txt input4.txt
OUTPUT1 = output1.txt
OUTPUT2 = output2.txt
OUTPUT3 = output3.txt
OUTPUT4 = output4.txt

POSITIVE_WORD = Your
NEGATIVE_WORD = have

all: $(TASK1) $(TASK2) $(TASK3) $(TASK4) run_all 

$(TASK1): task1.c
	$(CC) $(CFLAGS) task1.c -o $(TASK1)

$(TASK2): task2.c
	$(CC) $(CFLAGS) task2.c -o $(TASK2)

$(TASK3): task3.c
	$(CC) $(CFLAGS) task3.c -o $(TASK3)

$(TASK4): task4.c
	$(CC) $(CFLAGS) task4.c -o $(TASK4)

run_task1: $(TASK1)
	./$(TASK1) $(POSITIVE_WORD) $(NEGATIVE_WORD) 4 input1.txt input2.txt input3.txt input4.txt $(OUTPUT1)

run_task2: $(TASK2)
	./$(TASK2) $(POSITIVE_WORD) $(NEGATIVE_WORD) 4 input1.txt input2.txt input3.txt input4.txt $(OUTPUT2)

run_task3: $(TASK3)
	./$(TASK3) $(POSITIVE_WORD) $(NEGATIVE_WORD) 4 input1.txt input2.txt input3.txt input4.txt $(OUTPUT3)

run_task4: $(TASK4)
	./$(TASK4) $(POSITIVE_WORD) $(NEGATIVE_WORD) 4 input1.txt input2.txt input3.txt input4.txt $(OUTPUT4)

run_all: run_task1 run_task2 run_task3 run_task4

clean:
	rm -f $(TASK1) $(TASK2) $(TASK3) $(TASK4) $(OUTPUT1) $(OUTPUT2) $(OUTPUT3) $(OUTPUT4)

.PHONY: all clean run_task1 run_task2 run_task3 run_task4 run_all
