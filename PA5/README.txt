Custom Commands:


REMEMBER TO CHANGE PASSWORD BEFORE TURN IN!!!!!!
Server has hardcoded IP address :S

CONNECTSERVER - <IP><PORT>
Tells the server to try and connect to the IP address and port given. Good way to 'Hotwire' a start.

MESSAGEBUFFER
Gives list of how many messages the server has in the client buffer for messages.

DOCSERVERS
Gives list of all documented servers.

GetAll Messages?
Disconnect from some group?


Two error codes:

-1 == unknown command, / wrong variables etc
-2 == We fucked up, usually a system command.


!! Here is the real readme !!
# TSAM Assignment 5
# Group 23 - Students:
# Ágúst Máni Þorsteinsson
# Hermann Helgi Þrastarson

# HOW TO COMPILE

As per assignment descripton, simply run the make file and required executables should be made.
run make clean to remove all executables, in case some are causing troubles.

# HOW TO RUN

running ./tsamgroup23 <ip> <port> will start the server with the given port number.
To run the client run ./client <ip> <port> with the same port as the server.
Note the client will ask for a paassword to connect to the server, 
this is a security feature to prevent other client's to connect to our server.
The password can be found at the bottom of the server.h file ;).

# INPUTS

Any inputs described in the assignment descripton have been implemented as intended by the assignment.
Aside from them we also have implemented CONNECTSERVER command for the client, 
Which allows for jumpstarting the server if there aren't any other servers scanning for connections.
To run CONNECTSERVER simply start the client as mentioned before and then run "CONNECTSERVER,serverip,portnr"
This will connect our server (A) to the given server (B). 
Our server then fetches the list of servers from B, And attempts to connect to all servers in that list.
Just to note, as with other commands, each section is comma sperated.

# FOR BOUNS POINTS
We have a folder called saved logs. 
In there is a MaliciousAttac log which shows the communication log with a evil server.
The log shows how the evil server trying to DDOS our server by sending a huge ammount of SOH EOT.
This our server handled, however..
It also sent a HELO and responded with being us, 
which allowed us to discover a major flaw in our code. which allowed for this to happen.
This caused our code to attempt to close our own connection after not reciving any keep alives.
Affectivly ruining our socket. We have since fixed this issue and don't allow others to connect to us,
if they are trying to predend to be us.



