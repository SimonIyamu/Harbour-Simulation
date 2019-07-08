# Harbour-Simulation
The goal of this assignment was to create independent programs that are able to run simultaneously and simulate the operations of a port.  

There are 3 types of processes:  
1) Vessels of any type/size that wish to bind and stay for some finite time in the port  
2) Port master  
3) Port monitor  

Any vessel wishing to berth remains out of the port space and should first agree with the port master to get an order of approach along with a specific berthing space.
When a boat enters, exits, or maneuvers in the harbor area it should
always be the only ship in motion in this limited space.
When the port master makes sure that there is no traffic in the port area, he lets the next ship proceed.
The boat enters, docks, and stays in the specific position he has been given for as long as it is needed to complete its job.  

Port-master's role is to oversee the safe operation of the infrastructure and to record in one
a public ledger throughout his harbor activity.  Depending on the time of stay and the
type of vessel, for each ship the mooring cost shall be indicated in the public ledger. Also in public
ledger is the name of the ship, the time of arrival, the parking, the type of ship, and the status
of the ship (ie present at the port or departing).  

When a ship decides to depart, it follows the expected sequence of events: it communicates with the port master and waits for his acceptance. When the latter issues a departure order
(after clearing that there is no other outgoing / incoming traffic), it records in the public ledger the time
of that departure.

The port monitor is an independent program that provides the port situation at regular intervals
(ie which vessels are present). Also with a periodically defined user provides statistics
interest, such as total harbor revenue so far, average waiting and income
per category and overall for all vessels.  

For the implementation we are asked to use semaphores for the successful communication between the processes. Each process is attached to a shared segment where the public ledger is stored. The processes should be able to sleep instead of causing a 'busy waiting' problem.

---------------
This project was one of the assignments in the Operating Systems course in 2018.
