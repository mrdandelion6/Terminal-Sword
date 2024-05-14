# Terminal Sword

Terminal Sword is a low level C project hosting an online multiplayer terminal-based sword game using stream sockets with the TCP protocol.

## 🛑 AO Warning 🛑
Part of this code was submitted by me for a computer science course at UofT and you will most definitely be flagged for an **academic offense** if you use it for any assignments. Nobody at all did the assignment the way it was done in this repository - literally did not use any starter code and made this from scratch in a very unique way. Don't give yourself an AO. This repository is already known to be public by course instructors, and thus any code copied from this repo will be immediately flagged.

## Installation
To play the game you must run the server on a linux terminal and join from another linux terminal. 

### Running Server
To compile and run the server, follow these steps:

1. Clone the repository: `git clone https://github.com/your-username/terminal-sword.git`
2. Navigate to the project directory: `cd terminal-sword/src`
3. On Bash, run `make`, or compile directly with `gcc -o sword sword.c`
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
where `<IP_address>` is the IP of the server machine's network. 

### Port Forwarding
Port forwarding is mandatory for your server to allow clients outside of the local network to connect. Clients do not port forward, only the network hosting the server does. 

The internet router the server is connected on will need the port 55976 forwarded to the server machine's IPv4 address in order for clients on a different network to join. 

If you don't know how to do this, you can look for port forwarding tutorials online or stick to local network gameplay.

## Dependencies

This project requires the following dependencies:

- C compiler (e.g., GCC)
- Linux Terminal (e.g., Ubuntu)

## Gameplay

For gameplay documentation, see `game-documentation.pdf` in `documentation`.
