

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

running ./tsamgroup23 <port> will start the server with the given port number. 
The server uses a hardcoded ip address. 
It will need to be changed if the server is run on something other then the TSAM server.
To run the client run ./client <ip> <port> with the same port as the server.


// Keep password system?
Note the client will ask for a paassword to connect to the server, 
this is a security feature to prevent other client's to connect to our server.
The password can be found at the bottom of the server.h file ;).

# INPUTS

Any inputs described in the assignment descripton have been implemented as intended by the assignment.
Aside from them we also have implemented several other commands for the client.
* CONNECTSERVER: allows for manualy connecting our server (server A) to a given server (server B). 
    - Which allows for jumpstarting the server if there aren't any other servers scanning for connections.
    - To run CONNECTSERVER simply start the client as mentioned before and then run "CONNECTSERVER,serverip,portnr"
    - This will connect our server (A) to the given server (B). 
    - Our server then fetches the list of servers from B, And attempts to connect to all servers in that list.
    - Just to note, as with other commands, each section is comma sperated.
* MESSAGEBUFFER: Shows what messages addressed to us are stored and from who.
    - This message takes no options. We can use this command in combination with GETMSG.
    - Simply so that we don't have to guess to check if we have any messages from them.
* DOCSERVERS: Shows what servers have been documented.
    - We are documenting servers that we see are on the botnet. 
    - documenting servers allows us to easily maintain our max connections requirements.
    - if a server disconnects, then we can check our documented servers and try to connect to them.
    - For debugging purposes we can then see our list of documented servers with this command.


# FOR BOUNS POINTS
We have a folder called saved logs. 
In there is a MaliciousAttack log which shows the communication log with a evil server.
The log shows how the evil server trying to DDOS our server by sending a huge ammount of SOH EOT.
This our server handled, however..
It also sent a HELO and pretended to be us, 
which allowed us to discover a major flaw in our code. which allowed for this to happen.
This caused our code to attempt to close our own connection after not reciving any keep alives.
Affectivly ruining our own socket. We have since fixed this issue and don't allow others to connect to us,
if they are trying to predend to be us. 
This can be seen in the MaliciousAttack logs where servers start to disconnect from us slowly one after the other.
Where at the end the final server disconnects and our server affectivly frezzes and nothing more happend after,
20+ minutes of waiting.

# A5_300

We would like to mention our black list and the only group that is on it.
A5_300 has been great for finding bugs within our code such as.
* reconnecting whilst already connected,
* creating a death spiral of error commands back and forth.
* neagtive port numbers
* and more.
How ever despite fixing these issues that A5_300 showed us. we can not continue to cooparte with it.
So we created a black list specificly for A5_300.