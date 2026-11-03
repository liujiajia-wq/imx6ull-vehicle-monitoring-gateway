DST=all
SRC=main.c
CC=gcc 
FLAG=-g -lpaho-mqtt3c -lrt 
LIBS+= -L/home/linux/mqtt/paho.mqtt.c-master/build/output 
LIBS+= -L/home/linux/mqtt/openssl-1.0.0s/__install/lib
INC+=-I/home/linux/mqtt/paho.mqtt.c-master/src
INC+= -I/home/linux/mqtt/openssl-1.0.0s/__install/include

$(DST):$(SRC)
	$(CC) $(SRC) $(FLAG)
arm:$(SRC)
	arm-linux-gnueabihf-gcc	 $(SRC) $(LIBS) $(INC) $(FLAG) -lssl -lcrypto 
clean:
	rm $(DST)
