#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <iostream>
#include <signal.h>
#include <sys/epoll.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/time.h>
#include <sys/resource.h>

using namespace std;

#define MAXFDS 128
#define EVENTS 100
#define PORT 8080
#define MAXEPOLLSIZE 1024*10

int epfd;

		bool
setNonBlock (int fd)
{
		int flags = fcntl (fd, F_GETFL, 0);
		flags |= O_NONBLOCK;
		if (-1 == fcntl (fd, F_SETFL, flags))
				return false;
		return true;
}

		void
cws_client_request (int connfd, struct epoll_event &ev)
{
		char buffer[1024*8] = {0};
		int ret;
		char *requestPath = NULL;
		char tmpPath[512] = {"./www/"};
		int pagesize = 0;
		ret = recv (connfd, buffer, sizeof (buffer) -1, 0);
		if (ret > 0) {
				if (strncmp (buffer, "GET ", 4) != 0)
						cerr << "bad request.\n" <<endl;

				if (strncmp (buffer, "GET /", 5) == 0) {

						if(strncmp (buffer, "GET / ", 6) == 0) {
								strcat (tmpPath, "/index.html");
								requestPath = tmpPath;
						}else{
								requestPath = buffer+5;
								char * pos = strstr(buffer+5, " ");
								strncat(tmpPath, requestPath, pos - requestPath);
								requestPath = tmpPath;
						}

				}
				char * badRequest = (char *)"<b>Error 404 Not Found.</b>";
				char * httpStatus200 = (char *)"HTTP/1.0 200 OK\r\nContent-Type:text/html\r\n\r\n";

				FILE * fp = fopen(requestPath, "r");
				FILE * connfp = fdopen(connfd, "w");
				if( connfp == NULL )
						cout <<"bad connfp"<<endl;
				if( fp == NULL ){
						setlinebuf(connfp);
						fwrite(badRequest, strlen(badRequest), 1, connfp);
						fclose(connfp);
				}else{
						setlinebuf(connfp);
						//fwrite(httpStatus200, strlen(httpStatus200), 1, connfp);
						//fflush(connfp);
						while ((ret = fread (buffer, 1, sizeof(buffer) -1, fp)) > 0){
								fwrite(buffer, 1, ret, connfp);
								pagesize += ret;
								fflush(connfp);
						}
						cout <<"pagesize:" << pagesize << endl;
						fclose(fp);
				}


		}

		//1
		close(connfd);
		epoll_ctl (epfd, EPOLL_CTL_DEL, connfd, &ev);
		//2
		//    ev.data.fd = connfd;
		//    ev.events = EPOLLOUT | EPOLLET;
		//    epoll_ctl (epfd, EPOLL_CTL_ADD, connfd, &ev);

}


		int
main (int /*argc*/, char ** /*argv[]*/)
{
		int fd, nfds;
		int on = 1;
		char buffer[512];
		struct sockaddr_in saddr, caddr;
		struct epoll_event ev, events[EVENTS];
		
		signal(SIGPIPE, SIG_IGN);
	//	signal(SIGCHLD, SIG_IGN);
	

		if(fork())
		{
			exit(0);
		}
	

		if (-1 == (fd = socket (AF_INET, SOCK_STREAM, 0)))
		{
				cout << "create socket error!" << endl;
				return -1;
		}

		epfd = epoll_create (MAXFDS);
		setsockopt (fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof (on));
		bzero (&saddr, sizeof (saddr));
		saddr.sin_family = AF_INET;
		saddr.sin_port = htons ((short) (PORT));
		saddr.sin_addr.s_addr = INADDR_ANY;
		if (-1 == bind (fd, (struct sockaddr *) &saddr, sizeof (saddr)))
		{
				cout << " cann't bind socket on server " << endl;
				return -1;
		}

		if (-1 == listen (fd, 32))
		{
				cout << "listen error" << endl;
				return -1;
		}

		ev.data.fd = fd;
		ev.events = EPOLLIN | EPOLLET;
		epoll_ctl (epfd, EPOLL_CTL_ADD, fd, &ev);
		for (;;)
		{
				nfds = epoll_wait (epfd, events, MAXFDS, -1);
				for (int i = 0; i < nfds; ++i)
				{
						if (fd == events[i].data.fd)
						{
								memset (&caddr, 0, sizeof (caddr));
								int len = sizeof (caddr);
								int cfd =
										accept (fd, (struct sockaddr *) &caddr, (socklen_t *) & len);
								if (-1 == cfd)
								{
										cout << "server has an error, now accept a socket fd" <<
												endl;
										break;
								}
								setNonBlock (cfd);
								ev.data.fd = cfd;
								ev.events = EPOLLIN | EPOLLET;
								epoll_ctl (epfd, EPOLL_CTL_ADD, cfd, &ev);
						}
						else if (events[i].data.fd & EPOLLIN)
						{
								bzero (&buffer, sizeof (buffer));
								cws_client_request (events[i].data.fd, ev);
						}
						else if (events[i].data.fd & EPOLLOUT)
						{
								ev.data.fd = events[i].data.fd;
								close(ev.data.fd);
								epoll_ctl (epfd, EPOLL_CTL_DEL, ev.data.fd, &ev);
						}
				}
		}
		if (fd > 0)
		{
				shutdown (fd, SHUT_RDWR);
				close (fd);
		}

		return 0;
}