# Terminal Sword

This is a C project that demonstrates an online multiplayer terminal-based sword game.

## Installation

To play the game you must run the server and join from another terminal. To compile and run the server, follow these steps:

1. Clone the repository: `git clone https://github.com/your-username/terminal-sword.git`
2. Navigate to the project directory: `cd terminal-sword`
3. Compile the code with Bash: `gcc -o sword battle.c`
4. Run the server: `./sword`

Now in a separate terminal on the same machine, enter the following command:
```
stty -icanon; nc localhost 55976
```
Alternatively, to connect from a different machine use this command:
```
stty -icanon; nc <IP_address> 55976
```
where <IP_address> is the IP of your server machine. It is worth nothing that the server machine will need the port 55976 forwarded. You can also just change the value of the PORT macro in battle.c and makefile to whatever you want.

## Gameplay

Terminal , you control a sword and fight against enemies. Use the arrow keys to move the sword and press the spacebar to attack.

## Dependencies

This project requires the following dependencies:

- C compiler (e.g., GCC)
- Linux Terminal (e.g., Ubuntu)


