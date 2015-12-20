README for CS 61 Problem Set 4
------------------------------

In this problem set we built a simple shell capable of
running simple commands, background commands, conditional
commands (&& and ||), redirections, and pipes. 

All of the code I wrote for this problem is located in sh61.c.

However, there was some functionality that the instructors provided for us.
Namley, they provided most of the main function is sh61.c, which 
is the function responsible for running the shell.  They also provided
everything in the file helpers.c, which included a method, parse_shell_token,
that parsed input from the command line, one token at a time,
differentiating between different delimiters, and a function called set_forground, which took as argument
a process group id, and set that pgid to be the foreground process group.

Other than that eveything else was written by me. 


You can read the spec for what was expected at:

http://cs61.seas.harvard.edu/wiki/2015/Shell
