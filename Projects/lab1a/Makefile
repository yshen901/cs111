#Name : YUCI SHEN
#EMAIL: SHEN.YUCI11@GMAIL.COM
#UID  : 604836772

CC = gcc
CFLAGS = -g -Wall -Wextra
ID = 604836772
ARCHIVE = lab1a-$(ID).tar.gz
sources = lab1a.c Makefile README

build:
	@$(CC) $(CFLAGS) -o lab1a lab1a.c

dist:
	@tar -czf $(ARCHIVE) $(sources)

clean:
	@rm -f lab1a $(ARCHIVE)
