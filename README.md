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

For gameplay documentation, see `game-documentation.pdf` in `documentation`.