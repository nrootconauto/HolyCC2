#pragma once
struct eventPool;
struct event;
struct event *eventPoolAdd(struct eventPool *pool, const char *name,
                           void (*func)(void *, void *), void *data,
                           void (*killData)(void *));
void eventPoolRemove(struct eventPool *pool, struct event *event);
void eventPoolDestroy(struct eventPool *pool);
struct eventPool *eventPoolCreate();
void eventPoolTrigger(struct eventPool *pool, const char *name, void *data);
