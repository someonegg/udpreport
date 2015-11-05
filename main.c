
/*
 *  Copyright (C) someonegg .
 */

#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <ev.h>
#include "common_mp.h"
#include "logger.h"
#include "mempl_mt.h"


#define MSG_MAXSIZE          1024
#define MEMPL_STAGE_COUNT    1
#define MEMPL_STAGE_WRITE    0

// todo
#define IS_DAEMON      0
#define DATA_BUFSIZE   (4 * 1024 * 1024)

#define SVR_IP        "172.19.25.88"
#define SVR_PORT       10000
#define SVR_DATA      "/dev/null"

#define ERR_LOG       "/tmp/report_server.err"
#define PID_FILE      "/tmp/report_server.pid"


int daemon_init();
int create_pid_file(const char* pid_file);
int curtime_str(char* buf, int buf_len);
int sock_ntop(struct sockaddr_in* sin, char* text, size_t len);
int bind_socket(const char* ip, unsigned short port);


typedef struct server_stat_s {
    long long msgc_total;
    long long recv_total;
    long long writ_total;
    long long last_time;
    long long msgc_last;
    long long recv_last;
    long long writ_last;
} server_stat_t;

typedef struct work_thread_s {
    struct ev_loop* loop;
    ev_async wake_watcher;
    struct server_s* server;
    pthread_t thread;
    void* thread_data;
    void (*wake_cb)(struct work_thread_s* me);
} work_thread_t;

typedef struct server_s {
    int sock_fd;
    int data_fd;
    mempl_mt_t* mempl;
    volatile int need_exit;
    server_stat_t stat;
    work_thread_t recv_thread;
    work_thread_t writ_thread;
    work_thread_t stat_thread;
} server_t;

typedef struct recv_thread_data_s {
    int total_size;
    int used_size;
    char buf[1];
} recv_thread_data_t;

void srv_recv_cb(struct ev_loop* loop, ev_io* w, int revents)
{
    work_thread_t* me;
    server_t* server;
    server_stat_t* stat;
    recv_thread_data_t* td;
    int msg_too_long = 0;
    struct sockaddr_in client_addr;
    int sa_len = sizeof(client_addr);
    int msg_len, ret;

    me = (work_thread_t*)ev_userdata(loop);
    server = me->server;
    stat = &(server->stat);

    // msg save format:
    // msg         + \t + ip + \t + time + \n
    // MSG_MAXSIZE + 1  + 20 + 1  +  20  + 1  < 50 + MSG_MAXSIZE

    if (me->thread_data != NULL) {
        td = (recv_thread_data_t*)me->thread_data;
        if ((td->total_size - td->used_size) < (50 + MSG_MAXSIZE)) {
            mempl_mt_stage_push(server->mempl, MEMPL_STAGE_WRITE, me->thread_data);
            ev_async_send(server->writ_thread.loop, &(server->writ_thread.wake_watcher));
            me->thread_data = NULL;
        }
    }

    if (me->thread_data == NULL) {
        me->thread_data = malloc(DATA_BUFSIZE);
        if (me->thread_data == NULL) {
            DEFAULT_LOG_WRITE(LOG_LEVEL_WARN, "srv_recv_cb() : malloc failed");
            return;
        }
        td = (recv_thread_data_t*)me->thread_data;
        td->total_size = DATA_BUFSIZE + 1 - sizeof(recv_thread_data_t);
        td->used_size  = 0;
    }

    td = (recv_thread_data_t*)me->thread_data;

    msg_len = recvfrom(w->fd, td->buf + td->used_size, MSG_MAXSIZE, MSG_TRUNC,
        (struct sockaddr*)&client_addr, &sa_len);
    if (msg_len <= 0) return;
    if (msg_len > MSG_MAXSIZE) {
        msg_len = MSG_MAXSIZE;
        msg_too_long = 1;
    }
    td->used_size += msg_len;
    td->buf[td->used_size++] = '\t';

    ret = sock_ntop(&client_addr, td->buf + td->used_size, 20);
    td->used_size += ret;
    td->buf[td->used_size++] = '\t';

    ret = curtime_str(td->buf + td->used_size, 20);
    td->used_size += ret;
    td->buf[td->used_size++] = '\n';

    stat->msgc_total += 1;
    stat->recv_total += msg_len;
}

void thread_recv_wake_cb(work_thread_t* me)
{
    server_t* server = me->server;

    if (server->need_exit) {
        if (me->thread_data != NULL) {
            mempl_mt_stage_push(server->mempl, MEMPL_STAGE_WRITE, me->thread_data);
            ev_async_send(server->writ_thread.loop, &(server->writ_thread.wake_watcher));
            me->thread_data = NULL;
        }
    }
}

void* thread_recv_func(void* arg)
{
    work_thread_t* me;
    server_t* server;
    ev_io recv_watcher;

    me = (work_thread_t*)arg;
    server = me->server;

    me->thread = pthread_self();
    me->thread_data = NULL;
    me->wake_cb = thread_recv_wake_cb;

    ev_io_init(&recv_watcher, srv_recv_cb, server->sock_fd, EV_READ);
    ev_io_start(me->loop, &recv_watcher);

    ev_loop(me->loop, 0);

    return NULL;
}


void thread_writ_wake_cb(work_thread_t* me)
{
    server_t* server = me->server;
    server_stat_t* stat = &(server->stat);
    recv_thread_data_t* td = NULL;

    for ( ; ; ) {
        if (mempl_mt_stage_pop(server->mempl, MEMPL_STAGE_WRITE, (void**)&td) != 0) {
            DEFAULT_LOG_WRITE(LOG_LEVEL_WARN, "thread_writ_wake_cb() : mempl_mt_stage_pop");
            break;
        }

        if (td == NULL) {
            // stage memory list empty
            break;
        }

        if (td->used_size != 0) {
            if (write(server->data_fd, td->buf, td->used_size) != td->used_size) {
                DEFAULT_LOG_WRITE(LOG_LEVEL_ERROR, "thread_writ_wake_cb() : write failed : %s", strerror(errno));
            } else {
                stat->writ_total += td->used_size;
            }
        }

        free(td);
    }
}

void* thread_writ_func(void* arg)
{
    work_thread_t* me;

    me = (work_thread_t*)arg;

    me->thread = pthread_self();
    me->thread_data = NULL;
    me->wake_cb = thread_writ_wake_cb;

    ev_loop(me->loop, 0);

    return NULL;
}


void stat_update_cb(struct ev_loop* loop, ev_timer* w, int revents)
{
    work_thread_t* me;
    server_t* server;
    server_stat_t* stat;
    long long now_time, last_time, time_elapse;
    long long msgc_completed, recv_completed, writ_completed;

    me = (work_thread_t*)ev_userdata(loop);
    server = me->server;
    stat = &(server->stat);

    now_time = ev_now(loop);

    if (stat->last_time == 0) {
        stat->last_time = now_time;
        stat->msgc_last = stat->msgc_total;
        stat->recv_last = stat->recv_total;
        stat->writ_last = stat->writ_total;
        return;
    }

    last_time = stat->last_time;
    msgc_completed = stat->msgc_total - stat->msgc_last;
    recv_completed = stat->recv_total - stat->recv_last;
    writ_completed = stat->writ_total - stat->writ_last;

    stat->last_time = now_time;
    stat->msgc_last = stat->msgc_total;
    stat->recv_last = stat->recv_total;
    stat->writ_last = stat->writ_total;

    time_elapse = now_time - last_time;
    if (time_elapse > 0) {
        printf("Request Process   Rate : %lld\n", msgc_completed / time_elapse);
        printf("Data    Transfer  Rate : %lld\n", recv_completed / time_elapse);
        printf("Data    Write     Rate : %lld\n\n", writ_completed / time_elapse);
    }
}

void* thread_stat_func(void* arg)
{
    work_thread_t* me;
    server_t* server;
    ev_timer stat_watcher;

    me = (work_thread_t*)arg;
    server = me->server;

    me->thread = pthread_self();
    me->thread_data = NULL;
    me->wake_cb = NULL;

    ev_timer_init(&stat_watcher, stat_update_cb, 0., 60.);
    ev_timer_start(me->loop, &stat_watcher);

    ev_loop(me->loop, 0);

    return NULL;
}


void async_wake(struct ev_loop* loop, ev_async* w, int revents)
{
    work_thread_t* me;
    server_t* server;

    me = (work_thread_t*)ev_userdata(loop);
    server = me->server;

    if (me->wake_cb) {
        me->wake_cb(me);
    }

    if (server->need_exit) {
        ev_break(loop, EVBREAK_ALL);
    }
}

int setup_work_thread(work_thread_t* me, server_t* server)
{
    me->loop = ev_loop_new(0);
    if (me->loop == NULL) {
        return -1;
    }

    ev_set_userdata(me->loop, me);

    ev_async_init(&me->wake_watcher, async_wake);
    ev_async_start(me->loop, &me->wake_watcher);

    me->server = server;

    return 0;
}

int create_work_thread(void*(*func)(void*), work_thread_t* me)
{
    int ret;
    pthread_t thread;

    if ((ret = pthread_create(&thread, NULL, func, (void*)me)) != 0)
        return -1;

    me->thread = thread;

    return 0;
}

int work_threads_init(server_t* server)
{
    if (setup_work_thread(&(server->recv_thread), server) != 0 ||
        create_work_thread(thread_recv_func, &(server->recv_thread)) != 0) {
        return -1;
    }

    if (setup_work_thread(&(server->writ_thread), server) != 0 ||
        create_work_thread(thread_writ_func, &(server->writ_thread)) != 0) {
        return -1;
    }

    if (setup_work_thread(&(server->stat_thread), server) != 0 ||
        create_work_thread(thread_stat_func, &(server->stat_thread)) != 0) {
        return -1;
    }

    return 0;
}

void work_threads_term(server_t* server)
{
    server->need_exit = 1;

    ev_async_send(server->recv_thread.loop, &(server->recv_thread.wake_watcher));
    ev_async_send(server->writ_thread.loop, &(server->writ_thread.wake_watcher));
    ev_async_send(server->stat_thread.loop, &(server->stat_thread.wake_watcher));

    pthread_join(server->recv_thread.thread, NULL);
    pthread_join(server->writ_thread.thread, NULL);
    pthread_join(server->stat_thread.thread, NULL);

    ev_loop_destroy(server->recv_thread.loop);
    ev_loop_destroy(server->writ_thread.loop);
    ev_loop_destroy(server->stat_thread.loop);
}


void sig_exit_cb(struct ev_loop* loop, ev_signal* w, int revents)
{
    server_t* server = (server_t*)ev_userdata(loop);

    server->need_exit = 1;

    ev_async_send(server->recv_thread.loop, &(server->recv_thread.wake_watcher));
    ev_async_send(server->writ_thread.loop, &(server->writ_thread.wake_watcher));
    ev_async_send(server->stat_thread.loop, &(server->stat_thread.wake_watcher));

    ev_break(loop, EVBREAK_ALL);
}


int main(int argc, char** argv)
{
    int is_daemon = IS_DAEMON;

    server_t server;
    mempl_callback_t mempl_cb;
    struct ev_loop* loop;
    ev_signal sigint_watcher, sigterm_watcher;

    if (is_daemon && daemon_init() != 0) {
        DEFAULT_LOG_WRITE(LOG_LEVEL_FATAL, "main() : daemon_init()");
        return EXIT_FAILURE;
    }

    if (DEFAULT_LOG_OUTPUT_TO(ERR_LOG) != 0) {
        DEFAULT_LOG_WRITE(LOG_LEVEL_FATAL, "main() : DEFAULT_LOG_OUTPUT_TO()");
        return EXIT_FAILURE_ARG;
    }

    if (create_pid_file(PID_FILE) != 0) {
        DEFAULT_LOG_WRITE(LOG_LEVEL_FATAL, "main() : create_pid_file()");
        return EXIT_FAILURE_ARG;
    }

    memset(&server, 0, sizeof(server_t));
    memset(&mempl_cb, 0, sizeof(mempl_callback_t));

    if ((server.data_fd = open(SVR_DATA,
                O_WRONLY | O_CREAT | O_APPEND, S_IRWXU)) < 0) {
        DEFAULT_LOG_WRITE(LOG_LEVEL_FATAL, "main() : open logfile");
        return EXIT_FAILURE_ARG;
    }

    if ((server.sock_fd = bind_socket(SVR_IP, SVR_PORT)) < 0) {
        DEFAULT_LOG_WRITE(LOG_LEVEL_FATAL, "main() : bind_socket()");
        return EXIT_FAILURE_ARG;
    }

    server.mempl = mempl_mt_create(MEMPL_STAGE_COUNT, &mempl_cb);
    if (server.mempl == NULL) {
        DEFAULT_LOG_WRITE(LOG_LEVEL_FATAL, "main() : mempl_mt_create()");
        return EXIT_FAILURE;
    }

    server.need_exit = 0;

    if (work_threads_init(&server) != 0) {
        DEFAULT_LOG_WRITE(LOG_LEVEL_FATAL, "main() : work_threads_init()");
        return EXIT_FAILURE;
    }

    loop = ev_default_loop(0);
    ev_set_userdata(loop, &server);

    ev_signal_init(&sigint_watcher, sig_exit_cb, SIGINT);
    ev_signal_start(loop, &sigint_watcher);
    ev_signal_init(&sigterm_watcher, sig_exit_cb, SIGTERM);
        ev_signal_start(loop, &sigterm_watcher);

    signal(SIGPIPE, SIG_IGN);

    ev_loop(loop, 0);

    work_threads_term(&server);
    mempl_mt_destroy(server.mempl);
    close(server.sock_fd);
    close(server.data_fd);

    return EXIT_SUCCESS;
}


int daemon_init()
{
    const int MAXFD = 64;
    int i;
    pid_t pid;

    umask(0);

    if ((pid = fork()) < 0)
        return (-1);
    else if (pid)
        _exit(0);                  /* parent terminates */

    /* child 1 continues... */

    if (setsid() < 0)              /* become session leader */
        return (-1);

    signal(SIGHUP, SIG_IGN);

    if ((pid = fork()) < 0)
        return (-1);
    else if (pid)
        _exit(0);                  /* child 1 terminates */

    /* child 2 continues... */

    chdir("/");                    /* change working directory */

    /* close off file descriptors */
    for (i = 0; i < MAXFD; ++i)
        close(i);

    /* redirect stdin, stdout and stderr to /dev/null */
    open("/dev/null", O_RDONLY);
    open("/dev/null", O_RDWR);
    open("/dev/null", O_RDWR);

    return (0);
}


int create_pid_file(const char* pid_file)
{
    FILE*  pidf;

    if ((pidf = fopen(pid_file, "w")) == NULL)
        return -1;

    fprintf(pidf, "%d\n", getpid());

    fclose(pidf);

    return 0;
}


int curtime_str(char* buf, int buf_len)
{
    time_t ttNow;
    struct tm tmNow;

    ttNow = time(NULL);
    gmtime_r(&ttNow, &tmNow);

    return snprintf(
        buf, buf_len,
        "%04d%02d%02d-%02d:%02d:%02d",
        tmNow.tm_year + 1900,
        tmNow.tm_mon + 1,
        tmNow.tm_mday,
        tmNow.tm_hour,
        tmNow.tm_min,
        tmNow.tm_sec);
}


int sock_ntop(struct sockaddr_in* sin, char* text, size_t len)
{
    unsigned char* p = (unsigned char *)&(sin->sin_addr);
    return snprintf(text, len, "%d.%d.%d.%d", (int)p[0], (int)p[1], (int)p[2], (int)p[3]);
}

void fd_set_nonblock(int fd)
{
    int flags;
    flags = fcntl(fd, F_GETFL);
    if (!(flags & O_NONBLOCK)) {
        flags = flags | O_NONBLOCK;
        fcntl(fd, F_SETFL, flags);
    }
}

int fd_set_reuseaddr(int fd)
{
    int flag = 1;
    return setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));
}

int bind_socket(const char* ip, unsigned short port)
{
    int sock_fd;
    struct sockaddr_in sin;

    if ((sock_fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        DEFAULT_LOG_WRITE(LOG_LEVEL_ERROR, "bind_socket() : socket()");
        return -1;
    }

    if(fd_set_reuseaddr(sock_fd) != 0) {
        close(sock_fd);
        DEFAULT_LOG_WRITE(LOG_LEVEL_ERROR, "bind_socket() : fd_set_reuseaddr()");
        return -1;
    }
    fd_set_nonblock(sock_fd);

    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = htonl(INADDR_ANY);
    if (ip) sin.sin_addr.s_addr = inet_addr(ip);
    sin.sin_port = htons(port);

    if (bind(sock_fd, (struct sockaddr*)&sin, sizeof(struct sockaddr)) != 0) {
        close(sock_fd);
        DEFAULT_LOG_WRITE(LOG_LEVEL_ERROR, "bind_socket() : bind() : %s", strerror(errno));
        return -1;
    }

    return sock_fd;
}

