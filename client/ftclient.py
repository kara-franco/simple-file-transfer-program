# Programming Assignment 2
# Author: Kara Franco
# CS.372 - 400 Intro to Computer Networks
# Due Date: March 6, 2016
# Description: A python program that illustrates a client side that connects to a
# FTP server program. The client connects to the server, and can either request
# a list of files on the server, or ask to transfer a file from the server to
# the client's folder. The transmission of file data is over a separate data
# connection and the FTP session is managed over a control connection.

# -------------------------- modules and packages ----------------------------
# miscellaneous operating system interfaces
import os   
# regular expressions operations                   
import re       
# system specific parameters and functions               
import sys 
# socket low-level interface (socket API)
# https://docs.python.org/2/library/socket.html                     
from socket import (            
    socket,
    gethostbyname,
    AF_INET,
    SOCK_STREAM,
    SOL_SOCKET,
    SO_REUSEADDR
)
from struct import pack, unpack 

# --------------------------- main function ----------------------------------
def main():

# ------- global variables ----------
    # declare global variables for command line arguments
    global serverHost
    global serverPort
    global command
    global filename
    global dataPort

    # check for command line arguments
	# -l command is 5 agruments, -g command is 6 arguments
	# source for command line: http://www.tutorialspoint.com/python/python_command_line_arguments.htm
    if len(sys.argv) not in (5, 6):
        print (
            "Error: Use python2 ftclient <server-hostname> <server-port> " +
            "-l OR -g <filename> <data-port>"
        )
        sys.exit(1)
	# get data from the command line and place in vars
	# set the server hostname to var
    serverHost = gethostbyname(sys.argv[1])
    serverPort = sys.argv[2]
    command = sys.argv[3]
    filename = sys.argv[4] if len(sys.argv) == 6 else None
    dataPort = sys.argv[5] if len(sys.argv) == 6 else sys.argv[4]

	# ---------- check for client user input errors -------------
    # check server port number, make sure it is an actual number
    # call isNumber to check if the string entered is numbers or letters
    if not isNumber(serverPort):
        print "ftclient: Server port must be a number!"
        sys.exit(1)
    serverPort = int(serverPort)
	
	# check if -g (file transfer) command has a filename
    if command == "-g" and filename is None:
        print (
            "Error: Use python2 ftclient <server-hostname> <server-port> " +
            "-l|-g <filename> <data-port>"
        )
        sys.exit(1)

    # user command must be either -l (list) or -g (get)
    if command not in ("-l", "-g"):
        print "ftclient: Command must be either -l or -g"
        sys.exit(1)

   # check data port number, make sure it is an actual number
    if not isNumber(dataPort):
        print "ftclient: Data port must be a number!"
        sys.exit(1)
    dataPort = int(dataPort)

    # the server and data ports cannot be the same number
    if serverPort == dataPort:
        print "ftclient: Server port and data port cannot be the same!"
        sys.exit(1)

    # --- start a control connection between the FTP client and server ---
    initiateContact()

    sys.exit(0)

# ------------------------- function definitions -----------------------------
# -----------------------------------------------------------------------------
# isNumber()
# Description: A function that uses the re.match() funtion to determine if the
# user input string contains numbers or letters.
# Parameters: string
# Output : Only returns if re.match() returns not None
# sources : https ://docs.python.org/2/howto/regex.html
# http ://stackoverflow.com/questions/1265665/python-check-if-a-string-represents-an-int-without-using-try-except
# -----------------------------------------------------------------------------

def isNumber(string):
    # see if the user entered numbers or letters
    return re.match("^[0-9]+$", string) is not None

# -------------------------------------------------------------------------------
	# receiveData()
	# Description: A function that uses the recv() function to collect all the data
	# that is being sent.This function is used in the receiveFile function below.
	# Parameters: current socket endpoint to receive data, number of bytes to receive
	# Output: buffer of received data
#---------------------------------------------------------------------------------

def receiveData(socket, bytesNumber):
   # data will collect the number of bytes
	# will be called a number of times
	# source: http://stackoverflow.com/questions/7174927/when-does-socket-recvrecv-size-return
    data = "";
    while len(data) < bytesNumber:
        try:
            data += socket.recv(bytesNumber - len(data))
        except Exception as e:
            print e.strerror
            sys.exit(1);

    return data

# -----------------------------------------------------------------------------
# receiveFile()
# Description: A function that receives a file(packet) from the socket after the
# connection is made.The tag and data are returned in the control connection.
# Parameters: current socket endpoint to receive data
# Output: (tag, data) tuple
# Sources: http://www.tutorialspoint.com/python/python_tuples.htm
# http://beej.us/guide/bgnet/output/html/singlepage/bgnet.html#sonofdataencap
# -----------------------------------------------------------------------------

def receiveFile(socket):
    # get the file length
	# the first 2 bytes are the packet length
	# https://docs.python.org/2/library/struct.html
    packetLength = unpack(">H", receiveData(socket, 2))[0]

    # get the tag field
	# the 8 bytes after the packet length are the packet tag
	# http://www.tutorialspoint.com/python/string_rstrip.htm
    tag = receiveData(socket, 8).rstrip("\0")

    # get the encapsulated data(rest of the bytes from receiveData)
    data = receiveData(socket, packetLength - 8 - 2)

    return tag, data

# -----------------------------------------------------------------------------
# controlConnection()
# Description: A function that runs a control connection between the client
# and server. Communication is initiated in the initiateContact function.
# Parameters: socket of client side endpoint
# Output : -1 error, 0 success
# -----------------------------------------------------------------------------

def controlConnection(controlSocket):
    # send data port to server
    print "  Sending client data port..."
    outtag = "DPORT"
    outdata = str(dataPort)
    makeRequest(controlSocket, outtag, outdata)
	
	# send command to server
	# name the tag field, 8 byte limit
    print "  Sending user command ..."
    outtag = "NULL"
    outdata = ""
    if command == "-l":
        outtag = "LIST"
    elif command == "-g":
        outtag = "GET"
        outdata = filename
    makeRequest(controlSocket, outtag, outdata)

    # recieve the server's response
    inTag, inData = receiveFile(controlSocket)

    # if server-side error, alert user
    if inTag == "ERROR":
        print "ftclient: " + inData
        return -1
    return 0

# -----------------------------------------------------------------------------
# dataConnection()
# Description: A function that runs the data connection, used to transfer the
# file data over.Invoked in the initiateContact function.
# Parameters: the clientSocket and datasocket(client side endpoints)
# Returns : -1 error, 0 otherwise
# -----------------------------------------------------------------------------

def dataConnection(controlSocket, dataSocket):
    ret = 0 

    # get packet(s) from the server
    inTag, inData = receiveFile(dataSocket)

    # if tag field indicates filename, list the filenames to transfer
	# source for format https://docs.python.org/2/library/string.html
    if inTag == "FNAME":
        print "ftclient: List of files on \"{0}\"".format(serverHost, serverPort)

        # print received filenames
        while inTag != "DONE":
            print "  " + inData
            inTag, inData = receiveFile(dataSocket)

    # if tag field indicates file, then file is being transferred
	# source: https://docs.python.org/2/library/os.path.html
    elif inTag == "FILE":
        # Don't allow files to be overwritten.
        filename = inData
        if os.path.exists(filename):
           print "ftclient: File \"{0}\" already exists!".format(filename)
           ret = -1

        # write the received data to file
        else:
            with open(filename, "w") as outfile:
                while inTag != "DONE":
                    inTag, inData = receiveFile(dataSocket)
                    outfile.write(inData)
            print "ftclient: Success, file transfer completed!"

    # if we get here, something went terribly wrong
    else:
        ret = -1

    # send ACK of all packets
    makeRequest(controlSocket, "ACK", "")

    return ret

# -----------------------------------------------------------------------------
# makeRequest()
# Description: A function that makes a data or connection request from the client 
# socket to the server via sending a packet. Invoked in runControlConnect and 
# runDataConnect.
# Parameters: socket of connection endpoitn, tag field and the data to send.
# Output : none
# -----------------------------------------------------------------------------

def makeRequest(socket, tag = "", data = ""):
    # calculate the packet length, data + tag(8 bytes) + length bytes(2 bytes)
    packetLength = 2 + 8 + len(data)

    # construct the packet
	# sources: https://docs.python.org/2/library/struct.html
	# http://www.tutorialspoint.com/python/string_ljust.htm
    packet = pack(">H", packetLength)
    packet += tag.ljust(8, "\0")
    packet += data

    # send packet to server
	# https://docs.python.org/2/tutorial/errors.html
    try:
        socket.sendall(packet)
    except Exception as e:
        print e.strerror
        sys.exit(1)


# -----------------------------------------------------------------------------
# initiateContact()
# Description: A function that initiates and establishes the FTP connection
# between the client and server.Invoked in the main function.
# Parameters: none
# Output : none
# -----------------------------------------------------------------------------

def initiateContact():
    # create client-side endpoint of control connection
    try:
        controlSocket = socket(AF_INET, SOCK_STREAM, 0)
    except Exception as e:
        print e.strerror
        sys.exit(1)

    # establish FTP control connection
    try:
        controlSocket.connect((serverHost, serverPort))
    except Exception as e:
        print e.strerror
        sys.exit(1)
    print ("ftclient: Control connection established with " +
           "\"{0}\"".format(serverHost, serverPort)          )

    # run the FTP control connection
    status = controlConnection(controlSocket)

   # if control returns with success 0, start data connection
   # build client-side socket
    if status != -1:
        try:
            clientSocket = socket(AF_INET, SOCK_STREAM, 0)
        except Exception as e:
            print e.strerror
            sys.exit(1)

        # attach client-side socket to given data port
		# source for below: https://docs.python.org/2/library/socket.html
        try:
            clientSocket.setsockopt(SOL_SOCKET, SO_REUSEADDR, 1)
            clientSocket.bind(("", dataPort))
        except Exception as e:
            print e.strerror
            sys.exit(1)

        # listen for connections
		# set the max number to listen for to 5 (random choice)
        try:
            clientSocket.listen(5)
        except Exception as e:
            print e.strerror
            sys.exit(1)

        # run FTP data connection
		# source for below: http://stackoverflow.com/questions/12454675/whats-the-return-value-of-socket-accept-in-python
        try:
            dataSocket = clientSocket.accept()[0]
        except Exception as e:
            print e.strerror
            sys.exit(1)
        print ("ftclient: Data connection established with " +
               "\"{0}\"".format(serverHost)                       )

        # transfer file data over FTP data connection
        dataConnection(controlSocket, dataSocket)

        # print error messages from control connection
        while True:
            inTag, inData = receiveFile(controlSocket)
            if inTag == "ERROR":
                print "ftclient: " + inData
            if inTag == "CLOSE":
                break

    # client must close the connection
    try:
        controlSocket.close()
    except Exception as e:
        print e.strerror
        sys.exit(1)
    print "ftclient: File transfer connections closed, have a nice day!"


# Define script point of entry.
if __name__ == "__main__":
    main()