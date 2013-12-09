David Lopes and Andi Cescolini 

client subdirectory: 
tatl_client.c (defines the functions which provide the majority of the client functionality) 
client.c (used to initialize and run the client)
makefile 

server subdirectory:
tatl_server.c (defines the functions which provide the majority of the server functionality) 
server.c (used to initialize and run the client)
makefile 

files common to both directories: 
eztcp.h and eztcp.c (to handle the TCP calls)
linked.c and linked.h (to create a linked list)
sassyhash.h and sassyhash.c (to create a hashmap)
tatl.h (basic chat framework, defines our #defines and default values)
tatl_core.h and tatl_core.c (chat framework, includes the defintions for the protocols)
find_prime.c and find_generator.c (NEED DESCRIPTION HERE)
vegCrypt.c and vegCrypt.h (NEED DESCRIPTION HERE)

DO WE NEED NOTES.TXT?

Launching the chat client:
First run ./server on any machine

On a different machine, run p2pchat [hostname]:9421, where the hostname is the hostname of the machine running the server
The chat client will prompt you from there for a command (list to get a list of current groups, join to join one of these groups, and quit to close the chat client)
