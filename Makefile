SRV_SRCS= fwebserver.c hash.c http.c
HEADS	= http.h hash.h
CLI_SRCS= client.c
CCFLAGS	= -Wall

all: $(SRV_SRCS) $(HEADS)
	$(CC) $(CCFLAGS) $(SRV_SRCS) -o server

client: $(CLI_SRCS)
	$(CC) $(CCFLAGS) $(CLI_SRCS) -o client

clean:
	rm -f *.o *.exe *.out client server

