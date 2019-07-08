all: myport vessel port-master monitor

myport : myport.c 
	gcc myport.c -o myport -g -lpthread

vessel : vessel.c
	gcc vessel.c -o vessel -g -lpthread

port-master : port-master.c
	gcc port-master.c -o port-master -g -lpthread

monitor : monitor.c
	gcc monitor.c -o monitor -g -lpthread

clean:
	rm myport vessel port-master monitor
