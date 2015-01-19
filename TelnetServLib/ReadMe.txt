TelnetServLib is a very light ANSI Telnet Server library for use in apps with 'game
loops': i.e. update, render. It utilises TCP select to enable it to operate in the 
main thread.

There are two classes: TelnetServer, TelnetSession

TelnetServer
============
TelnetServer is responsible for setting up a listening port and accepting 
incoming connections. It also is responsible for keeping TelnetSessions up-to-
date.

Start a TelnetServer with the following code:
    
    // Create your TelnetServer with a shared pointer.
    std::shared_ptr<TelnetServer> ts = std::make_shared < TelnetServer >();

    // Start your TelnetServer on a given port and present an interactive prompt: py>
    ts->initialise(27015, "py> ");

Once your TelnetServer is created you can hook external functions to call when
certain events happen in TelnetSessions.
	
	// Call back after initial telnet client initialised (used for MOTD etc)
    ts->connectedCallback(&MyClass::myConnectedFunction);

    // Call back made after every carriage return from the telnet client
    ts->newLineCallback(&MyClass::myNewLineFunction);

These callback functions should be structured as follows:
    
    void myConnectedFunction(SP_TelnetSession session);
    void myNewLineFunction  (SP_TelnetSession session, std::string line);

SP_TelnetSession is a type definition to a shared pointer to the TelnetSession. With
access to the TelnetSession you can send responses etc.

TelnetSession
=============
TelnetSessions are currently open telnet sessions with clients.

TelnetSessions can be accessed in two ways:
- It is passed to the callback functions
- A list of active sessions can be retrieved from TelnetServer::sessions()

The key functions:
    void sendLine(std::string data);    // Send a line of data to the Telnet Server
    void closeClient();                 // Finish the session

NB: sendline does not require a closing newline.

