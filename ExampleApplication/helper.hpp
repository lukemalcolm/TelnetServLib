/*

  HELPER.H
  ========
  (c) Paul Griffiths, 1999
  Email: mail@paulgriffiths.net

  Interface to socket helper functions. 

  Many of these functions are adapted from, inspired by, or 
  otherwise shamelessly plagiarised from "Unix Network 
  Programming", W Richard Stevens (Prentice Hall).

*/


#ifndef PG_SOCK_HELP
#define PG_SOCK_HELP

#define LISTENQ        (1024)   /*  Backlog for listen()   */


/*  Function declarations  */

size_t Readline(int fd, void *vptr, size_t maxlen);
size_t Writeline(int fc, const void *vptr, size_t maxlen);


#endif  /*  PG_SOCK_HELP  */

