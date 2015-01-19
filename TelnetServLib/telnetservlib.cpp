#include "telnetservlib.hpp"
#include "iostream"
#include <assert.h>
#include <array>
#include <iterator>

// Need to link with Ws2_32.lib
#pragma comment (lib, "Ws2_32.lib")
// #pragma comment (lib, "Mswsock.lib")

#define DEFAULT_BUFLEN 512

void TelnetSession::sendPromptAndBuffer()
{
    // Output the prompt
    u_long iSendResult;
    iSendResult = send(m_socket, m_telnetServer->promptString().c_str(), (u_long)m_telnetServer->promptString().length(), 0);

    if (m_buffer.length() > 0)
    {
        // resend the buffer
        iSendResult = send(m_socket, m_buffer.c_str(), (u_long)m_buffer.length(), 0);
    }
}

void TelnetSession::eraseLine()
{
    u_long iSendResult;
    // send an erase line       
    iSendResult = send(m_socket, ANSI_ERASE_LINE.c_str(), (u_long)ANSI_ERASE_LINE.length(), 0);

    // Move the cursor to the beginning of the line
    std::string moveBack = "\x1b[80D";
    iSendResult = send(m_socket, moveBack.c_str(), (u_long)moveBack.length(), 0);
}

void TelnetSession::sendLine(std::string data)
{
    u_long iSendResult;
    // If is something is on the prompt, wipe it off
    if (m_telnetServer->interactivePrompt() || m_buffer.length() > 0)
    {
        eraseLine();
    }

    data.append("\r\n");
    iSendResult = send(m_socket, data.c_str(), (u_long)data.length(), 0);

    if (m_telnetServer->interactivePrompt())
        sendPromptAndBuffer();
}

void TelnetSession::closeClient()
{
    u_long iResult;

    // attempt to cleanly shutdown the connection since we're done
    iResult = shutdown(m_socket, SD_SEND);
    if (iResult == SOCKET_ERROR) {
        printf("shutdown failed with error: %d\n", WSAGetLastError());
        return;
    }

    // cleanup
    closesocket(m_socket);
}

void TelnetSession::echoBack(char * buffer, u_long length)
{
    // Echo the buffer back to the sender

    // If you are an NVT command (i.e. first it of data is 255) then ignore the echo back
    unsigned char firstItem = * buffer;
    if (firstItem == 0xff)
        return;

    u_long iSendResult;
    iSendResult = send(m_socket, buffer, length, 0);

    if (iSendResult == SOCKET_ERROR && iSendResult != WSAEWOULDBLOCK) {
        printf("Send failed with Winsock error: %d\n", WSAGetLastError());
        std::cout << "Closing session and socket.\r\n";
        closesocket(m_socket);
        return;
    }
}

void TelnetSession::initialise()
{
    // get details of connection
    SOCKADDR_IN client_info = { 0 };
    int addrsize = sizeof(client_info);
    getpeername(m_socket, (struct sockaddr*)&client_info, &addrsize);

    char ip[16];
    inet_ntop(AF_INET, &client_info.sin_addr, &ip[0], 16);

    std::cout << "Client " << ip << " connected...\n";

    // Set the connection to be non-blocking
    u_long iMode = 1;
    ioctlsocket(m_socket, FIONBIO, &iMode);

    // Set NVT mode to say that I will echo back characters.
    u_long iSendResult;
    unsigned char willEcho[3] = { 0xff, 0xfb, 0x01 };
    iSendResult = send(m_socket, (char *)willEcho, 3, 0);

    // Set NVT requesting that the remote system not/dont echo back characters
    unsigned char dontEcho[3] = { 0xff, 0xfe, 0x01 };
    iSendResult = send(m_socket, (char *)dontEcho, 3, 0);

    // Set NVT mode to say that I will supress go-ahead. Stops remote clients from doing local linemode.
    unsigned char willSGA[3] = { 0xff, 0xfb, 0x03 };
    iSendResult = send(m_socket, (char *)willSGA, 3, 0);

    if (m_telnetServer->connectedCallback())
        m_telnetServer->connectedCallback()(shared_from_this());
}

void TelnetSession::stripNVT(std::string &buffer)
{
    size_t found;
    do
    {
        unsigned char findChar = 0xff;
        found = buffer.find_first_of((char)findChar);
        if (found != std::string::npos && (found + 2) <= buffer.length() - 1)
        {
            buffer.erase(found, 3);
        }
    } while (found != std::string::npos);
}

void TelnetSession::stripEscapeCharacters(std::string &buffer)
{
    size_t found;

    std::array<std::string, 4> cursors = { ANSI_ARROW_UP, ANSI_ARROW_DOWN, ANSI_ARROW_RIGHT, ANSI_ARROW_LEFT };

    for (auto c : cursors)
    {
        do
        {
            found = buffer.find(c);
            if (found != std::string::npos)
            {
                buffer.erase(found, c.length());
            }
        } while (found != std::string::npos);
    }
}

bool TelnetSession::processBackspace(std::string &buffer)
{
    bool foundBackspaces = false;
    size_t found;
    do
    {
        // Need to handle both \x7f and \b backspaces
        unsigned char findChar = '\x7f';
        found = buffer.find_first_of((char)findChar);
        if (found == std::string::npos)
        {
            findChar = '\b';
            found = buffer.find_first_of((char)findChar);
        }

        if (found != std::string::npos)
        {
            if (buffer.length() > 1)
                buffer.erase(found - 1, 2);
            else
                buffer = "";
            foundBackspaces = true;
        }
    } while (found != std::string::npos);
    return foundBackspaces;
}

void TelnetSession::addToHistory(std::string line)
{
    // Add it to the history
    if (line != (m_history.size() > 0 ? m_history.back() : "") && line != "")
    {
        m_history.push_back(line);
        if (m_history.size() > 50)
            m_history.pop_front();
    }
    m_historyCursor = m_history.end();
}

bool TelnetSession::processCommandHistory(std::string &buffer)
{
    // Handle up and down arrow actions
    if (m_telnetServer->interactivePrompt())
    {
        if (buffer.find(ANSI_ARROW_UP) != std::string::npos && m_history.size() > 0)
        {
            if (m_historyCursor != m_history.begin())
            {
                m_historyCursor--;
            }
            buffer = *m_historyCursor;

            // Issue a cursor command to counter it
            u_long iSendResult;
            iSendResult = send(m_socket, ANSI_ARROW_DOWN.c_str(), (u_long)ANSI_ARROW_DOWN.length(), 0);
            return true;
        }
        if (buffer.find(ANSI_ARROW_DOWN) != std::string::npos && m_history.size() > 0)
        {
            if (next(m_historyCursor) != m_history.end())
            {
                m_historyCursor++;
            }
            buffer = *m_historyCursor;

            // Issue a cursor command to counter it
            u_long iSendResult;
            iSendResult = send(m_socket, ANSI_ARROW_UP.c_str(), (u_long)ANSI_ARROW_UP.length(), 0);
            return true;
        }
        if (buffer.find(ANSI_ARROW_LEFT) != std::string::npos || buffer.find(ANSI_ARROW_RIGHT) != std::string::npos)
        {
            // Ignore left and right and just reprint buffer
            return true;
        }
    }
    return false;
}

std::vector<std::string> TelnetSession::getCompleteLines(std::string &buffer)
{
    // Now find all new lines (<CR><LF>) and place in a vector and delete from buffer
    
    char CRLF[2] = { 0x0D, 0x0A };
    std::vector<std::string> lines;
    size_t found;
    do
    {
        found = buffer.find("\r\n");
        if (found != std::string::npos)
        {
            lines.push_back(buffer.substr(0, found));
            buffer.erase(0, found + 2);
        }
    } while (found != std::string::npos);

    return lines;
}

void TelnetSession::update()
{
    int  readBytes;
    char recvbuf[DEFAULT_BUFLEN];
    u_long  recvbuflen = DEFAULT_BUFLEN;

    readBytes = recv(m_socket, recvbuf, recvbuflen, 0);

    // Check for errors from the read
    int error = WSAGetLastError();
    if (error != WSAEWOULDBLOCK && error != 0)
    {
        std::cout << "Receive failed with Winsock error code: " << error << "\r\n";
        std::cout << "Closing session and socket.\r\n";
        closesocket(m_socket);
        return;
    }

    if (readBytes > 0) {
        // Echo it back to the sender
        echoBack(recvbuf, readBytes);

        // we've got to be careful here. Telnet client might send null characters for New Lines mid-data block. We need to swap these out. recv is not null terminated, so its cool
        for (int i = 0; i < readBytes; i++)
        {
            if (recvbuf[i] == 0x00)
                recvbuf[i] = 0x0A;      // New Line
        }

        // Add it to the received buffer
        m_buffer.append(recvbuf, readBytes);

        stripNVT(m_buffer);                         // Remove telnet negotiation sequences

        bool requirePromptReprint = false;

        if (m_telnetServer->interactivePrompt())
        {
            if (processCommandHistory(m_buffer))   // Read up and down arrow keys and scroll through history
                requirePromptReprint = true;
            stripEscapeCharacters(m_buffer);

            if (processBackspace(m_buffer))
                requirePromptReprint = true;
        }

        auto lines = getCompleteLines(m_buffer);
        for (auto line : lines)
        {
            if (m_telnetServer->newLineCallBack())
                m_telnetServer->newLineCallBack()(shared_from_this(), line);
            
            addToHistory(line);
        }

        if (m_telnetServer->interactivePrompt() && requirePromptReprint)
        {
            eraseLine();
            sendPromptAndBuffer();
        }
    }
}

void TelnetSession::UNIT_TEST()
{
    /* stripNVT */
    std::cout << "TEST: stripNVT\n";
    std::string origData = "12345";
    std::string data = origData;
    unsigned char toStrip[3] = { 255, 251, 1 };
    data.insert(2, (char *)toStrip, 3);
    TelnetSession::stripNVT(data);

    assert(origData == data);

    /* processBackspace */
    std::cout << "TEST: handleBackspace\n";
    std::string bkData = "123455\x7f";
    bool bkResult = TelnetSession::processBackspace(bkData);
    assert(bkData == "12345");
    assert(bkResult == true);

    /* getCompleteLines */
    std::cout << "TEST: getCompleteLines\n";
    std::string multiData = "LINE1\r\nLINE2\r\nLINE3\r\n";
    auto lines = TelnetSession::getCompleteLines(multiData);

    assert(lines.size() == 3);
    assert(lines[0] == "LINE1");
    assert(lines[1] == "LINE2");
    assert(lines[2] == "LINE3");
}

/* ------------------ Telnet Server -------------------*/
bool TelnetServer::initialise(u_long listenPort, std::string promptString)
{
    if (m_initialised)
    {
        std::cout << "This Telnet Server instance has already been initialised. Please shut it down before reinitialising it.";
        return false;
    }

    m_listenPort = listenPort;
    m_promptString = promptString;

    std::cout << "Starting Telnet Server on port " << std::to_string(m_listenPort) << "\n";

    WSADATA wsaData;
    int iResult;

    m_listenSocket = INVALID_SOCKET;

    struct addrinfo *result = NULL;
    struct addrinfo hints;

    // Initialize Winsock
    iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        printf("WSAStartup failed with error: %d\n", iResult);
        return false;
    }

    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;

    // Resolve the server address and port
    iResult = getaddrinfo(NULL, std::to_string(m_listenPort).c_str(), &hints, &result);
    if (iResult != 0) {
        printf("getaddrinfo failed with error: %d\n", iResult);
        return false;
    }

    // Create a SOCKET for connecting to server
    m_listenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (m_listenSocket == INVALID_SOCKET) {
        printf("socket failed with error: %ld\n", WSAGetLastError());
        freeaddrinfo(result);
        return false;
    }

    // Setup the TCP listening socket
    iResult = bind(m_listenSocket, result->ai_addr, (int)result->ai_addrlen);
    if (iResult == SOCKET_ERROR) {
        printf("bind failed with error: %d\n", WSAGetLastError());
        freeaddrinfo(result);
        closesocket(m_listenSocket);
        return false;
    }

    freeaddrinfo(result);

    iResult = listen(m_listenSocket, SOMAXCONN);
    if (iResult == SOCKET_ERROR) {
        printf("listen failed with error: %d\n", WSAGetLastError());
        closesocket(m_listenSocket);
        return false;
    }

    m_initialised = true;
    return true;
}

void TelnetServer::acceptConnection()
{
    SOCKET ClientSocket = INVALID_SOCKET;
    ClientSocket = accept(m_listenSocket, NULL, NULL);
    if (ClientSocket == INVALID_SOCKET) {
        printf("accept failed with error: %d\n", WSAGetLastError());
        closesocket(m_listenSocket);
        return;
    }
    else
    {
        SP_TelnetSession s = std::make_shared < TelnetSession >(ClientSocket, shared_from_this());
        m_sessions.push_back(s);
        s->initialise();        
    }
}

void TelnetServer::update()
{
    // See if connection pending on the listening socket
    fd_set readSet;
    FD_ZERO(&readSet);
    FD_SET(m_listenSocket, &readSet);
    timeval timeout;
    timeout.tv_sec = 0;  // Zero timeout (poll)
    timeout.tv_usec = 0;

    if (select(m_listenSocket, &readSet, NULL, NULL, &timeout) == 1)
    {
        // There is a connection pending, so accept it.
        acceptConnection();
    }

    // Update all the telnet Sessions that are currently in flight.
    for (SP_TelnetSession ts : m_sessions)
    {
        ts->update();
    }
}

void TelnetServer::shutdown()
{
    // Attempt to cleanly close every telnet session in flight.
    for (SP_TelnetSession ts : m_sessions)
    {
        ts->closeClient();
    }
    m_sessions.clear();

    // No longer need server socket so close it.
    closesocket(m_listenSocket);
    m_initialised = false;
}