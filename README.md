# simple-file-transfer-program
A program from Networking course that demonstrates a simple file transfer interaction between a server and a client.

This program implements the file transfer server in C and the file transfer client in python, and is based on the resource 
Beej’s Guide to Network Programming. Full references can be found in the .c and .py files.

*** This program was tested and will run on flip.engr.oregonstate.edu port 22 ***

*** This program was tested on the same computer, using two flip windows ***

Compile/Set up:

- Place the makefile and ftserver.c in a separate directory, with the files you choose to test

- Place the ftclient.py in a separate directory, that does not have the test files

Ex. project2Server (folder) 

makefile 

ftserver.c

test_file.txt

Ex. project2Client (folder)

ftclient.py

** names of directories do not matter! **

- In your first flip window instance go to the directory with the makefile and ftserver.c files and Type “make” into the command line 
(without parentheses).

Run/Control of Program:

- In the directory that you compiled ftserver.c, type “ftserver port#” without parentheses, and where port# is the port you wish to use.

Example- “flip1% .ftserver 30472”

- ** Note which flip you are using for the server, flip1, flip2, flip3 **

- The server is running, if no clients are trying to connect, the server will wait for them.

- Next, open a new window (instance) of putty, and login to the flip server, go into the directory with the ftclient.py file.

- Type “python ftclient.py flip# serverPort# [-l OR -g&lt;filename&gt;] dataPort#” without parentheses. flip# is the flip instance the 
server is using, serverPort# is the port number you selected for the server to run on, command is either list (-l) or get file (-g) and
dataPort# is a number the client chooses to run. Example below uses server above.

Example- “python ftclient.py flip1 30472 -l 30147”

- The above command will list all the files in the directory, and close the connection.

- Next type the get command to transfer a file to your client directory with a new dataPort#:

Example- “python ftclient.py flip1 30472 -g test_file.txt 30148”

- The above command will transfer the test_file.txt to your client directory, or send a message if the file already exists there. 
The connection will be closed.

- After these client interactions, the server will remain on, waiting for more clients.
