/***************************************************************************
** Name         : blogspider.c
** Description  : Download CSDN Blog. 
**                This program can backup everybody's csdn blog.
****************************************************************************/
//���Ժ�
#if 0
#define SPIDER_DEBUG
#endif

//linux��ͷ�ļ�����
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <sys/time.h>
#include <netdb.h>


#define BUFSIZE		     1024

//html�ļ����﷨
#define HTML_ARTICLE     ("<span class=\"link_title\">")
#define HTML_MULPAGE     ("class=\"pagelist\"")
#define BLOG_NEXT_LIST   ("article/list")
#define BLOG_TITLE       ("title=\"")
#define BLOG_HREF        ("<a href=\"")
#define BLOG_DATE        ("<span class=\"link_postdate\">")
#define BLOG_READ        ("<span class=\"link_view\"")
#define BLOG_COMMENT     ("<span class=\"link_comments\"")
#define BLOG_SPAN_HEAD   ("<span>")
#define BLOG_SPAN_END    ("</span>")
#define BLOG_RANK        ("blog_rank")
#define BLOG_LI          ("<li>")
#define BLOG_INDEX       ("index.html")
#define CSDN_BLOG_URL    ("http://blog.csdn.net")
#define CSDN_BLOG_HOST   ("blog.csdn.net")
#define CSDN_BLOG_PORT   (80)

//���߳�ʹ�ã�Ԥ��
#define BLOG_LOCK        (10)
#define BLOG_UNLOCK      (11)

#define BLOG_DOWNLOAD    (20)
#define BLOG_UNDOWNLOAD  (21)


//��Ҫ����������Ĳ�����Ϣ
typedef struct tag_blog_info {
	char *b_url;           /*��ַ*/
	char *b_host;          /*��վ������������*/
	char *b_page_file;     /*ҳ���ļ�����*/
	char *b_local_file;    /*���ر�����ļ�����*/
	char *b_title;         /*��������*/
	char *b_date;          /*���ͷ�������*/
	int   b_port;          /*��ַ�˿ں�*/
	int   b_sockfd;        /*�����׽���*/
	int   b_reads;         /*�Ķ�����*/
	int   b_comments;      /*���۴���*/
	int   b_download;      /*����״̬*/
	int   b_lock;          /*������*/
	int   b_seq_num;       /*���*/
}blog_info;

//����������
typedef struct tag_blog_spider {
	blog_info *blog;
	struct tag_blog_spider *next;
}blog_spider;

//���ͻ�����Ϣ
typedef struct tag_blog_rank {
	int   b_page_total;    /*������ҳ��*/
	char *b_title;         /*���ͱ���*/
	char *b_page_view;     /*���ͷ�����*/
	char *b_integral;      /*���ͻ���*/
	char *b_ranking;       /*��������*/
	char *b_original;      /*����ԭ����������*/
	char *b_reship;        /*����ת����������*/
	char *b_translation;   /*����������������*/
	char *b_comments;      /*������������*/
}blog_rank;

//ȫ�ֱ���
static int g_seq_num = 0;
static char csdn_id[255];
static struct hostent *web_host;
/* hostent ������Ϣ�Ľṹ�� 
gethostbyname()����һ��hostentָ��
struct hostent  
{
����char  *h_name; //��ַ����ʽ����
  ��char  **h_aliases;
	int   h_addrtype;//��ַ����(AF_INET)
	int   h_length;   //��ַ�ı��س���
	char  **h_addr_list;//���������ַָ��(�����ֽ���)
���� #define h_addr h_addr_list[0] //h_addr_list�еĵ�һ����ַ
 };
 */

static char *strrstr(const char *s1, const char *s2);//��s1�ַ����в���s2�ַ���,�������һ�γ��ֵĵ�ַ 
static char *strfchr(char *s);//���˵�s�ַ����в�������ַ�
static int  init_spider(blog_spider **spider);//��ʼ����������ڵ�,����ʹ��ָ���ָ��,����ﲻ��Ԥ��Ч��
static int  init_rank(blog_rank **rank);//��ʼ�����ʹ�Ż�����Ϣ�Ľṹ��
static void insert_spider(blog_spider *spider_head, blog_spider *spider);//�����Ͳ�����������  
static int  spider_size(blog_spider *spider_head);//������������ĳ���  
static void print_spider(blog_spider *spider_head);//��ӡ��������,���浽��ǰĿ¼��*.log�ļ�
static void print_rank(blog_rank *rank);//��ӡ���ͻ�����Ϣ  
static void free_spider(blog_spider *spider_head);//�ͷ���������Ŀռ� 
static void free_rank(blog_rank *rank);//�ͷŲ��ͻ�����Ϣ�Ŀռ�  
static int  get_blog_info(blog_spider *spider_head, blog_rank *rank);//�Ӳ�����ҳ��ȡ���ͱ���,�������µ���ҳ��,����,��������Ϣ
static int  analyse_index(blog_spider *spider_head);//����ÿһҳ���͵���Ϣ,����ӽ��������� 
static int  download_index(blog_spider *spider_head);//���ز�����ҳ  
static int  download_blog(blog_spider *spider);//����ÿһƪ����    
static int  get_web_host(const char *hostname);//�õ�"blog.csdn.net"��վ��������Ϣ
static int  connect_web(const blog_spider *spider);//��ʼ��socket,��������վ������  
static int  send_request(const blog_spider * spider);//����վ��������������
static int  recv_response(const blog_spider * spider);//������վ����������Ӧ��Ϣ  


/**************************************************************
strrstr  : ����ָ���ַ���, �������һ�γ��ֵĵ�ַ, �Լ�ʵ��
***************************************************************/
static char *strrstr(const char *s1, const char *s2)
{
	//������ָ�봫�ݶ�Ӧ�ü���Ƿ�ΪNULL����ʹ��assert
	int len2;
	char *ps1;

	if (!(len2 = strlen(s2))) {       //s2����Ϊ0ʱ
		return (char *)s1;
	}
	
	ps1 = (char *)s1 + strlen(s1) - 1; //s1�ַ�������ַ���ַ
	ps1 = ps1 - len2 + 1;              

	while (ps1 >= s1) {             //����Ƚϣ��õ����һ��ƥ��
		if ((*ps1 == *s2) && (strncmp(ps1, s2, len2) == 0)) {
			return (char *)ps1;
		}
		ps1--;
	}

	return NULL;
}

/*********************************************************
strfchr  : ����ָ���ַ����в�������ַ�, ������
��û��ɾ����Щ��������ַ�,�򴴽��ļ���ʱ�򽫻����
*********************************************************/
static char *strfchr(char *s)
{
	char *p = s;
	
	while (*p) {
		if (('/' == *p) || ('?' == *p)) {
			*p = 0;
			strcat(s, "xxx"); //��xxx��ӵ�s�У���\0
			
			return p;
		}
		p++;
	}
	
	return NULL;
}

/*********************************************************
��ʼ���������������ڵ�, ����ռ䲢����ֵ
*********************************************************/
static int init_spider(blog_spider * * spider)
{
	 //������
	*spider = (blog_spider *)malloc(sizeof(blog_spider));
	if (NULL == *spider) {   //����ռ�δ�ɹ�
		#ifdef SPIDER_DEBUG
		fprintf(stderr, "malloc: %s\n", strerror(errno));
		#endif
		return -1;
	}
	
    //������Ϣ
	(*spider)->blog = (blog_info *)malloc(sizeof(blog_info));
	if (NULL == (*spider)->blog) {
		#ifdef SPIDER_DEBUG
		fprintf(stderr, "malloc: %s\n", strerror(errno));
		#endif
		free(*spider);
		return -1;
	}

    //��ʼ�����
	//strdup ����malloc�����ڴ� ����ʱ����free
	(*spider)->blog->b_url           = NULL;
	(*spider)->blog->b_host          = strdup(CSDN_BLOG_HOST); 
	(*spider)->blog->b_page_file     = NULL;
	(*spider)->blog->b_local_file    = NULL;
	(*spider)->blog->b_title         = NULL;
	(*spider)->blog->b_date          = NULL;
	(*spider)->blog->b_port          = CSDN_BLOG_PORT;
	(*spider)->blog->b_sockfd        = 0;
	(*spider)->blog->b_reads         = 0;
	(*spider)->blog->b_comments      = 0;
	(*spider)->blog->b_download      = BLOG_UNDOWNLOAD;
	(*spider)->blog->b_lock          = BLOG_UNLOCK;
	(*spider)->blog->b_seq_num       = 0;
		
	(*spider)->next = NULL;

	return 0;
}

/*********************************************************
��ʼ�����ͻ�����Ϣ�ṹ��,�������¼�������:
1.����ҳ����ҳ��
2.���ͱ���
3.���ͷ�����
4.���ͻ���
5.��������
6.����ԭ����������
7.����ת����������
8.����������������
9.������������
*********************************************************/
static int init_rank(blog_rank **rank)
{
	*rank = (blog_rank *)malloc(sizeof(blog_rank));
	if (NULL == *rank) {
		#ifdef SPIDER_DEBUG
		fprintf(stderr, "malloc: %s\n", strerror(errno));
		#endif
		return -1;
	}

	(*rank)->b_page_total      = 0;
	(*rank)->b_title           = NULL;
	(*rank)->b_page_view       = NULL;
	(*rank)->b_integral        = NULL;
	(*rank)->b_ranking         = NULL;
	(*rank)->b_original        = NULL;
	(*rank)->b_reship          = NULL;
	(*rank)->b_translation     = NULL;
	(*rank)->b_comments        = NULL;

	return 0;
}

/*********************************************************
����������ڵ������������
*********************************************************/
static void insert_spider(blog_spider * spider_head, blog_spider * spider)
{
	blog_spider *pspider;

	pspider = spider_head;

	while (pspider->next) {   //�ҵ������
		pspider = pspider->next;
	}
	
	pspider->next = spider;  //������
}

/*********************************************************
��������������
*********************************************************/
static int spider_size(blog_spider * spider_head)
{
	int count = 0;
	blog_spider *pspider;

	pspider = spider_head;

	while (pspider->next) {
		pspider = pspider->next;
		count++;
	}
	
	return count;
}

/*********************************************************
��������������ݴ�ӡ��log�ļ�,��ʽΪ"csdn_id.log",����
�ҵĲ��͵ĵ�ַΪ: "gzshun.log"
*********************************************************/
static void print_spider(blog_spider *spider_head)
{
	char file[BUFSIZE] = {0};
	char tmpbuf[BUFSIZE] = {0};
	blog_spider *spider;
	FILE *fp;

	sprintf(file, "%s.log", csdn_id);

	fp = fopen(file, "a+"); //���ӷ�ʽ�򿪣��������򴴽�
	if (NULL == fp) {
		#ifdef SPIDER_DEBUG
		fprintf(stderr, "fopen: %s\n", strerror(errno));
		#endif
		return;
	}
	
	setvbuf(fp, NULL, _IONBF, 0); //�رջ�����
	fseek(fp, 0, SEEK_END); //ָ���ƶ���ĩβ
	
	spider = spider_head->next;
	while (spider) {
		fprintf(fp, "%d:\n"
					"%-15s : %s\n"
					"%-15s : %s\n"
					"%-15s : %s\n"
					"%-15s : %d\n"
					"%-15s : %d\n"
					"%-15s : %s\n\n",
					spider->blog->b_seq_num,
					"title", spider->blog->b_title,
					"url", spider->blog->b_url,
					"date", spider->blog->b_date,
					"reads", spider->blog->b_reads,
					"comments", spider->blog->b_comments,
					"download", 
					(spider->blog->b_download == BLOG_DOWNLOAD) ? "Download" : "UnDownload");

		spider = spider->next;
	}
	
	fclose(fp);
}

/*********************************************************
�����͵Ļ�����Ϣ��ӡ����׼���
*********************************************************/
static void print_rank(blog_rank *rank)
{
	char file[BUFSIZE] = {0};
	FILE *fp;

	sprintf(file, "%s.log", csdn_id);

	fp = fopen(file, "w+");
	if (NULL == fp) {
		#ifdef SPIDER_DEBUG
		fprintf(stderr, "fopen: %s\n", strerror(errno));
		#endif
		return;
	}
	setvbuf(stdout, NULL, _IONBF, 0);
	
	fprintf(stdout, "CSDN ID : %s\n"
					"TITLE   : %s\n"
					"URL     : %s/%s\n"
					"%s\n"
					"%s\n"
					"%s\n"
					"%s\n"
					"%s\n"
					"%s\n"
					"%s\n",
					csdn_id,
					rank->b_title,
					CSDN_BLOG_URL,
					csdn_id,
					rank->b_page_view,
					rank->b_integral,
					rank->b_ranking,
					rank->b_original,
					rank->b_reship,
					rank->b_translation,
					rank->b_comments);

	fprintf(fp, "CSDN ID : %s\n"
				"TITLE   : %s\n"
				"URL     : %s/%s\n"
				"%s\n"
				"%s\n"
				"%s\n"
				"%s\n"
				"%s\n"
				"%s\n"
				"%s\n",
				csdn_id,
				rank->b_title,
				CSDN_BLOG_URL,
				csdn_id,
				rank->b_page_view,
				rank->b_integral,
				rank->b_ranking,
				rank->b_original,
				rank->b_reship,
				rank->b_translation,
				rank->b_comments);

	fclose(fp);
}

/*********************************************************
�ͷ���������Ŀռ�
*********************************************************/
static void free_spider(blog_spider * spider_head)
{
	blog_spider *pspider;
	blog_spider *tmp;

	pspider = spider_head;
	
	while (pspider) {
		if (pspider->blog->b_url) {
			free(pspider->blog->b_url);
		}
		if (pspider->blog->b_host) {
			free(pspider->blog->b_host);
		}
		if (pspider->blog->b_page_file) {
			free(pspider->blog->b_page_file);
		}
		if (pspider->blog->b_local_file) {
			free(pspider->blog->b_local_file);
		}
		if (pspider->blog->b_title) {
			free(pspider->blog->b_title);
		}
		if (pspider->blog->b_date) {
			free(pspider->blog->b_date);
		}
		
		free(pspider->blog);

		tmp = pspider->next; /*������һ���ڵ��ַ*/
		free(pspider);
		pspider = tmp;
	}
}

/*********************************************************
�ͷŲ��ͻ�����Ϣ�ṹ��ռ�
*********************************************************/
static void free_rank(blog_rank *rank)
{
	if (rank->b_title) {
		free(rank->b_title);
	}
	if (rank->b_page_view) {
		free(rank->b_page_view);
	}
	if (rank->b_integral) {
		free(rank->b_integral);
	}
	if (rank->b_ranking) {
		free(rank->b_ranking);
	}
	if (rank->b_original) {
		free(rank->b_original);
	}
	if (rank->b_reship) {
		free(rank->b_reship);
	}
	if (rank->b_translation) {
		free(rank->b_translation);
	}
	if (rank->b_comments) {
		free(rank->b_comments);
	}
	
	free(rank);
}

/*����������Ҫ���ز��͵���ҳ��Ȼ�������Ҫ����Ϣ��
   ���������ҵĲ�����ҳ��Ȼ����Կ���
   ���͵ı���
<div class="header">
<div id="blog_title">
<h1>
<a href="/xxg1413">Ѱ�����ҵĲ���</a></h1>
<h2>רעLinux������</h2>
<div class="clear">
</div>

������ҳ��

<!--��ʾ��ҳ-->
<div id="papelist" class="pagelist">
<span> 130������  ��13ҳ</span><strong>1</strong> 
<a href="/xxg1413/article/list/2">2</a>
<a href="/xxg1413/article/list/3">3</a> 
<a href="/xxg1413/article/list/4">4</a> 
<a href="/xxg1413/article/list/5">5</a>
<a href="/xxg1413/article/list/6">...</a> 
<a href="/xxg1413/article/list/2">��һҳ</a> 
<a href="/xxg1413/article/list/13">βҳ</a> 
</div>

����������������Ϣ
<div id="blog_medal">
</div>
<ul id="blog_rank">
<li>���ʣ�<span>4418��</span></li>
<li>���֣�<span>509��</span></li>
<li>������<span>��17783��</span></li>
</ul>
<ul id="blog_statistics">
<li>ԭ����<span>28ƪ</span></li>
<li>ת�أ�<span>101ƪ</span></li>
<li>���ģ�<span>0ƪ</span></li>
<li>���ۣ�<span>4��</span></li>
</ul>
</ul>

*/



/**********************************************************
��ȡ���͵Ļ�����Ϣ,�������¼���(�����ǰ���ҳ���˳��,
�������ո�˳��,ÿ�β��ұ�������ƫ��������ͷrewind(fp)):
�����ȡ�ܶ���Ϣ, ����ο�blog_spider��blog_rank�ṹ��
**********************************************************/
static int get_blog_info(blog_spider * spider_head, blog_rank * rank)
{
	FILE *fp;
	int count;
	char *posA, *posB, *posC, *posD, *posE;
	char tmpbuf[BUFSIZE]   = {0};
	char tmpbuf2[BUFSIZE]  = {0};
	char line[BUFSIZE]     = {0};
	char *rank_info_addr[7];
	
	fp = fopen(spider_head->blog->b_local_file, "r");
	if (NULL == fp) {
		fprintf(stderr, "fopen: %s\n", strerror(errno));
		return -1;
	}

	/*���Ҳ��͵ı���*/
	sprintf(tmpbuf, "<a href=\"/%s\">", csdn_id); //���Ҳ���ID������ǲ�����
	while (fgets(line, sizeof(line), fp)) {
		posA = strstr(line, tmpbuf);    //�ҵ�����ID��һ�γ��ֵ�λ��
		
		if (posA) {
			posA += strlen(tmpbuf);
			posB = strstr(posA, "</a>"); //
			*posB = 0;
			/*��������ͷ���ı���*/
			spider_head->blog->b_title = strdup(posA);
			rank->b_title = strdup(posA);
			
			#ifdef SPIDER_DEBUG
			printf("%s\n", spider_head->blog->b_title);
			#endif
			break;
		}
	}

	/*���Ҳ������µ���ҳ��*/
	while (fgets(line, sizeof(line), fp)) {
		posA = strstr(line, HTML_MULPAGE); //�ҵ�papelist
		
		if (posA) {
			fgets(line, sizeof(line), fp);
			posB = strrstr(line, BLOG_HREF); //���һ�γ���

			/* /gzshun/article/list/N, N������ҳ�� */
			memset(tmpbuf, 0, sizeof(tmpbuf));
			sprintf(tmpbuf, "/%s/%s/", csdn_id, BLOG_NEXT_LIST);
			posB += strlen(BLOG_HREF) + strlen(tmpbuf);
			posC = strchr(posB, '"');
			*posC = 0;
			rank->b_page_total = atoi(posB);//�ַ���ת��Ϊ����
			spider_head->blog->b_seq_num = rank->b_page_total;
			
			#ifdef SPIDER_DEBUG
			printf("b_page_total = %d\n", rank->b_page_total);
			#endif
			
			break;
		}
	}

	/*�ܹ���7����Ϣ: ���� ���� ���� ԭ�� ת�� ���� ����*/
	while (fgets(line, sizeof(line), fp)) {
		posA = strstr(line, BLOG_RANK);

		if (posA) {
			count = 0;
			while (fgets(line, sizeof(line), fp)) {
				posB = strstr(line, BLOG_LI);
				if (posB) {
					if (7 == count) {
						break;
					}
					posB += strlen(BLOG_LI);
					posC = strstr(posB, BLOG_SPAN_HEAD);
					posD = posC + strlen(BLOG_SPAN_HEAD);
					posE = strstr(posD, BLOG_SPAN_END);
					*posC = 0;
					*posE = 0;
					memset(tmpbuf, 0, sizeof(tmpbuf));
					memset(tmpbuf2, 0, sizeof(tmpbuf2));
					strcpy(tmpbuf, posB);
					strcpy(tmpbuf2, posD);
					strcat(tmpbuf, tmpbuf2);
					rank_info_addr[count++] = strdup(tmpbuf);
				}
			}
			
			rank->b_page_view     = rank_info_addr[0];
			rank->b_integral      = rank_info_addr[1];
			rank->b_ranking       = rank_info_addr[2];
			rank->b_original      = rank_info_addr[3];
			rank->b_reship        = rank_info_addr[4];
			rank->b_translation   = rank_info_addr[5];
			rank->b_comments      = rank_info_addr[6];
			
			break;
		}
	}

	fclose(fp);
	return 0;
}

/*****************************************************************
�������˵Ĳ�����ҳ, ��ȡ�������µ�URL, ��������Ϣ��ӵ�����������.
*****************************************************************/
/*
�ڲ�������<span class="link_title">
<a href="/xxg1413/article/details/7643695">
��href���������ǰ����������������һ����������
*/
static int analyse_index(blog_spider *spider_head)
{
	FILE *fp;
	int i;
	int ret;
	int len;
	int reads, comments;
	char *posA, *posB, *posC, *posD;
	char line[BUFSIZE*4]     = {0};
	char tmpbuf[BUFSIZE]     = {0};
	char tmpbuf2[BUFSIZE]    = {0};
	char page_file[BUFSIZE]  = {0};
	char url[BUFSIZE]        = {0};
	char title[BUFSIZE]      = {0};
	char date[BUFSIZE]       = {0};

	fp = fopen(spider_head->blog->b_local_file, "r");
	if (fp == NULL) {
		#ifdef SPIDER_DEBUG
		fprintf(stderr, "fopen: %s\n", strerror(errno));
		#endif
		return -1;
	}
	
	while (1) {
		if (feof(fp)) {
			break;
		}

		/*���Ҳ���*/
		while (fgets(line, sizeof(line), fp)) {
			posA = strstr(line, HTML_ARTICLE);

			if (posA) {
				/*���Ҳ�����ַ*/
				posA += strlen(HTML_ARTICLE) + strlen(BLOG_HREF);
				posB = strchr(posA, '"');
				*posB = 0;
				memset(page_file, 0, sizeof(page_file));
				memset(url, 0, sizeof(url));
				strcpy(page_file, posA);
				sprintf(url, "%s%s", CSDN_BLOG_URL, posA);

				/*���Ҳ��ͱ���*/
				#if 0
				posB += 1;
				posC = strstr(posB, BLOG_TITLE);
				/*�벩�͵�ַ����ͬһ��*/
				posC += strlen(BLOG_TITLE);
				posD = strstr(posC, "\">");
				*posD = 0;
				#else
				/*�ڲ��͵�ַ����һ��*/
				fgets(line, sizeof(line), fp);
				
				i = 0;
				while (1) {
					/*�ӵ�һ�����ǿո���ַ���ʼ��ȡ*/
					if (line[i] != ' ') {
						memset(title, 0, sizeof(title));
						line[strlen(line) - 1] = 0;
						strcpy(title, line + i);
						break;
					}
					i++;
				}
				#endif

				/*���Ҳ��ͷ�������*/
				while (fgets(line, sizeof(line), fp)) {
					posA = strstr(line, BLOG_DATE);
					
					if (posA) {
						posA += strlen(BLOG_DATE);
						posB = strstr(posA, BLOG_SPAN_END);
						*posB = 0;
						memset(date, 0, sizeof(date));
						strcpy(date, posA);
						
						break;
					}
				}

				/*���Ҳ����Ķ�����*/
				while (fgets(line, sizeof(line), fp)) {
					posA = strstr(line, BLOG_READ);

					if (posA) {
						posA += strlen(BLOG_READ);
						posB = strchr(posA, '(') + 1;
						posC = strchr(posB, ')');
						*posC = 0;
						reads = atoi(posB);
						break;
					}
				}

				/*���Ҳ������۴���*/
				while (fgets(line, sizeof(line), fp)) {
					posA = strstr(line, BLOG_COMMENT);

					if (posA) {
						posA += strlen(BLOG_COMMENT);
						posB = strchr(posA, '(') + 1;
						posC = strchr(posB, ')');
						*posC = 0;
						comments = atoi(posB);
						break;
					}
				}

				spider_head->blog->b_download = BLOG_DOWNLOAD; 

				blog_spider *spider;    //�½����
				ret = init_spider(&spider); //��ʼ�����
				if (ret < 0) {
					return -1;
				}
				
				//���������Ϣ
				spider->blog->b_page_file   = strdup(page_file);
				spider->blog->b_url         = strdup(url);
				spider->blog->b_date        = strdup(date);
				spider->blog->b_reads       = reads;
				spider->blog->b_comments    = comments;
				spider->blog->b_seq_num     = ++g_seq_num;

				memset(tmpbuf, 0, sizeof(tmpbuf));
				sprintf(tmpbuf, "%d.%s", spider->blog->b_seq_num, title);
				spider->blog->b_title = strdup(tmpbuf);

				memset(tmpbuf, 0, sizeof(tmpbuf));
				memset(tmpbuf2, 0, sizeof(tmpbuf2));
				strcpy(tmpbuf2, spider->blog->b_title);
				strfchr(tmpbuf2);
				sprintf(tmpbuf, "%s/%s.html", csdn_id, tmpbuf2);
				spider->blog->b_local_file  = strdup(tmpbuf);

				/*�����Ͳ��벩����������*/
				insert_spider(spider_head, spider);
				fputc('.', stdout);
			}
		}
	}

	fclose(fp);
	
	#ifdef SPIDER_DEBUG
	printf("\nspider size = %d\n", spider_size(spider_head));
	#endif
	
	return 0;
}

/*****************************************************************
���ظ��˵Ĳ�����ҳ
*****************************************************************/
/**������ҳ�����صĲ���Ϊ��
**�������� -> ������վ������ -> �������� -> ������Ӧ -> ���浽����
   connect_web -> send_request -> recv_response
*/
static int download_index(blog_spider * spider_head)
{
	int ret;
	
	ret = connect_web(spider_head); //��������
	if (ret < 0) {
		goto fail_download_index;
	}
	
	ret = send_request(spider_head); //��������
	if (ret < 0) {
		goto fail_download_index;
	}

	ret = recv_response(spider_head);  //������Ӧ
	if (ret < 0) {
		goto fail_download_index;
	}
	
	close(spider_head->blog->b_sockfd); 
	
	return 0;
	
fail_download_index:
	close(spider_head->blog->b_sockfd);
	return -1;
}

/**********************************************************
���ز�������
**********************************************************/
static int download_blog(blog_spider * spider)
{
	int ret;
	
	ret = connect_web(spider);
	if (ret < 0) {
		goto fail_download_blog;
	}
	
	ret = send_request(spider);
	if (ret < 0) {
		goto fail_download_blog;
	}

	ret = recv_response(spider);
	if (ret < 0) {
		goto fail_download_blog;
	}
	
	close(spider->blog->b_sockfd);
	
	return 0;
		
fail_download_blog:
	close(spider->blog->b_sockfd);
	return -1;
}

/**********************************************************
������������ȡ��������Ϣ,��Ҫ�ǻ�ȡ��IP��ַ.
**********************************************************/
static int get_web_host(const char * hostname)
{
	/*get host ip*/
	web_host = gethostbyname(hostname); //���������Ϣ
	if (NULL == web_host) {
		#ifdef SPIDER_DEBUG
		fprintf(stderr, "gethostbyname: %s\n", strerror(errno));
		#endif
		return -1;
	}
	
	#ifdef SPIDER_DEBUG
	printf("IP: %s\n", inet_ntoa(*((struct in_addr *)web_host->h_addr_list[0])));
	#endif

	return 0;
}

/**********************************************************
��ʼ��SOCKET,�����ӵ���վ������
**********************************************************/
static int connect_web(const blog_spider * spider)
{	
	int ret;
	struct sockaddr_in server_addr;

	/*init socket*/
	spider->blog->b_sockfd = socket(AF_INET, SOCK_STREAM, 0); //�����׽���
	if (spider->blog->b_sockfd < 0) {
		#ifdef SPIDER_DEBUG
		fprintf(stderr, "socket: %s\n", strerror(errno));
		#endif
		return -1;
	}

	memset(&server_addr, 0, sizeof(server_addr));
	
	server_addr.sin_family	= AF_INET;
	server_addr.sin_port	= htons(spider->blog->b_port);
	//�����õ�hostent�����һ���ֶ�
	server_addr.sin_addr	= *((struct in_addr *)web_host->h_addr_list[0]);
    
	//����
	ret = connect(spider->blog->b_sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr));
	if (ret < 0) {
		#ifdef SPIDER_DEBUG
		fprintf(stderr, "connect: %s\n", strerror(errno));
		#endif
		return -1;
	}
	
	return 0;
}

/**********************************************************
����վ��������������
**********************************************************/
static int send_request(const blog_spider * spider)
{
	int ret;
	char request[BUFSIZE];
	
	memset(request, 0, sizeof(request));
	sprintf(request, 
		"GET %s HTTP/1.1\r\n"
		"Accept: */*\r\n"
		"Accept-Language: zh-cn\r\n"
		"User-Agent: Mozilla/4.0 (compatible; MSIE 5.01; Windows NT 5.0)\r\n"
		"Host: %s:%d\r\n"
		"Connection: Close\r\n"
		"\r\n", spider->blog->b_page_file, spider->blog->b_host, spider->blog->b_port);

	ret = send(spider->blog->b_sockfd, request, sizeof(request), 0); //������Ϣ��������
	if (ret < 0) {
		#ifdef SPIDER_DEBUG
		fprintf(stderr, "send: %s\n", strerror(errno));
		#endif
		return -1;
	}
	
	#ifdef SPIDER_DEBUG
	printf("request:\n%s\n", request);
	#endif

	return 0;
}

/***************************************************************************************
������վ�������ķ�����Ϣ,�õ�������ļ�����
�����������������Ϣ���߷���������Ӧ��Ϣ,�Կ��н���,���Կ�����"\r\n\r\n"���жϽ�����־
select:
int select (int maxfdp1, fd_set *readset, fd_set *writeset, fd_set *exceptset, const struct timeval * timeout);
>0: ��ȷ
-1: ����
0 : ��ʱ
void FD_ZERO(fd_set *fdset); // clear all bits in fdset
void FD_SET(int fd, fd_set *fdset); // turn on the bit for fd in fdset
void FD_CLR(int fd, fd_set *fdset); // turn off the bit for fd in fdset
int  FD_ISSET(int fd, fd_set *fdset); // is the bit for fd on in fdset
***************************************************************************************/
/*�����õ���select�����ڴ˳����Ѿ�����Ҫ���ˣ�����select�и�ȱ�㣬�Ǿ��Ǳ���˳��ɨ��ȫ�����ļ���������
   ���Ҷ���������������ƣ�����epoll������Щȱ��
*/
static int recv_response(const blog_spider * spider)
{
	int ret, end, recvsize, count;
	char recvbuf[BUFSIZE];
	fd_set read_fds;
	struct timeval timeout;
	FILE *fp;

	/*����ʱ��Ҫ����, selectʧ�ܿ��ܵ�ԭ�����յ���վ����Ӧ��Ϣ��ʱ*/
	timeout.tv_sec  = 30;
	timeout.tv_usec = 0;

	FD_ZERO(&read_fds); //��ʼ��
	FD_SET(spider->blog->b_sockfd, &read_fds); //���һ���ļ�������

	while (1) {
		ret = select(spider->blog->b_sockfd+1, &read_fds, NULL, NULL, &timeout);
		if (-1 == ret) {
			/*����,ֱ�ӷ��ش���*/
			#ifdef SPIDER_DEBUG
			fprintf(stderr, "select: %s\n", strerror(errno));
			#endif
			return -1;
		}
		else if (0 == ret) {
			/*��ʱ, ������ѯ*/
			#ifdef SPIDER_DEBUG
			fprintf(stderr, "select timeout: %s\n", spider->blog->b_title);
			#endif
			goto fail_recv_response;
		}
		
		/*���ܵ�����*/
		if (FD_ISSET(spider->blog->b_sockfd, &read_fds)) {  
			end = 0;
			count = 0;

			/*�������������ļ���������,����"3/5",'/'��Linux�Ǵ���Ŀ¼*/
			fp = fopen(spider->blog->b_local_file, "w+");
			if (NULL == fp) {
				goto fail_recv_response;
			}

			spider->blog->b_download = BLOG_DOWNLOAD;
			
			while (read(spider->blog->b_sockfd, recvbuf, 1) == 1) {
				if(end< 4) {
					if(recvbuf[0] == '\r' || recvbuf[0] == '\n')  {
						end++;
					}
					else {
						end = 0;
					}
					/*������http��������������Ϣͷ,����Ҫ,����Ա�������*/
				}
				else {
					fputc(recvbuf[0], fp);
					count++;
					if (1024 == count) {
						fflush(fp);
					}
				}
			}
			
			fclose(fp);			
			break;
		}
	}

	FD_CLR(spider->blog->b_sockfd, &read_fds); //��set���Ƴ��ļ���������һ�㲻��
	
	return 0;
	
fail_recv_response:
	spider->blog->b_download = BLOG_UNDOWNLOAD; //�������Ϊδ����
	FD_CLR(spider->blog->b_sockfd, &read_fds);
	return -1;
}

int main(int argc, char **argv)
{
	int i;
	int retval;
	int count;
	char tmpbuf[BUFSIZE];
	char url[BUFSIZE];
	char next_page_url[BUFSIZE];
	blog_spider *spider_head;
	blog_spider *spider;
	blog_rank *b_rank;
	
	if (argc != 2) {
		fprintf(stderr, "Usage  : %s CSDN_ID\n"
						"Example: %s gzshun\n", 
						argv[0], argv[0]);
		exit(1);
	}

	/*��ʼ�����������ͷ���*/
	retval = init_spider(&spider_head);
	if (retval < 0) {
		goto fail_main;
	}
	
	retval = init_rank(&b_rank);
	if (retval < 0) {
		goto fail_main;
	}

	memset(csdn_id, 0, sizeof(csdn_id));
	strcpy(csdn_id, argv[1]);
	
	memset(url, 0, sizeof(url));
	sprintf(url, "%s/%s", CSDN_BLOG_URL, csdn_id);
	spider_head->blog->b_url = strdup(url);
	spider_head->blog->b_local_file = strdup(BLOG_INDEX);

	memset(tmpbuf, 0, sizeof(tmpbuf));
	sprintf(tmpbuf, "/%s", csdn_id);
	spider_head->blog->b_page_file = strdup(tmpbuf);

	/*******************************************************
	�Ȼ�ȡIP��ַ.
	*******************************************************/
	retval = get_web_host(CSDN_BLOG_HOST);
	if (retval < 0) {
		goto fail_main;
	}

	/*******************************************************
	���ظ��˵Ĳ�����ҳ,��:http://blog.csdn.net/gzshun
	���˵Ĳ�����ҳ�����кܶ�����,��ô�ͻ��кü�ҳ
	*******************************************************/
	retval = download_index(spider_head);
	if (retval < 0) {
		goto fail_main;
	}

	retval = get_blog_info(spider_head, b_rank);
	if (retval < 0) {
		goto fail_main;
	}
	
	setvbuf(stdout, NULL, _IONBF, 0);

	/*******************************************************
	ѭ������ÿһҳ,�����������͵�URL.
	*******************************************************/
	for (i = 1; i <= b_rank->b_page_total; i++) {
		memset(tmpbuf, 0, sizeof(tmpbuf));
		sprintf(tmpbuf, "/%s/%s/%d", csdn_id, BLOG_NEXT_LIST, i);
		
		if (spider_head->blog->b_page_file) {
			free(spider_head->blog->b_page_file);
		}
		spider_head->blog->b_page_file = strdup(tmpbuf);
		
		retval = download_index(spider_head);
		if (retval < 0) {
			goto fail_main;
		}
		
		retval = analyse_index(spider_head);
		if (retval < 0) {
			goto fail_main;
		}
	}
	printf("\n");
	
	print_rank(b_rank);
	sleep(2);

	/*��ʼ�������в���*/
	mkdir(csdn_id, 0755);
	spider = spider_head->next;
	while (spider) {
		if (access(spider->blog->b_local_file, F_OK) == 0) {
			spider->blog->b_download = BLOG_DOWNLOAD;
			spider = spider->next;
			continue;
		}
		retval = download_blog(spider);
		if (retval < 0) {
			goto fail_main;
		}

		#if 1
		printf("%-10s  ==>  %s  %s\n", 
			   (spider->blog->b_download == BLOG_DOWNLOAD) ? "Download" : "UnDownload", 
			   spider->blog->b_date,
			   spider->blog->b_title);
		#else
		fprintf(stdout, "%d:\n"
								"%-15s : %s\n"
								"%-15s : %s\n"
								"%-15s : %s\n"
								"%-15s : %d\n"
								"%-15s : %d\n"
								"%-15s : %s\n\n",
								spider->blog->b_seq_num,
								"title", spider->blog->b_title,
								"url", spider->blog->b_url,
								"date", spider->blog->b_date,
								"reads", spider->blog->b_reads,
								"comments", spider->blog->b_comments,
								"download", 
								(spider->blog->b_download == BLOG_DOWNLOAD) ? "Download" : "UnDownload");
		#endif

		spider = spider->next;
	}
	
	print_spider(spider_head);

	free_spider(spider_head);
	free_rank(b_rank);

	return 0;

fail_main:
	free_spider(spider_head);
	free_rank(b_rank);
	exit(1);
}
