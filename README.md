# proxy_server
My college assignment for the System Programming class

This program is basically developed and tested on Ubuntu 16.04 LTS 64bits.
HTTP requests from Firefox browser are handled and send response from web server to browser by this program.
HTTP responses received from the web server are stored in the cache directory, which is automatically made under the home directory.
Log files that record the operation of the server are also recorded in the log.txt under log directory, which also exists under the home directory.

When testing this program, be sure to set up a manual proxy in your browser!
The port used 39999, please enter your ip address! (use ifconfig)

*26/06/2022 Update
add log file for observating server operation

*22/07/2022 Update
add Connection to a Web server using the http protocol from a Firefox browser
cached sites bring up existing information; otherwise, send http requests to the web server
add signal that works If the program does not receive a response from web server within 10 seconds
