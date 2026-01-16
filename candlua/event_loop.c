// Revised event loop: Lua creates coroutines and registers them via c_register(co)
// C exposes c_register, c_sleep, c_waitfd. C keeps a list of registered coroutine
// threads and resumes them; when a coroutine yields it must return two values:
//  tag ("sleep" or "waitfd") and a value (seconds or fd).

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

typedef enum { EVT_SLEEP, EVT_FD } EvType;

typedef struct Event {
    EvType type;
    lua_State *th; /* coroutine */
    double wake;   /* for sleep */
    int fd;        /* for fd wait */
    struct Event *next;
} Event;

static Event *events = NULL;

static double now_seconds(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

static void add_event(Event *e) {
    e->next = events;
    events = e;
}

static void remove_event(Event *prev, Event *cur) {
    if (prev) prev->next = cur->next; else events = cur->next;
    free(cur);
}

/* C functions exposed to Lua */
static int l_c_sleep(lua_State *L) {
    double s = luaL_checknumber(L, 1);
    lua_pushstring(L, "sleep");
    lua_pushnumber(L, s);
    return lua_yield(L, 2);
}

static int l_c_waitfd(lua_State *L) {
    int fd = luaL_checkinteger(L, 1);
    lua_pushstring(L, "waitfd");
    lua_pushinteger(L, fd);
    return lua_yield(L, 2);
}

/* Register a coroutine created in Lua: c_register(co) */
static int l_c_register(lua_State *L) {
    if (!lua_isthread(L, 1)) return luaL_error(L, "expected coroutine/thread");
    lua_State *th = lua_tothread(L, 1);
    Event *e = (Event*)(calloc(1, sizeof(Event)));
    e->type = EVT_SLEEP; e->th = th; e->wake = 0; e->fd = -1;
    add_event(e);
    printf("[debug] registered coroutine %p\n", (void*)th);
    return 0;
}

int main(int argc, char **argv) {
    setvbuf(stdout, NULL, _IONBF, 0);
    lua_State *L = luaL_newstate();
    luaL_openlibs(L);

    lua_register(L, "c_sleep", l_c_sleep);
    lua_register(L, "c_waitfd", l_c_waitfd);
    lua_register(L, "c_register", l_c_register);

    /* create a pipe and fork a writer child that writes after delay */
    int pipefd[2];
    if (pipe(pipefd) != 0) {
        perror("pipe");
        return 1;
    }
    pid_t pid = fork();
    if (pid < 0) { perror("fork"); return 1; }
    if (pid == 0) {
        /* child: close read end, sleep, write message */
        close(pipefd[0]);
        sleep(3);
        const char *msg = "hello from child\n";
        write(pipefd[1], msg, strlen(msg));
        close(pipefd[1]);
        _exit(0);
    }
    /* parent: close write end, keep read end */
    close(pipefd[1]);

    /* expose read fd to Lua as global */
    lua_pushinteger(L, pipefd[0]);
    lua_setglobal(L, "PIPE_READ_FD");

    /* load demo script */
    const char *script = "event_demo.lua";
    if (argc > 1) script = argv[1];
    if (luaL_dofile(L, script) != LUA_OK) {
        fprintf(stderr, "lua error: %s\n", lua_tostring(L, -1));
        lua_close(L);
        return 1;
    }

    /* At this point the Lua script should have created coroutines and called c_register on each.
       Our 'events' list contains entries with th set. We now run the event loop. */

    printf("[debug] entering event loop\n");
    while (events) {
        double now = now_seconds();
        double timeout = 1.0; /* default timeout */
        fd_set rfds;
        FD_ZERO(&rfds);
        int maxfd = -1;

        for (Event *e = events; e; e = e->next) {
            if (e->type == EVT_SLEEP) {
                double t = e->wake - now;
                if (t < 0) t = 0;
                if (t < timeout) timeout = t;
            } else if (e->type == EVT_FD) {
                FD_SET(e->fd, &rfds);
                if (e->fd > maxfd) maxfd = e->fd;
            }
        }

        struct timeval tv;
        tv.tv_sec = (int)timeout;
        tv.tv_usec = (int)((timeout - tv.tv_sec) * 1e6);

        int sel = select(maxfd+1, &rfds, NULL, NULL, &tv);
        now = now_seconds();

        /* handle fd-ready events */
        for (Event *prev = NULL, *cur = events; cur; ) {
            if (cur->type == EVT_FD && FD_ISSET(cur->fd, &rfds)) {
                char buf[1024];
                ssize_t r = read(cur->fd, buf, sizeof(buf)-1);
                if (r < 0) r = 0;
                buf[r] = '\0';
                /* resume with data argument */
                lua_pushstring(cur->th, buf);
                printf("[debug] resuming coroutine %p with fd data\n", (void*)cur->th);
                int status = lua_resume(cur->th, L, 1, NULL);
                printf("[debug] resume returned %d\n", status);
                if (status == LUA_YIELD) {
                    int top = lua_gettop(cur->th);
                    if (top >= 2) {
                        const char *tag = lua_tostring(cur->th, -2);
                        if (tag && strcmp(tag, "sleep") == 0) {
                            double s = lua_tonumber(cur->th, -1);
                            cur->type = EVT_SLEEP; cur->wake = now + s;
                        } else if (tag && strcmp(tag, "waitfd") == 0) {
                            cur->type = EVT_FD; cur->fd = lua_tointeger(cur->th, -1);
                        }
                        lua_settop(cur->th, 0);
                    }
                    prev = cur; cur = cur->next;
                } else if (status == LUA_OK) {
                    Event *tofree = cur;
                    cur = cur->next;
                    remove_event(prev, tofree);
                } else {
                    fprintf(stderr, "thread error: %s\n", lua_tostring(cur->th, -1));
                    Event *tofree = cur;
                    cur = cur->next;
                    remove_event(prev, tofree);
                }
            } else {
                prev = cur; cur = cur->next;
            }
        }

        /* handle expired timers */
        for (Event *prev = NULL, *cur = events; cur; ) {
            if (cur->type == EVT_SLEEP && cur->wake <= now) {
                printf("[debug] resuming coroutine %p for timer\n", (void*)cur->th);
                int status = lua_resume(cur->th, L, 0, NULL);
                printf("[debug] resume returned %d\n", status);
                if (status == LUA_YIELD) {
                    int top = lua_gettop(cur->th);
                    if (top >= 2) {
                        const char *tag = lua_tostring(cur->th, -2);
                        if (tag && strcmp(tag, "sleep") == 0) {
                            double s = lua_tonumber(cur->th, -1);
                            cur->type = EVT_SLEEP; cur->wake = now + s;
                            lua_settop(cur->th, 0);
                            prev = cur; cur = cur->next;
                        } else if (tag && strcmp(tag, "waitfd") == 0) {
                            cur->type = EVT_FD; cur->fd = lua_tointeger(cur->th, -1);
                            lua_settop(cur->th, 0);
                            prev = cur; cur = cur->next;
                        } else {
                            prev = cur; cur = cur->next;
                        }
                    } else {
                        prev = cur; cur = cur->next;
                    }
                } else if (status == LUA_OK) {
                    Event *tofree = cur;
                    cur = cur->next;
                    remove_event(prev, tofree);
                } else {
                    fprintf(stderr, "thread error: %s\n", lua_tostring(cur->th, -1));
                    Event *tofree = cur;
                    cur = cur->next;
                    remove_event(prev, tofree);
                }
            } else {
                prev = cur; cur = cur->next;
            }
        }
    }

    lua_close(L);
    return 0;
}
