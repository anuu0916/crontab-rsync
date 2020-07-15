all : ssu_crontab ssu_crond ssu_rsync

ssu_crontab : ssu_crontab.c
	gcc -o ssu_crontab ssu_crontab.c

ssu_crond : ssu_crond.c
	gcc -o ssu_crond ssu_crond.c

ssu_rsync : ssu_rsync.c
	gcc -o ssu_rsync ssu_rsync.c -lpthread
