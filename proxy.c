#include <stdio.h>
#include "csapp.h"

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";
static const char *host_hdr = "Host: ";
static const char *connection_hdr = "Connection: close\r\n";
static const char *proxy_hdr = "Proxy-Connection: close\r\n";
static const char *user_agent_const = "User-Agent: ";
static const char *connection_const = "Connection: ";
static const char *proxy_const = "Proxy-Connection: ";

/*
Parses a given uri and extracts hostname, port, and query info.
*/
void parse_uri(char *uri, char *host_name, int *portnum, char *query)
{
    char *ptr;
    // first check if uri starts with http://
    // update the ptr accordingly
    if(strncmp(uri, "http://", 7) == 0)
    {
        // skip http://
        ptr = uri+7; 
    }
    else
    {
        ptr = uri;
    }

    // find port or querry section
    // update the host name as we go
    while(*ptr != '/' && *ptr != ':')
    {
        *host_name = *ptr;
        ptr += 1;
        host_name +=1;
    }
    *host_name = '\0';

    // at this point we're at the port or query section
    if(*ptr == ':') // port section
    {
        ptr+=1;
        // load in the port number and query
        sscanf(ptr, "%d%s", portnum, query);
    }
    else
    {
        *portnum = 80; // default port
        strcpy(query, ptr); // copy the query
    }
}

/*
Determines if a string starts with a given string. 
*/
int starts_with_str(char *header, const char* cmpto)
{
    int len = strlen(cmpto);
    if(strncmp(header, cmpto, len) == 0)
    {
        return 1;
    }
    else return 0;
}

/*
Creates a header string to send to server. 
*/
void make_header(rio_t rio, char* headers_str, char* host_name, char* query)
{
    char buf[MAXLINE], host_header[MAXLINE];
    int host_flag = 1, user_agent_flag = 1;

    // add default headers and format GET
    sprintf(headers_str, "GET %s HTTP/1.0\r\n", query);
    strcat(headers_str, connection_hdr);
    strcat(headers_str, proxy_hdr);

    // check for headers the server sent
    if(strcmp(rio.rio_bufptr, ""))
    {
        while(rio_readlineb(&rio, buf, MAXLINE))
        {
            if(strcmp(buf, "\r\n") == 0) break; // we've hit the end

            // does the received data start with a host header
            if(starts_with_str(buf, host_hdr))
            {
                // add the header and set the flag
                strcat(headers_str, buf);
                host_flag = 0;
            }

            // does the received data start with a user_agent header
            else if(starts_with_str(buf, user_agent_const))
            {
                // add the header and set the flag
                strcat(headers_str, buf);
                user_agent_flag = 0;
            }

            // does the received data start with a connection header or proxy header
            else if(!(starts_with_str(buf, connection_const) || starts_with_str(buf, proxy_const)))
            {
                // additional header to add
                strcat(headers_str, buf);
            }
        }
    }

    // check flags
    if(host_flag)
    {
        // no given host header, need to add our own
        sprintf(host_header, "Host: %s\r\n", host_name);
        strcat(headers_str, host_header);
    }
    if(user_agent_flag)
    {
        // no given user agent flag, add our own
        strcat(headers_str, user_agent_hdr);
    }

    // end of request, add \r\n to end
    strcat(headers_str, "\r\n");
}

/*
Forwards teh request from client to server.
*/
void forward_request(int clientfd, rio_t rio, char* uri)
{
    int serverfd; 
    char host_name[MAXLINE], port[8], query[MAXLINE], headers[MAXLINE] = "", response[MAXLINE] = "";
    int portnum;
    size_t length;
    rio_t server_rio;
    parse_uri(uri, host_name, &portnum, query);

    sprintf(port, "%d", portnum); //writes the port in string format

    // open the server connection
    if((serverfd = open_clientfd(host_name, port)) < 0)
    {
        return;
    }

    // make the headers
    make_header(rio, headers, host_name, query);
    size_t n = strlen(headers);

    // write to server
    Rio_readinitb(&server_rio, serverfd);
    Rio_writen(serverfd, headers, n);

    // tell the server we're done sending info
    shutdown(serverfd, SHUT_WR);

    // read the response from the server
    while((length = Rio_readlineb(&server_rio, response, MAXLINE)) > 0)
    {
        // send the reponse to the client
        printf("%ld bytes read from server\n", length);
        Rio_writen(clientfd, response, length);
    }

    printf("Response fowarded, closing connection to server\n");
    Close(serverfd); // close the server fd
}

/*
Handle the proxy request.
*/
void handle_request(int connfd)
{
    size_t n;
    char buf[MAXLINE], method[8], uri[MAXLINE], version[8];
    rio_t rio;
    memset(rio.rio_buf, 0, 8192);
    Rio_readinitb(&rio, connfd);

    // recieves the http request from client
    if((n = Rio_readlineb(&rio, buf, MAXLINE)) == 0)
    { 
        // return as there is nothing to read
        return;
    }

    // print how many bytes of data we recieved
    printf("server received %d bytes\n", (int)n);

    // print the buffer
    printf("%s", buf);

    // get the method, uri and version
    sscanf(buf, "%s %s %s", method, uri, version); 

    // check to make sure we have a valid method
    if(strcasecmp(method, "GET") != 0)
    {
        printf("Proxy can only take HTTP & GET requests\n");
        return;
    }

    // forward the request to the server
    forward_request(connfd, rio, uri);
}

int main(int argc, char**argv)
{
    int listenfd, connfd;
    socklen_t clientlen;
    struct sockaddr_storage clientaddr; 
    char client_hostname[MAXLINE], client_port[MAXLINE];
    signal(SIGCHLD, SIG_IGN); // handles zombie processes

    if(argc != 2)
    {
        fprintf(stderr, "usage: %s <port>\n", argv[0]); // no port number provided
        return 1;
    }

    if((listenfd = open_listenfd(argv[1])) < 0)
    {
        printf("error opening port: %s\n", argv[1]);
        return 1;
    }

    printf("%s", user_agent_hdr);

    while (1) 
    {
        clientlen = sizeof(struct sockaddr_storage); /* Important! */
        connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
        Getnameinfo((SA *) &clientaddr, clientlen, 
         client_hostname, MAXLINE, client_port, MAXLINE, 0);
        printf("Connection accepted to (%s, %s)\n", client_hostname, client_port);

        // handle multiple requests
        pid_t pid = fork();
        if(pid == 0) // in child process
        {
            Close(listenfd); // child does not need it
            handle_request(connfd); // handle the request
            Close(connfd); // close the connection
            printf("Closed connection with (%s, %s)\n", client_hostname, client_port);
            exit(0); // exit the child
        }
        else // parent process
        {
            Close(connfd); //close the connection fd
        }
    }

    return 0;
}
