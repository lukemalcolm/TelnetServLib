/*
TelnetServLib 
=============

TelnetServLib is a very light ANSI Telnet Server library for use in apps with 'game
loops': i.e. update, render. It utilises TCP select to enable it to operate in the 
main thread.

License
=======

Copyright (c) 2015, Luke Malcolm
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimer in the documentation
and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

The views and conclusions contained in the software and documentation are those
of the authors and should not be interpreted as representing official policies,
either expressed or implied, of the FreeBSD Project.
*/

#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>
#include <string>
#include <memory>
#include <vector>
#include <functional>
#include <list>

class TelnetServer;
class TelnetSession;

const std::string ANSI_FG_BLACK   ("\x1b[30m");
const std::string ANSI_FG_RED     ("\x1b[31m");
const std::string ANSI_FG_GREEN   ("\x1b[32m");
const std::string ANSI_FG_YELLOW  ("\x1b[33m");
const std::string ANSI_FG_BLUE    ("\x1b[34m");
const std::string ANSI_FG_MAGENTA ("\x1b[35m");
const std::string ANSI_FG_CYAN    ("\x1b[36m");
const std::string ANSI_FG_WHITE   ("\x1b[37m");
const std::string ANSI_FG_DEFAULT ("\x1b[39m");

const std::string ANSI_BG_BLACK   ("\x1b[40m");
const std::string ANSI_BG_RED     ("\x1b[41m");
const std::string ANSI_BG_GREEN   ("\x1b[42m");
const std::string ANSI_BG_YELLOW  ("\x1b[43m");
const std::string ANSI_BG_BLUE    ("\x1b[44m");
const std::string ANSI_BG_MAGENTA ("\x1b[45m");
const std::string ANSI_BG_CYAN    ("\x1b[46m");
const std::string ANSI_BG_WHITE   ("\x1b[47m");
const std::string ANSI_BG_DEFAULT ("\x1b[49m");

const std::string ANSI_BOLD_ON       ("\x1b[1m");
const std::string ANSI_BOLD_OFF      ("\x1b[22m");

const std::string ANSI_ITALICS_ON    ("\x1b[3m");
const std::string ANSI_ITALCIS_OFF   ("\x1b[23m");

const std::string ANSI_UNDERLINE_ON  ("\x1b[4m");
const std::string ANSI_UNDERLINE_OFF ("\x1b[24m");

const std::string ANSI_INVERSE_ON    ("\x1b[7m");
const std::string ANSI_INVERSE_OFF   ("\x1b[27m");

const std::string ANSI_STRIKETHROUGH_ON  ("\x1b[9m");
const std::string ANSI_STRIKETHROUGH_OFF ("\x1b[29m");

const std::string ANSI_ERASE_LINE        ("\x1b[2K");
const std::string ANSI_ERASE_SCREEN      ("\x1b[2J");

const std::string ANSI_ARROW_UP("\x1b\x5b\x41");
const std::string ANSI_ARROW_DOWN("\x1b\x5b\x42");
const std::string ANSI_ARROW_RIGHT("\x1b\x5b\x43");
const std::string ANSI_ARROW_LEFT("\x1b\x5b\x44");


const std::string TELNET_ERASE_LINE      ("\xff\xf8");


class TelnetSession : public std::enable_shared_from_this < TelnetSession >
{
public:
    TelnetSession(SOCKET ClientSocket, std::shared_ptr<TelnetServer> ts) : m_socket(ClientSocket), m_telnetServer(ts) 
    {
        m_historyCursor = m_history.end();
    };

public:
    void sendLine(std::string data);    // Send a line of data to the Telnet Server
    void closeClient();                 // Finish the session

    static void UNIT_TEST();

protected:
    void initialise();                  // 
    void update();                      // Called every frame/loop by the Terminal Server

private:
    void sendPromptAndBuffer();         // Write the prompt and any data sat in the input buffer
    void eraseLine();                   // Erase all characters on the current line and move prompt back to beginning of line
    void echoBack(char * buffer, u_long length);
    static void stripNVT(std::string &buffer);
    static void stripEscapeCharacters(std::string &buffer);                 // Remove all escape characters from the line
    static bool processBackspace(std::string &buffer);                      // Takes backspace commands and removes them and the preceeding character from the m_buffer. // Handles arrow key actions for history management. Returns true if the input buffer was changed.
    void addToHistory(std::string line);                                    // Add a command into the command history
    bool processCommandHistory(std::string &buffer);                        // Handles arrow key actions for history management. Returns true if the input buffer was changed.
    static std::vector<std::string> getCompleteLines(std::string &buffer);  

private:
    SOCKET m_socket;                // The Winsock socket
    std::shared_ptr<TelnetServer> m_telnetServer; // Parent TelnetServer class
    std::string m_buffer;           // Buffer of input data (mid line)
    std::list<std::string>           m_history;  // A history of all completed commands
    std::list<std::string>::iterator m_historyCursor;

friend TelnetServer;
};

typedef std::shared_ptr<TelnetSession>   SP_TelnetSession;
typedef std::vector < SP_TelnetSession > VEC_SP_TelnetSession;

typedef std::function< void(SP_TelnetSession) >              FPTR_ConnectedCallback;
typedef std::function< void(SP_TelnetSession, std::string) > FPTR_NewLineCallback;

class TelnetServer : public std::enable_shared_from_this < TelnetServer >
{
public:
    TelnetServer() : m_initialised(false), m_promptString("") {};

    bool initialise(u_long listenPort, std::string promptString = "");
    void update();
    void shutdown();

public:
    void connectedCallback(FPTR_ConnectedCallback f) { m_connectedCallback = f; }
    FPTR_ConnectedCallback connectedCallback() const { return m_connectedCallback; }

    void newLineCallback(FPTR_NewLineCallback f) { m_newlineCallback = f; }
    FPTR_NewLineCallback newLineCallBack() const { return m_newlineCallback; }

    VEC_SP_TelnetSession sessions() const { return m_sessions; }

    bool interactivePrompt() const { return m_promptString.length() > 0; }
    void promptString(std::string prompt) { m_promptString = prompt; }
    std::string promptString() const { return m_promptString; }

private:
    void acceptConnection();

private:
    u_long m_listenPort;
    SOCKET m_listenSocket;
    VEC_SP_TelnetSession m_sessions;
    bool   m_initialised;
    std::string m_promptString;                     // A string that denotes the current prompt

protected:
    FPTR_ConnectedCallback m_connectedCallback;     // Called after the telnet session is initialised. function(SP_TelnetSession) {}
    FPTR_NewLineCallback   m_newlineCallback;       // Called after every new line (from CR or LF)     function(SP_TelnetSession, std::string) {}
};