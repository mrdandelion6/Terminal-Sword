# challenges
### challenge:
- keeping track of all the connected clients dynamically
### solution: 
- implemented a dynamic integer set containing list of file descriptors
---
### challenge: 
- managing the player state for a dynamic set of clients
### solution: 
- use another dynamic set, but each element is of type struct player