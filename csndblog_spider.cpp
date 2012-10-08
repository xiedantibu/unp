/***************************************************************************
** Name         : blogspider.c
** Description  : Download CSDN Blog. 
**                This program can backup everybody's csdn blog.
****************************************************************************/
//调试宏
#if 0
#define SPIDER_DEBUG
#endif

//linux下头文件包含
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

//html文件的语法
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

//多线程使用，预留
#define BLOG_LOCK        (10)
#define BLOG_UNLOCK      (11)

#define BLOG_DOWNLOAD    (20)
#define BLOG_UNDOWNLOAD  (21)


//需要加入链表结点的博客信息
typedef struct tag_blog_info {
	char *b_url;           /*网址*/
	char *b_host;          /*网站服务器主机名*/
	char *b_page_file;     /*页面文件名称*/
	char *b_local_file;    /*本地保存的文件名称*/
	char *b_title;         /*博客主题*/
	char *b_date;          /*博客发表日期*/
	int   b_port;          /*网址端口号*/
	int   b_sockfd;        /*网络套接字*/
	int   b_reads;         /*阅读次数*/
	int   b_comments;      /*评论次数*/
	int   b_download;      /*下载状态*/
	int   b_lock;          /*处理锁*/
	int   b_seq_num;       /*序号*/
}blog_info;

//爬行链表结点
typedef struct tag_blog_spider {
	blog_info *blog;
	struct tag_blog_spider *next;
}blog_spider;

//博客基本信息
typedef struct tag_blog_rank {
	int   b_page_total;    /*博客总页数*/
	char *b_title;         /*博客标题*/
	char *b_page_view;     /*博客访问量*/
	char *b_integral;      /*博客积分*/
	char *b_ranking;       /*博客排名*/
	char *b_original;      /*博客原创文章数量*/
	char *b_reship;        /*博客转载文章数量*/
	char *b_translation;   /*博客译文文章数量*/
	char *b_comments;      /*博客评论数量*/
}blog_rank;

//全局变量
static int g_seq_num = 0;
static char csdn_id[255];
static struct hostent *web_host;
/* hostent 主机信息的结构体 
gethostbyname()返回一个hostent指针
struct hostent  
{
　　char  *h_name; //地址的正式名称
  　char  **h_aliases;
	int   h_addrtype;//地址类型(AF_INET)
	int   h_length;   //地址的比特长度
	char  **h_addr_list;//主机网络地址指针(网络字节序)
　　 #define h_addr h_addr_list[0] //h_addr_list中的第一个地址
 };
 */

static char *strrstr(const char *s1, const char *s2);//从s1字符串中查找s2字符串,返回最后一次出现的地址 
static char *strfchr(char *s);//过滤掉s字符串中不规则的字符
static int  init_spider(blog_spider **spider);//初始化博客爬虫节点,必须使用指针的指针,否则达不到预期效果
static int  init_rank(blog_rank **rank);//初始化博客存放基本信息的结构体
static void insert_spider(blog_spider *spider_head, blog_spider *spider);//将博客插入爬行链表  
static int  spider_size(blog_spider *spider_head);//计算爬行链表的长度  
static void print_spider(blog_spider *spider_head);//打印爬行链表,保存到当前目录的*.log文件
static void print_rank(blog_rank *rank);//打印博客基本信息  
static void free_spider(blog_spider *spider_head);//释放爬行链表的空间 
static void free_rank(blog_rank *rank);//释放博客基本信息的空间  
static int  get_blog_info(blog_spider *spider_head, blog_rank *rank);//从博客主页获取博客标题,博客文章的总页数,积分,排名等信息
static int  analyse_index(blog_spider *spider_head);//分析每一页博客的信息,并添加进爬行链表 
static int  download_index(blog_spider *spider_head);//下载博客主页  
static int  download_blog(blog_spider *spider);//下载每一篇博客    
static int  get_web_host(const char *hostname);//得到"blog.csdn.net"网站的主机信息
static int  connect_web(const blog_spider *spider);//初始化socket,并连接网站服务器  
static int  send_request(const blog_spider * spider);//给网站服务器发送请求
static int  recv_response(const blog_spider * spider);//接受网站服务器的响应消息  


/**************************************************************
strrstr  : 查找指定字符串, 返回最后一次出现的地址, 自己实现
***************************************************************/
static char *strrstr(const char *s1, const char *s2)
{
	//代码中指针传递都应该检查是否为NULL，可使用assert
	int len2;
	char *ps1;

	if (!(len2 = strlen(s2))) {       //s2长度为0时
		return (char *)s1;
	}
	
	ps1 = (char *)s1 + strlen(s1) - 1; //s1字符串最后字符地址
	ps1 = ps1 - len2 + 1;              

	while (ps1 >= s1) {             //逆序比较，得到最后一个匹配
		if ((*ps1 == *s2) && (strncmp(ps1, s2, len2) == 0)) {
			return (char *)ps1;
		}
		ps1--;
	}

	return NULL;
}

/*********************************************************
strfchr  : 查找指定字符串中不规则的字符, 并赋空
若没有删除这些不规则的字符,则创建文件的时候将会出错
*********************************************************/
static char *strfchr(char *s)
{
	char *p = s;
	
	while (*p) {
		if (('/' == *p) || ('?' == *p)) {
			*p = 0;
			strcat(s, "xxx"); //将xxx添加到s中，并\0
			
			return p;
		}
		p++;
	}
	
	return NULL;
}

/*********************************************************
初始化博客爬虫的链表节点, 申请空间并赋空值
*********************************************************/
static int init_spider(blog_spider * * spider)
{
	 //链表结点
	*spider = (blog_spider *)malloc(sizeof(blog_spider));
	if (NULL == *spider) {   //申请空间未成功
		#ifdef SPIDER_DEBUG
		fprintf(stderr, "malloc: %s\n", strerror(errno));
		#endif
		return -1;
	}
	
    //博客信息
	(*spider)->blog = (blog_info *)malloc(sizeof(blog_info));
	if (NULL == (*spider)->blog) {
		#ifdef SPIDER_DEBUG
		fprintf(stderr, "malloc: %s\n", strerror(errno));
		#endif
		free(*spider);
		return -1;
	}

    //初始化结点
	//strdup 调用malloc分配内存 结束时必须free
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
初始化博客基本信息结构体,包含以下几个变量:
1.博客页面总页数
2.博客标题
3.博客访问量
4.博客积分
5.博客排名
6.博客原创文章数量
7.博客转载文章数量
8.博客译文文章数量
9.博客评论数量
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
将博客爬虫节点插入爬虫链表
*********************************************************/
static void insert_spider(blog_spider * spider_head, blog_spider * spider)
{
	blog_spider *pspider;

	pspider = spider_head;

	while (pspider->next) {   //找到最后结点
		pspider = pspider->next;
	}
	
	pspider->next = spider;  //插入结点
}

/*********************************************************
返回爬虫链表长度
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
将爬虫链表的内容打印到log文件,格式为"csdn_id.log",比如
我的博客的地址为: "gzshun.log"
*********************************************************/
static void print_spider(blog_spider *spider_head)
{
	char file[BUFSIZE] = {0};
	char tmpbuf[BUFSIZE] = {0};
	blog_spider *spider;
	FILE *fp;

	sprintf(file, "%s.log", csdn_id);

	fp = fopen(file, "a+"); //附加方式打开，不存在则创建
	if (NULL == fp) {
		#ifdef SPIDER_DEBUG
		fprintf(stderr, "fopen: %s\n", strerror(errno));
		#endif
		return;
	}
	
	setvbuf(fp, NULL, _IONBF, 0); //关闭缓冲区
	fseek(fp, 0, SEEK_END); //指针移动到末尾
	
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
将博客的基本信息打印到标准输出
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
释放爬虫链表的空间
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

		tmp = pspider->next; /*保存下一个节点地址*/
		free(pspider);
		pspider = tmp;
	}
}

/*********************************************************
释放博客基本信息结构体空间
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

/*现在我们需要下载博客的主页，然后分析必要的信息。
   我下载了我的博客首页，然后可以看到
   博客的标题
<div class="header">
<div id="blog_title">
<h1>
<a href="/xxg1413">寻找自我的博客</a></h1>
<h2>专注Linux网络编程</h2>
<div class="clear">
</div>

博客总页数

<!--显示分页-->
<div id="papelist" class="pagelist">
<span> 130条数据  共13页</span><strong>1</strong> 
<a href="/xxg1413/article/list/2">2</a>
<a href="/xxg1413/article/list/3">3</a> 
<a href="/xxg1413/article/list/4">4</a> 
<a href="/xxg1413/article/list/5">5</a>
<a href="/xxg1413/article/list/6">...</a> 
<a href="/xxg1413/article/list/2">下一页</a> 
<a href="/xxg1413/article/list/13">尾页</a> 
</div>

博客排名，积分信息
<div id="blog_medal">
</div>
<ul id="blog_rank">
<li>访问：<span>4418次</span></li>
<li>积分：<span>509分</span></li>
<li>排名：<span>第17783名</span></li>
</ul>
<ul id="blog_statistics">
<li>原创：<span>28篇</span></li>
<li>转载：<span>101篇</span></li>
<li>译文：<span>0篇</span></li>
<li>评论：<span>4条</span></li>
</ul>
</ul>

*/



/**********************************************************
获取博客的基本信息,包括以下几点(以下是按照页面的顺序,
若不按照该顺序,每次查找必须重设偏移量到开头rewind(fp)):
这里获取很多信息, 具体参考blog_spider与blog_rank结构体
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

	/*查找博客的标题*/
	sprintf(tmpbuf, "<a href=\"/%s\">", csdn_id); //查找博客ID，后便是博客名
	while (fgets(line, sizeof(line), fp)) {
		posA = strstr(line, tmpbuf);    //找到博客ID第一次出现的位置
		
		if (posA) {
			posA += strlen(tmpbuf);
			posB = strstr(posA, "</a>"); //
			*posB = 0;
			/*设置爬虫头结点的标题*/
			spider_head->blog->b_title = strdup(posA);
			rank->b_title = strdup(posA);
			
			#ifdef SPIDER_DEBUG
			printf("%s\n", spider_head->blog->b_title);
			#endif
			break;
		}
	}

	/*查找博客文章的总页数*/
	while (fgets(line, sizeof(line), fp)) {
		posA = strstr(line, HTML_MULPAGE); //找到papelist
		
		if (posA) {
			fgets(line, sizeof(line), fp);
			posB = strrstr(line, BLOG_HREF); //最后一次出现

			/* /gzshun/article/list/N, N就是总页数 */
			memset(tmpbuf, 0, sizeof(tmpbuf));
			sprintf(tmpbuf, "/%s/%s/", csdn_id, BLOG_NEXT_LIST);
			posB += strlen(BLOG_HREF) + strlen(tmpbuf);
			posC = strchr(posB, '"');
			*posC = 0;
			rank->b_page_total = atoi(posB);//字符串转换为整型
			spider_head->blog->b_seq_num = rank->b_page_total;
			
			#ifdef SPIDER_DEBUG
			printf("b_page_total = %d\n", rank->b_page_total);
			#endif
			
			break;
		}
	}

	/*总共有7条信息: 访问 积分 排名 原创 转载 译文 评论*/
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
分析个人的博客主页, 获取所有文章的URL, 将博客信息添加到爬虫链表中.
*****************************************************************/
/*
在博客中有<span class="link_title">
<a href="/xxg1413/article/details/7643695">
在href后面的链接前加上域名都生成了一个博文链接
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

		/*查找博客*/
		while (fgets(line, sizeof(line), fp)) {
			posA = strstr(line, HTML_ARTICLE);

			if (posA) {
				/*查找博客网址*/
				posA += strlen(HTML_ARTICLE) + strlen(BLOG_HREF);
				posB = strchr(posA, '"');
				*posB = 0;
				memset(page_file, 0, sizeof(page_file));
				memset(url, 0, sizeof(url));
				strcpy(page_file, posA);
				sprintf(url, "%s%s", CSDN_BLOG_URL, posA);

				/*查找博客标题*/
				#if 0
				posB += 1;
				posC = strstr(posB, BLOG_TITLE);
				/*与博客地址处在同一行*/
				posC += strlen(BLOG_TITLE);
				posD = strstr(posC, "\">");
				*posD = 0;
				#else
				/*在博客地址的下一行*/
				fgets(line, sizeof(line), fp);
				
				i = 0;
				while (1) {
					/*从第一个不是空格的字符开始读取*/
					if (line[i] != ' ') {
						memset(title, 0, sizeof(title));
						line[strlen(line) - 1] = 0;
						strcpy(title, line + i);
						break;
					}
					i++;
				}
				#endif

				/*查找博客发表日期*/
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

				/*查找博客阅读次数*/
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

				/*查找博客评论次数*/
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

				blog_spider *spider;    //新建结点
				ret = init_spider(&spider); //初始化结点
				if (ret < 0) {
					return -1;
				}
				
				//加入基本信息
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

				/*将博客插入博客爬虫链表*/
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
下载个人的博客主页
*****************************************************************/
/**下载网页到本地的步骤为：
**建立连接 -> 连接网站服务器 -> 发送请求 -> 接收响应 -> 保存到本地
   connect_web -> send_request -> recv_response
*/
static int download_index(blog_spider * spider_head)
{
	int ret;
	
	ret = connect_web(spider_head); //建立连接
	if (ret < 0) {
		goto fail_download_index;
	}
	
	ret = send_request(spider_head); //发送请求
	if (ret < 0) {
		goto fail_download_index;
	}

	ret = recv_response(spider_head);  //接受响应
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
下载博客文章
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
根据主机名获取到主机信息,主要是获取到IP地址.
**********************************************************/
static int get_web_host(const char * hostname)
{
	/*get host ip*/
	web_host = gethostbyname(hostname); //获得主机信息
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
初始化SOCKET,并连接到网站服务器
**********************************************************/
static int connect_web(const blog_spider * spider)
{	
	int ret;
	struct sockaddr_in server_addr;

	/*init socket*/
	spider->blog->b_sockfd = socket(AF_INET, SOCK_STREAM, 0); //创建套接字
	if (spider->blog->b_sockfd < 0) {
		#ifdef SPIDER_DEBUG
		fprintf(stderr, "socket: %s\n", strerror(errno));
		#endif
		return -1;
	}

	memset(&server_addr, 0, sizeof(server_addr));
	
	server_addr.sin_family	= AF_INET;
	server_addr.sin_port	= htons(spider->blog->b_port);
	//这里用到hostent的最后一个字段
	server_addr.sin_addr	= *((struct in_addr *)web_host->h_addr_list[0]);
    
	//连接
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
向网站服务器发送请求
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

	ret = send(spider->blog->b_sockfd, request, sizeof(request), 0); //发送信息给服务器
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
接受网站服务器的反馈信息,得到请求的文件内容
向服务器发送请求信息或者服务器的响应消息,以空行结束,所以可以用"\r\n\r\n"来判断结束标志
select:
int select (int maxfdp1, fd_set *readset, fd_set *writeset, fd_set *exceptset, const struct timeval * timeout);
>0: 正确
-1: 出错
0 : 超时
void FD_ZERO(fd_set *fdset); // clear all bits in fdset
void FD_SET(int fd, fd_set *fdset); // turn on the bit for fd in fdset
void FD_CLR(int fd, fd_set *fdset); // turn off the bit for fd in fdset
int  FD_ISSET(int fd, fd_set *fdset); // is the bit for fd on in fdset
***************************************************************************************/
/*这里用到了select，对于此程序已经满足要求了，但是select有个缺点，那就是必须顺序扫描全部的文件描述符，
   而且对最多描述符有限制，可用epoll避免这些缺点
*/
static int recv_response(const blog_spider * spider)
{
	int ret, end, recvsize, count;
	char recvbuf[BUFSIZE];
	fd_set read_fds;
	struct timeval timeout;
	FILE *fp;

	/*建议时间要长点, select失败可能的原因是收到网站的响应消息超时*/
	timeout.tv_sec  = 30;
	timeout.tv_usec = 0;

	FD_ZERO(&read_fds); //初始化
	FD_SET(spider->blog->b_sockfd, &read_fds); //添加一个文件描述符

	while (1) {
		ret = select(spider->blog->b_sockfd+1, &read_fds, NULL, NULL, &timeout);
		if (-1 == ret) {
			/*出错,直接返回错误*/
			#ifdef SPIDER_DEBUG
			fprintf(stderr, "select: %s\n", strerror(errno));
			#endif
			return -1;
		}
		else if (0 == ret) {
			/*超时, 继续轮询*/
			#ifdef SPIDER_DEBUG
			fprintf(stderr, "select timeout: %s\n", spider->blog->b_title);
			#endif
			goto fail_recv_response;
		}
		
		/*接受到数据*/
		if (FD_ISSET(spider->blog->b_sockfd, &read_fds)) {  
			end = 0;
			count = 0;

			/*这里出错可能是文件名不规则,比如"3/5",'/'在Linux是代表目录*/
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
					/*这里是http服务器反馈的消息头,若需要,则可以保存下来*/
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

	FD_CLR(spider->blog->b_sockfd, &read_fds); //从set中移除文件描述符，一般不用
	
	return 0;
	
fail_recv_response:
	spider->blog->b_download = BLOG_UNDOWNLOAD; //出错，标记为未下载
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

	/*初始化爬虫链表的头结点*/
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
	先获取IP地址.
	*******************************************************/
	retval = get_web_host(CSDN_BLOG_HOST);
	if (retval < 0) {
		goto fail_main;
	}

	/*******************************************************
	下载个人的博客主页,如:http://blog.csdn.net/gzshun
	个人的博客主页可能有很多文章,那么就会有好几页
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
	循环下载每一页,并分析出博客的URL.
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

	/*开始下载所有博客*/
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
