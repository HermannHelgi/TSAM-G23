# TSAM Assignment 5
## Group 23 - Students:
* Ágúst Máni Þorsteinsson
* Hermann Helgi Þrastarson

## LINGO

To note, in this file, we refer to our own server simply as 'server', or 'the server', etc. 
Any other server which is not ours will be referenced as a 'bot' 

## HOW TO COMPILE

As per the assignment description, one should be able to simply run in the terminal 'make'
And the make file will create all the required executables necessary. 

Running 'make clean' will remove all executables AND the Log/ErrorLog files, in case some are causing troubles.

## HOW TO RUN

SERVER:
After compiling, running ./tsamgroup23 < port > will start the server with the given port number. 
The server uses a hardcoded ip address for self analysis.
It will need to be changed if the server is run on something other then the TSAM server.

CLIENT:
To run the client, simply run ./client < ip > < port > with the same port as the server.
Once connected, the client will be prompted to type in a password to prove its authenticity.
This is a security feature to prevent other client's to connect to our server.
The password can be found at the bottom of the server.h file and can be changed as you wish.
Currently it is set to: Admin123

SERVERSIM:
In addition to these files, there is also a 'serversim' file. This can be run with: ./serversim < ip > < port >.
This file allows you to 'simulate' a bot connecting to the main server and send commands. 
It will try and connect to the given IP and portnumber and from there you can manually type commands 
to test the server. Such as 'HELO,A5_1' and such. Typing in 'DOUBLE' at any time will allow you to send two commands at once.
Note, the server simulator only adds SOH/EOT characters to your messages.

## INPUTS
Any inputs described in the assignment description have been implemented as intended by the assignment.

Aside from them we also have implemented several other commands for the client for ease of use.
Just to note, as with other commands, all additional variables are comma sperated.

* CONNECTSERVER: Allows the client to manually tell the server to connect to another bot.
To run, send: CONNECTSERVER,< IP of bot >,< PORT of bot >
Example (connects to Instr_1): CONNECTSERVER,130.208.246.249,5001

    - This allows for jumpstarting the server if there aren't any other bots scanning for connections.
    
* MESSAGEBUFFER: Shows what messages addressed to us are stored and from who.
To run, send: MESSAGEBUFFER

    - This message takes no options. The user can use this command in combination with GETMSG.
    - This is for ease of use, so the user doesn't have to guess what groups may have sent a message.

* DOCSERVERS: Shows what bots have been documented.
To run, send: DOCSERVERS

    - This message takes no options.
    - Whenever the servers takes in a SERVERS command, it documents every bot that is supposedly connected to that bot. 
    - Documenting bots allows us to easily maintain our minimum connections requirements.
    - If a bot disconnects, and the server goes below our minimum connection requirement, 
        the server tries to connect to other bots which are on the documented bot list.
    - This command is for debugging purposes so the user can then see the full list of documented bots with this command.

## FOR GRADING
Within the .Zip file, we have a folder called SavedFiles.
Within this folder are all the files which are referenced by the Project Report.

## NOTE ON STRUCTURE
The tsamgroup23.cpp file is simply a shell file which runs an infinite loop of commands to the server.cpp file.
Our actual server is implemented in a large class which can be found in the server.cpp/.h files.

## A5_300
We would like to mention our black list and the only group that is on it.
A5_300 has been great for finding bugs within our code such as:
* Creating a death spiral of error commands back and forth,
* Sending negative messages in STATUSRESP,
* Sending plain text messages instead of commands, such as \[SOH] I am sending this message because....\[EOH],
* General exception handling,
* And more.

However, despite fixing these issues that A5_300 showed us. We believe that the A5_300 bot is not implementing the protocol as agreed upon by the class, nor as stated in the project description. For this reason, it has been hardcoded to be blacklisted.