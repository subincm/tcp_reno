CC = gcc

LIBS = -lresolv  -lnsl -lpthread -lm\
	/home/subin/Desktop/np/stevens/unpv13e/libunp.a\
	
FLAGS = -g -O2

CFLAGS = ${FLAGS} -I/home/subin/Desktop/np/stevens/unpv13e/lib
#CFLAGS = ${FLAGS} -I/home/raghu/np/np1/unpv13e/lib -I/home/raghu/np/np2/Asgn2_code

all: client server 

# server uses the thread-safe version of readline.c

server: dgserver.o readline.o config.o ifi.o get_ifi_info_plus.o packet.o buffer.o client_list.o rtt_mod.o persist_timer.o congestion_ctrl.o
	${CC} ${FLAGS} get_ifi_info_plus.o dgserver.o readline.o config.o ifi.o packet.o buffer.o client_list.o rtt_mod.o persist_timer.o congestion_ctrl.o -o server ${LIBS}

dgserver.o: dgserver.c
	${CC} ${CFLAGS} -c dgserver.c

client: dgclient.o config.o ifi.o get_ifi_info_plus.o packet.o buffer.o rtt_mod.o
	${CC} ${FLAGS} dgclient.o config.o ifi.o get_ifi_info_plus.o packet.o buffer.o rtt_mod.o -o client ${LIBS}
#${CC} ${FLAGS} -o dgclient.o config.o ifi.o get_ifi_info_plus.o ${LIBS}

dgclient.o: dgclient.c
	${CC} ${CFLAGS} -c dgclient.c

config.o: config.c
	${CC} ${CFLAGS} -c config.c

ifi.o: ifi.c
	${CC} ${CFLAGS} -c ifi.c

get_ifi_info_plus.o: unpifiplus.h get_ifi_info_plus.c
	${CC} ${CFLAGS} -c get_ifi_info_plus.c

packet.o: packet.c
	${CC} ${CFLAGS} -c packet.c

buffer.o: buffer.c
	${CC} ${CFLAGS} -c buffer.c

rtt_mod.o: rtt_mod.c
	${CC} ${CFLAGS} -c rtt_mod.c

persist_timer.o: persist_timer.c
	${CC} ${CFLAGS} -c persist_timer.c

congestion_ctrl.o: congestion_ctrl.c
	${CC} ${CFLAGS} -c congestion_ctrl.c

client_list.o: client_list.c
	${CC} ${CFLAGS} -c client_list.c
# pick up the thread-safe version of readline.c from directory "threads"

readline.o: readline.o
	${CC} ${CFLAGS} -c /home/subin/Desktop/np/stevens/unpv13e/threads/readline.c

clean:
	rm dgserver.o dgclient.o readline.o get_ifi_info_plus.o ifi.o packet.o config.o buffer.o rtt_mod.o client_list.o congestion_ctrl.o persist_timer.o client server

