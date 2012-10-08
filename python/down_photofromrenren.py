#!/usr/bin/env python
#encoding=utf-8


import sys, urllib2, cookielib,urllib,re
from HTMLParser import HTMLParser

reload(sys)
sys.setdefaultencoding('utf-8')
class ShareParser(HTMLParser):
	def __init__(self, has_next):
		self.has_next = has_next
		self.link = []
		self.qualified = 0
		HTMLParser.__init__(self) 

	def handle_starttag(self, tag, attr):
		if tag == 'div':
			for k,v in attr:
				if k == 'class' and v == 'photo-list clearfix':
					self.qualified = 1
					break
		if tag == 'img' and self.qualified:
			for k,v in attr:
				if k == 'src':
					self.link.append(v)
		is_next = 0
		if tag == 'a' and self.has_next:
			for k,v in attr:
				if k == 'title' and v == '下一页':
					is_next = 1
				if is_next and k == 'href':
					link = get_next_link(v)
					self.link.extend(link)
					self.has_next = 0
					break
	
	def handle_endtag(self, tag):
		if tag == 'div' and self.qualified:
			self.qualified = 0


class PhotoParser(HTMLParser):
	def __init__(self):
		self.link = []
		self.qualified = 0
		self.num_qualified = 0
		HTMLParser.__init__(self)

	def handle_starttag(self, tag, attr):
		if tag == 'a':
			for k,v in attr:
				if k == 'class' and v == 'pic':
					self.qualified = 1
					break
		if tag == 'img' and self.qualified:
			for k,v in attr:
				if k == 'data-src':
					self.link.append(v)
					break
	
	def handle_endtag(self, tag):
		if tag == 'a' and self.qualified:
			self.qualified = 0
	
			


def get_next_link(src):
	global opener
	req = urllib2.Request(src)
	fd = opener.open(req)
	data = fd.read()
	sp = ShareParser(data.find('下一页')!=-1)
	sp.feed(data)

	return sp.link
	
def change(src):
	dest = []
	for str in src:
		if str.find('head') == -1:
			print 'This image link cannot be changed -- %s' % str
		else:
			new = str.replace('head','original')
			dest.append(new)
	return dest


def get_link_from_ajax_return(text):
	string = text 
	link_str = string.split('"largeUrl":"')[1:]
	link = []
	for s in link_str:
		link.append(s.split('"')[0].replace('\\',''))
	return link

# the header of the connection
header = {'Host':'www.renren.com','User-Agent':"Mozilla/5.0 (X11;Ubuntu;Linux i686,rv:10.02)\
		Gecko/20100101 Firefox/10.0.2",'Accept':"text/html,application/xhtml+xml,\
		application/xml;q=0.9,*/*;q=0.8",'Accept-Language':"en-us,en;q=0.5",\
		'Connection':'keep-alive'}

cookiesjar = cookielib.CookieJar()
cookiehandler = urllib2.HTTPCookieProcessor(cookiesjar)
opener = urllib2.build_opener(cookiehandler)

opener.open('http://www.renren.com')

post_data = {'email':#你的帐号,\ #我的帐号是'bobobogogogo@126.com'
		'password':#你的密码,\
		'icode':'',\
		'origURL':'http://www.renren.com/indexcon',\
		'domain':'renren.com',\
		'key_id':'1',\
		'captcha_tpye':'web_login',\
		'_rtk':'91cdffe3'}

req = urllib2.Request('http://www.renren.com/PLogin.do', headers = header, data = urllib.urlencode(post_data))
try:
	fd = opener.open(req)
except urllib2.HTTPError, e:
	print e
	print 'Login Failed, I am sorry you can not use this script right now, maybe sometime latter\
	or please contact the author of this script linjianfengqrh@gmail.com'
	sys.exit(0)

print 'Login in successfully'
print 'please input the album you want to download:'
album_link = raw_input()
if album_link.find('curPage') != -1:
	album_link = album_link.split('?')[0]
print 'How many photoes in this album:'
try:
	photo_count = int(raw_input())
except ValueError,e:
	print e
	sys.exit(0)


req = urllib2.Request(album_link)
try:
	fd =opener.open(req)
except urllib2.HTTPError, e:
	print 'URL open error'
	print e
	sys.exit(0)

data = fd.read()
original_img_src = []
if album_link.find('album') == -1:
	sp = ShareParser(data.find('下一页') != -1)

	sp.feed(data)

	if not sp.link:
		print 'there is no pictures'
		sys.exit(0)
	head_img_src = sp.link
	original_img_src = change(head_img_src)

else:
	pp = PhotoParser()
	pp.feed(data)
	
	if not pp.link:
		print 'There is no pictures Maybe you dont have the auth'
		sys.exit(0)
	
	photo_num = photo_count 
	link = pp.link
	ajax_call = 1
	cur_num = len(link)
	if photo_num > 60:
		while(cur_num < photo_num):
			ajax_link = album_link + '/bypage/ajax?curPage=' + str(ajax_call*3) +'&pagenum=3'
			fd = opener.open(ajax_link)
			data = fd.read()
			link.extend(get_link_from_ajax_return(data))
			cur_num = len(link)
			ajax_call += 1
	original_img_src = link

	
count = 1 
print 'There are %d images -->' %len(original_img_src)
for src in original_img_src:
	req = urllib2.Request(src)
	inet = opener.open(req);
	
	filename = str(count)+'.jpg'
	
	file = open(filename, 'wb')
	file.write(inet.read())
	file.close()
	print 'picture %d has done' % count
	count += 1
print 'done!!'