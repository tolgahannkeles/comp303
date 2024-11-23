CC = gcc 

SentimentCal1 = sentimentCal1
SentimentCal2 = sentimentCal2
SentimentCal3 = sentimentCal3
SentimentCal4 = sentimentCal4

INPUTS = input1.txt input2.txt input3.txt input4.txt
OUTPUT1 = output1.txt
OUTPUT2 = output2.txt
OUTPUT3 = output3.txt
OUTPUT4 = output4.txt

POSITIVE_WORD = Your
NEGATIVE_WORD = have

all: $(SentimentCal1) $(SentimentCal2) $(SentimentCal3) $(SentimentCal4) run_all 

$(SentimentCal1): sentimentCal1.c
	$(CC) $(CFLAGS) sentimentCal1.c -o $(SentimentCal1)

$(SentimentCal2): sentimentCal2.c
	$(CC) $(CFLAGS) sentimentCal2.c -o $(SentimentCal2)

$(SentimentCal3): sentimentCal3.c
	$(CC) $(CFLAGS) sentimentCal3.c -o $(SentimentCal3)

$(SentimentCal4): sentimentCal4.c
	$(CC) $(CFLAGS) sentimentCal4.c -o $(SentimentCal4)

run_sentimentCal1: $(SentimentCal1)
	./$(SentimentCal1) $(POSITIVE_WORD) $(NEGATIVE_WORD) 4 input1.txt input2.txt input3.txt input4.txt $(OUTPUT1)

run_sentimentCal2: $(SentimentCal2)
	./$(SentimentCal2) $(POSITIVE_WORD) $(NEGATIVE_WORD) 4 input1.txt input2.txt input3.txt input4.txt $(OUTPUT2)

run_sentimentCal3: $(SentimentCal3)
	./$(SentimentCal3) $(POSITIVE_WORD) $(NEGATIVE_WORD) 4 input1.txt input2.txt input3.txt input4.txt $(OUTPUT3)

run_sentimentCal4: $(SentimentCal4)
	./$(SentimentCal4) $(POSITIVE_WORD) $(NEGATIVE_WORD) 4 input1.txt input2.txt input3.txt input4.txt $(OUTPUT4)

run_all: run_sentimentCal1 run_sentimentCal2 run_sentimentCal3 run_sentimentCal4

clean:
	rm -f $(SentimentCal1) $(SentimentCal2) $(SentimentCal3) $(SentimentCal4) $(OUTPUT1) $(OUTPUT2) $(OUTPUT3) $(OUTPUT4)

.PHONY: all clean run_sentimentCal1 run_sentimentCal2 run_sentimentCal3 run_sentimentCal4 run_all
