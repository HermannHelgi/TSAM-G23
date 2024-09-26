# TSAM Assignment 3
# Group 23 - Students:
# Ágúst Máni Þorsteinsson
# Hermann Helgi Þrastarson

# HOW TO COMPILE:

Within this submission should be a standard Makefile, typing "make" should create the executables for both the puzzlesolver and the scanner.

# INPUTS

Both of the executables should be ran as described in the assignment description, but a sudo is necessary for the puzzlesolver due to the raw socket:

    ./scanner <Ip address> <Low port> <High port>
    sudo ./puzzlesolver <Ip address> <Port 1> <Port 2> <Port 3> <Port 4>

# BUGS

There are some bugs that can take place with the puzzle solver. It is a rare issue, but can happen.
We were not able to find the root of the issue and we assume it to be the fault of slow I/O times leading to our puzzle solver for loops receiving a message from a port, the data not being sent to the buffer until one cycle of if sentences are passed, and the port AFTER that desired port being registered as the "right port" to access.
This leads to the operation necessary to solve a certain port being performed on the wrong port, leading to bugs. We reccommend simply running the code a second time or third time and it should hopefull work.

# ICMP Bonus problem.

In the puzzlesolver file, we have two pieces of code in place to solve the ICMP problem. 
We asked on the discord whether we had gotten the bonus problem, which was succesful however, due to rampant debugging and switching between codes, we were unsure of which variant of our code acutally succeded.
We didn't want to bother further on the day of the turn-in, so we've left both pieces of code that may have solved it.
We believe the code currently placed in is the one which worked, however if it doesn't work then try commenting out the code above the current one. (Start at line 426 and ends at 489) 
(Sorry for the inconvenience).