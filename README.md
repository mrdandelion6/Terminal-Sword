# Terminal Sword

Terminal Sword is a C project hosting an online multiplayer terminal-based sword game using stream sockets with a TCP protocol.

## Installation

To play the game you must run the server on a linux terminal and join from another linux terminal. 

### Running Server
To compile and run the server, follow these steps:

1. Clone the repository: `git clone https://github.com/your-username/terminal-sword.git`
2. Navigate to the project directory: `cd terminal-sword`
3. On Bash, `make`, or compile directly `gcc -o sword sword.c`
4. Run the server: `./sword`

The server is set to run on port 55976. Ensure this port is not in use by anything else on the server's local network. You can also choose to change this port by changing the PORT macro in `sword.c` and `makefile`. 

### Joining Server
Clients can join from a separate terminal on the same machine by entering the following command:
```
stty -icanon; nc localhost 55976
```
Alternatively, to connect from a different machine use this command:
```
stty -icanon; nc <IP_address> 55976
```
where `<IP_address>` is the IP of the server machine. 

### Port Forwarding
Port forwarding is mandatory for your server to allow clients outside of the local network to connect. Clients do not port forward, only the network hosting the server does. 

The internet router the server is connected on will need the port 55976 forwarded to `<IP_address>` in order for clients on a different network to join. 

If you don't know how to do this, you can look for tutorials online or stick to local network gameplay.

## Dependencies

This project requires the following dependencies:

- C compiler (e.g., GCC)
- Linux Terminal (e.g., Ubuntu)

## Gameplay

Terminal Sword is a turn-based game, where players choose a sword and fight against other players online. 

### Matching
When a player joins the server, they will be queued for a match until another player becomes available to fight. When this happens, a match will commence and end when either player wins. A player will win if they successfully drain all of their opponent's HP or if their opponent disconnects from the server.

Once two players have finished a match, they will prefer to initiate a battle with a different player who is waiting, if any. The player who gets to fight the waiting player will be random. If there is no one waiting, the two players will fight again.

### Different Moves
When it is a player's turn, they can choose to use a normal attack `(a)`, a power attack `(p)`, or to speak `(s)`. Speaking does not end your turn but attacking does. 

### Character Stats

There are 5 different character stats which go into gameplay:

`HP` : How much damage players can take before losing.

`MP` : How many power attacks plays can use.

`ND` : The damage a normal attack does.

`PD` : The damage a power attack does, when landed.

`LUCK` : The chance of landing a power attack. A decimal ranging from 0 - 1.

### Different Swords

The sword a player chooses will determine their character stats and attack cosmetic. There are currently 4 different swords to choose from:

---
`fire sword` : 
- 100 HP
- 1 MP
- 20 ND
- 70 PD
- 0.8 LUCK
---
`water sword` :
- 150 HP
- 10 MP
- 15 ND
- 30 PD
- 0.5 LUCK
---
`air sword` :
- 90 HP
- 2 MP
- 20 ND
- 50 PD
- 0.6 LUCK
---
`blood sword` :
- 120 HP
- ∞ MP
- 5 ND
- 999 PD
- 0.1 LUCK
---

### Power Attacks
Using a power attack costs 1 MP. Power attacks are not guaranteed to hit. The chance of landing one depends on a character's LUCK stat. When a player misses a power attack, their MP is still drained by 1. When a player attempts to use a power attack at 0 MP, they will be told to use another move and their turn will not end.
