---
title: RP2040 Doom
description: Network Games
---

This is part of the series behind the scenes of RP2040 Doom:

* [Introduction](index.md)
* [Rendering And Display Composition](rendering.md)
* [Making It All Fit In Flash](flash.md)
* [Making It Run Fast And Fit in RAM](speed_and_ram.md)
* [Music And Sound](sound.md)
* Network Games **<- this part**
* [Development Overview](dev_overview.md)

See [here](https://www.youtube.com/playlist?list=PL-_wCtHUfdDPi7i-4OIy5iQjQ3QSqq1Mh) for some nice videos of
RP2040 Doom in action. The code is [here](https://github.com/kilograham/rp2040-doom).

## Networking Introduction

The original Doom used IPX networking, whereas Chocolate Doom ditches this in favor of a full-on internet game 
support, with the inherent latency requirements.

For RP2040 Doom, whilst I thought I might need to build my own single pin PIO networking with some sort of token 
passing, it turned out I had 2 GPIO pins free that could be configured for I2C, so I decided to just use that 
instead.

## Flat Network Stack

In general, it is preferable to separate the network stack into multiple distinct layers, but for RP2040 Doom 
this would mean creating a lot of extra code with abstractions which were hardly used. 
Chocolate Doom has 
dozens of neatly delineated 
asynchronous message types coordinating a networked state machine on top of the lower TCP/IP stack, but a lot of 
these deals with cases such as high network latency that RP2040 Doom will never encounter, or corner cases that are 
pretty meaningless if all the players are within a meter or two's wire's length of each other!

For RP2040 
Doom, I therefore decided to keep it simple, and munge the layers together reducing the code size massively, but 
requiring the networking code be completely re-written. 

## Single host multiple clients

Whichever player sets up the network game, becomes the *host* node, an I2C "master". All I2C traffic is coordinated by 
the *host* node. Initially the *host* node does not know the address of any other nodes, although it knows they will 
have addresses in the 
range 0x20-0x5f, chosen to be large enough to reduce the chance of multiple nodes choosing the same address at any 
given time, but 
also small enough 
to be able to poll each address reasonably frequently.

At any point the *host* node knows of a set of up to three *client* nodes which have been accepted by the *host* node 
for the network game. For each *client* node in the set, the *host* node knows the actual I2C address and a 32-bit 
identifier provided by the *client* node.

Because there is no need to pre-configure anything, the *host* node's client set is initially empty, and must be 
populated as a result of *auto-discovery*.

## Rounds and auto-discovery

In the flattened network stack, everything is handled by a simple "round" protocol entirely coordinated by the 
*host* node. A round occurs roughly 100 times per second, and is structured as follows:

1. The *host node* attempts to receive 32 bytes from each *client* node in its set. Under non-error conditions, a 
   *client* node should respond every time, unless it has left the game. If a *client* node repeatedly fails to 
   respond, it is dropped from the set (and the game if it in progress).

2. The *host* node attempts to receive 32 bytes from one other "polling" address (from the range 0x20-0x5f) not 
   present in its client set. This "polling" address increments within the address range every round, allowing for 
   discovery of new nodes. These nodes who are wanting to join a network game will have picked a 
   random address in the 0x20-0x5f range, and become ready to respond to read requests from a *host* node.

   The lack of response for a non-existent "polling" address is noticed very quickly by 
   the *host* node, and it moves on to the next step. If instead there is a response from the *polled* node, the 
   *host* node makes a decision whether to admit the new node to its client set. A node might not be admitted 
   because:

   1. There are already 4 players in the lobby.
   2. The WAD or game versions do not match.
   3. The game has already started.
   
   The fact that all this is handled during node discovery, is a clear example of the flat pancaking of the network 
   stack.

3. The *host node* then sends a single packet to each *client* node in the set in turn; more on that later.

4. If a *polled* node was found but not admitted as a *client* node, the *host node* sends the *polled* node a packet 
   telling it to go away!

As mentioned above, nodes wanting to join a game pick random addresses in the range 0x20-0x5f and wait to be 
contacted by the *host* node. It is of course possible that they have picked the address of an existing node, or end 
up racing 
with a new node 
using the same address. Therefore, the joining nodes pick a new random address every second (long enough for the 
host to have polled all addresses) unless they receive a packet from the *host* node indicating that they have been 
admitted to the client set. Packets from the *host* node to the *client* nodes always include that *client* node's 
32-bit identifier, so it is always clear in the case of I2C address collision who has "won".

Note that it does not matter if a player decides to join a network game before a host decides to crete one; 
their joining node will just sit there jumping to random addresses every second or so until a *host* node talks to it.

## Communication with the client set

*Client* nodes that have been admitted are in the *host* node's client set. A state sequence number is kept for each 
direction of communication between each *client node* and the *host* node, and the latest received sequence number 
is always acknowledged in the next communication in the opposite direction. This simple mechanism, means empty 
messages can be sent when there is no new state to transmit, and re-delivery can be performed if a state update is 
somehow missed.

What data is sent in each direction is dependent on the game state, other than of course the core information such 
as sequence numbers, and the 32-bit client identifier.

### In the lobby

* *Client* nodes send the player name.

* The *host* nodes sends the lobby state (i.e. all the player numbers and names) along with the game type, level and 
  difficulty. When the game 
  is started, the last 
  lobby status updates contains a flag indicating this.

### In the game

* *Client* nodes send updated player input. Player input data is generated for the local player by vanilla Doom every 
  tic, which is 35 times per second. Every tic in a network game is numbered, and there must be input data for every 
  player for each tic in order for the game AI to proceed. The *client*  node uses the sequence number mechanism to 
  make sure the 
  *host* node
  receives 
  every tic in order
  for the *client* node's player. Only one or zero tics are sent per round as the packet only has room for one, and 
  rounds are more 
  frequent than tics.

* The *host* node tailors a response to each *client* node individually, making sure each client has received and 
  acknowledged each "complete" tic in order. A tic is "complete" when input data has 
  been received for that numbered network game tic from every active player.

In this way, *client* nodes send their player input to the *host* node, who returns "completed" tics with the input 
data from each player. Every node applies only "completed" tics received back from the *host* node to update the 
game state, which means the nodes' game states do not diverge.

Clearly there needs to be some elasticity in this mechanism, and Chocolate Doom, needing to support potentially 
high network latency can buffer up 128 tics, whereas RP2040 Doom which is very RAM constrained limits this 
to 5. 
Really, 
with I2C
there really shouldn't be any dropped packets, unless someone sitting next to you tries to join with a colliding 
address in middle of a game! 
There 
does still need to be that small leeway though, as the different nodes' game loops are not synchronized, we just 
expect them to send tics out at the correct average rate.

*Client* nodes that don't provide data in time, or don't receive "completed" tics in time from the *host* node are 
unceremoniously dropped from the game.

Progress through the levels of the game is handled automatically, as vanilla Doom handles all these transitions in a 
network game as special types of "button press" which are already included in the tic input data.

---

Read the last section [Development Overview](dev_overview.md), or go back to the [Introduction](index.md).

Find me on [twitter](https://twitter.com/kilograham5).