David Lopes and Andi Cescolini 

client subdirectory: 
tatl_client.c (defines the functions which provide the majority of the client functionality, including the p2p functionality) 
client.c (used to initialize and run the client, takes user input)
vegCrypt.c and vegCrypt.h (used to implement secure chat rooms)
gmp directory (also used for chat room security features)
makefile 

server subdirectory:
tatl_server.c (defines the functions which provide the majority of the server functionality) 
server.c (used to initialize and run the server)
makefile 

files common to both directories: 
eztcp.h and eztcp.c (to handle the TCP calls)
linked.c and linked.h (to create a linked list)
sassyhash.h and sassyhash.c (to create a hashmap)
tatl.h (basic chat framework, defines our #defines and default values)
tatl_core.h and tatl_core.c (chat framework, includes the defintions for the protocols)

Launching the chat client:
First run ./server on any machine

On a different machine, run p2pchat [hostname]:9421, where the hostname is the hostname of the machine running the server
The chat client will prompt you from there for a command (list to get a list of current groups, join to join one of these groups, and quit to close the chat client)

Extended feature: Our chat client is cryptographically secure in that it requires a user to enter a password before entering and uses 
