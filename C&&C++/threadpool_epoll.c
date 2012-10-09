/* Linux 2.6 x86_64 only*/ 
#include <pthread.h> 
#include <string.h> 
#include <stdlib.h> 
#include <unistd.h> 
#include <stdio.h> 
#include <fcntl.h>

#include <arpa/inet.h> 
#include <sys/epoll.h> 
#include <sys/errno.h>
#include <sys/socket.h> 

#define THREAD_MAX 20 
#define LISTEN_MAX 20 
#define SERVER_IP "127.0.0.1" 

typedef struct {
    char ip4[128]; 
    int port; 
    int fd; 
} LISTEN_INFO; 

//���������� 
static LISTEN_INFO s_listens[LISTEN_MAX]; 

//�̳߳ز��� 
static unsigned int s_thread_para[THREAD_MAX][8];//�̲߳��� 
static pthread_t s_tid[THREAD_MAX];//�߳�ID 
pthread_mutex_t s_mutex[THREAD_MAX];//�߳��� 

//˽�к��� 
static int init_thread_pool(void);//��ʼ������
static int init_listen4(char *ip4, int port, int max_link); //��ʼ������

//�̺߳��� 
void * test_server4(unsigned int thread_para[]);

//�����ļ�������ΪNonBlock
bool setNonBlock(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    flags |= O_NONBLOCK;
    if(-1 == fcntl(fd, F_SETFL, flags))
        return false;
    return true;
}
 
int main(int argc, char *argv[])//�ͻ������� 
{ 
    //��ʱ���� 
    int i, j, rc; 

    int sock_listen; //�����׽��� 
    int sock_cli; //�ͻ������� 
    int listen_index; 

    int epfd; 
    int nfds; 
    struct epoll_event ev; 
    struct epoll_event events[LISTEN_MAX];
     
    socklen_t addrlen; //��ַ��Ϣ���� 
    struct sockaddr_in addr4; //IPv4��ַ�ṹ 

    //�̳߳س�ʼ�� 
    rc = init_thread_pool(); 
    if (0 != rc) exit(-1); 

    //��ʼ��������� 
    for(i = 0; i < LISTEN_MAX; i++) { 
        sprintf(s_listens[i].ip4, "%s", SERVER_IP); 
        s_listens[i].port = 40000 + i; 
        //�������� 
        rc = init_listen4(s_listens[i].ip4, s_listens[i].port, 64); 
        if (0 > rc) { 
            fprintf(stderr, "�޷�����������������%s:%d\r\n", s_listens[i].ip4, s_listens[i].port); 
            exit(-1); 
        } else {
            fprintf(stdout, "�Ѵ���������������%s:%d\r\n", s_listens[i].ip4, s_listens[i].port);  
        } 
        s_listens[i].fd = rc; 
    } 
     
    //���ü��� 
    epfd = epoll_create(8192); 
    for (i = 0; i < LISTEN_MAX; i++) { 
        //����epoll�¼����� 
        ev.events = EPOLLIN | EPOLLET;
        ev.data.u32 = i;//��¼listen�����±� 
        if (epoll_ctl(epfd, EPOLL_CTL_ADD, s_listens[i].fd, &ev) < 0) { 
            fprintf(stderr, "��epoll��������׽���ʧ��(fd =%d)\r\n", rc); 
            exit(-1); 
        } 
    } 
     
    //����ѭ�� 
    for( ; ; ) { 
        //�ȴ�epoll�¼� 
        nfds = epoll_wait(epfd, events, LISTEN_MAX, -1); 
        //����epoll�¼� 
        for(i = 0; i < nfds; i++) { 
            //���տͻ������� 
            listen_index = events[i].data.u32; 
            sock_listen = s_listens[listen_index].fd; 
            addrlen = sizeof(struct sockaddr_in); 
            bzero(&addr4, addrlen); 
             
            sock_cli = accept(sock_listen, (struct sockaddr *)&addr4, &addrlen); 
            if(0 > sock_cli) { 
                fprintf(stderr, "���տͻ�������ʧ��\n"); 
                continue; 
            } else {
                char *myIP = inet_ntoa(addr4.sin_addr);
                printf("accept a connection from %s...\n", myIP); 
            } 
             
            setNonBlock(sock_cli);
            //��ѯ�����̶߳� 
            for(j = 0; j < THREAD_MAX; j++) { 
                if (0 == s_thread_para[j][0]) break; 
            } 
            if (j >= THREAD_MAX) { 
                fprintf(stderr, "�̳߳�����, ���ӽ�������\r\n"); 
                shutdown(sock_cli, SHUT_RDWR); 
                close(sock_cli); 
                continue; 
            } 
            //�����йز��� 
            s_thread_para[j][0] = 1;//���û��־Ϊ"�" 
            s_thread_para[j][1] = sock_cli;//�ͻ������� 
            s_thread_para[j][2] = listen_index;//�������� 
            //�߳̽��� 
            pthread_mutex_unlock(s_mutex + j); 
        }//end of for(i;;) 
    }//end of for(;;) 

    exit(0); 
} 

static int init_thread_pool(void) 
{ 
    int i, rc; 

    //��ʼ���̳߳ز��� 
    for(i = 0; i < THREAD_MAX; i++) { 
        s_thread_para[i][0] = 0;//�����߳�ռ�ñ�־Ϊ"����" 
        s_thread_para[i][7] = i;//�̳߳����� 
        pthread_mutex_lock(s_mutex + i);// ����ط�ΪʲôҪ����������������������ʱ�᲻�ɹ� 
    } 

    //�����̳߳� 
    for(i = 0; i < THREAD_MAX; i++) { 
        rc = pthread_create(s_tid + i, 0, (void* (*)(void *))test_server4, (void *)(s_thread_para[i])); 
        if (0 != rc) { 
            fprintf(stderr, "�̴߳���ʧ��\n"); 
            return(-1); 
        } 
    } 

    //�ɹ����� 
    return(0); 
} 

static int init_listen4(char *ip4, int port, int max_link) 
{ 
    //��ʱ���� 
    int sock_listen4; 
    struct sockaddr_in addr4; 
    unsigned int optval; 
    struct linger optval1; 

    //��ʼ�����ݽṹ 
    bzero(&addr4, sizeof(addr4)); 
    //inet_pton�����ʮ����IPת��Ϊ����
    inet_pton(AF_INET, ip4, &(addr4.sin_addr)); 
    addr4.sin_family = AF_INET; 
    //htons���޷���short�������ֽ���(x86:Big-Endian)ת��Ϊ�����ֽ���
    addr4.sin_port = htons(port); 
     
    //���������͵�SOCKET 
    sock_listen4 = socket(AF_INET, SOCK_STREAM, 0); 
    if (0 > sock_listen4) {
        fprintf(stderr, "����socket�쳣, sock_listen4:%d\n", sock_listen4);
        perror("����socket�쳣");
        return(-1); 
    }
     
    //����SO_REUSEADDRѡ��(��������������) 
    optval = 0x1; 
    setsockopt(sock_listen4, SOL_SOCKET, SO_REUSEADDR, &optval, 4); 

    //����SO_LINGERѡ��(����CLOSE_WAIT��ס�����׽���) 
    optval1.l_onoff = 1; 
    optval1.l_linger = 60; 
    setsockopt(sock_listen4, SOL_SOCKET, SO_LINGER, &optval1, sizeof(struct linger)); 

    if (0 > bind(sock_listen4, (struct sockaddr *)&addr4, sizeof(addr4))) { 
        fprintf(stderr, "bind socket�쳣, sock_listen4:%d\n", sock_listen4);
        perror("bind socket�쳣");
        close(sock_listen4);
        return(-1); 
    } 

    if (0 > listen(sock_listen4, max_link)) { 
        fprintf(stderr, "listen socket�쳣, sock_listen4:%d\n", sock_listen4);
        perror("listen socket�쳣");
        close(sock_listen4); 
        return(-1); 
    } 

    return (sock_listen4); 
} 

void * test_server4(unsigned int thread_para[]) 
{ 
    //��ʱ���� 
    int sock_cli; //�ͻ������� 
    int pool_index; //�̳߳����� 
    int listen_index; //�������� 

    char buff[32768]; //���仺���� 
    int i, j, len; 
    char *p; 

    //�߳����봴���� 
    pthread_detach(pthread_self()); 
    pool_index = thread_para[7]; 

wait_unlock: 
    pthread_mutex_lock(s_mutex + pool_index);//�ȴ��߳̽��� 

    //�̱߳������ݸ��� 
    sock_cli = thread_para[1];//�ͻ������� 
    listen_index = thread_para[2];//�������� 

    //�������� 
    len = recv(sock_cli, buff, sizeof(buff), MSG_NOSIGNAL); 
    printf("%s\n", buff);
     
    //������Ӧ 
    p = buff; 
    //HTTPͷ 
    p += sprintf(p, "HTTP/1.1 200 OK\r\n"); 
    p += sprintf(p, "Content-Type: text/html\r\n"); 
    p += sprintf(p, "Connection: closed\r\n\r\n"); 
    //ҳ�� 
    p += sprintf(p, "<html>\r\n<head>\r\n"); 
    p += sprintf(p, "<meta content=\"text/html; charset=UTF-8\" http-equiv=\"Content-Type\">\r\n"); 
    p += sprintf(p, "</head>\r\n"); 
    p += sprintf(p, "<body style=\"background-color: rgb(229, 229, 229);\">\r\n"); 

    p += sprintf(p, "<center>\r\n"); 
    p += sprintf(p, "<H3>����״̬</H3>\r\n"); 
    p += sprintf(p, "<p>��������ַ %s:%d</p>\r\n", s_listens[listen_index].ip4, s_listens[listen_index].port); 
    j = 0; 
    for(i = 0; i < THREAD_MAX; i++) { 
        if (0 != s_thread_para[i][0]) j++; 
    } 
    p += sprintf(p, "<H3>�̳߳�״̬</H3>\r\n"); 
    p += sprintf(p, "<p>�̳߳����� %d ��߳����� %d</p>\r\n", THREAD_MAX, j); 
    p += sprintf(p, "</center></body></html>\r\n"); 
    len = p - buff; 
     
    //������Ӧ 
    send(sock_cli, buff, len, MSG_NOSIGNAL); 
    memset(buff, 0, 32768);
     
    //�ͷ����� 
    shutdown(sock_cli, SHUT_RDWR); 
    close(sock_cli); 

    //�߳�������� 
    thread_para[0] = 0;//�����߳�ռ�ñ�־Ϊ"����" 
    goto wait_unlock; 

    pthread_exit(NULL); 
} 