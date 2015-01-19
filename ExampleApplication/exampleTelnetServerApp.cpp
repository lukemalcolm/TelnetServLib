// TelnetApplication.cpp : Defines the entry point for the console application.
//

#undef UNICODE

#define WIN32_LEAN_AND_MEAN

#include "exampleTelnetServerApp.hpp"
#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <tchar.h>

void myConnected(SP_TelnetSession session)
{
    std::cout << "myConnected got called\n";
    session->sendLine("Welcome to the Telnet Server.");
}

void myNewLine(SP_TelnetSession session, std::string line)
{
    std::cout << "myNewLine got called with line: " << line << "\n";
    session->sendLine("Copy that.");
}

int _tmain(int argc, _TCHAR* argv[])
{
    // Do unit tests
    TelnetSession::UNIT_TEST();

    // Create a terminal server which
    auto ts = std::make_shared < TelnetServer >();
    
    ts->initialise(27015);
    ts->connectedCallback(myConnected);
    ts->newLineCallback(myNewLine);

    // Our loop
    do 
    {
        ts->update();
        Sleep(16);
    } 
    while (true);

    ts->shutdown();
    WSACleanup();

    return 0;
}
