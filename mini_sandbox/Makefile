CC = g++

TARGET = sandbox

SRC = sandbox.c

$(TARGET): $(SRC)
	mkdir -p out/
	$(CC) -o out/$(TARGET) $(SRC)

clean:
	rm -f out/$(TARGET)
