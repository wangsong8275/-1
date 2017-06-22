#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <ev++.h>

#include "gamesvr.h"
#include "log.h"
#include "redisSub.h"

extern CDdzsvr ddzsvr;
extern Log xt_log;

void subCallback(redisAsyncContext *c, void *r, void *priv);
void connectCallback(const redisAsyncContext *c, int status);
void disconnectCallback(const redisAsyncContext *c, int status);

RedisSub::RedisSub()
{
	ac = NULL;
	reply = NULL;
	str.clear();
}

RedisSub::~RedisSub()
{
	if (ac) {
		redisAsyncDisconnect(ac);
		redisAsyncFree(ac);
	}
	if (reply) {
		freeReplyObject(reply);
	}
}

int RedisSub::init(std::string host, int port, std::string pass)
{
	_host = host;
	_port = port;
	_pass = pass;

	return async_connect_redis();
}

int RedisSub::async_connect_redis()
{
	if (ac) {
		redisAsyncFree(ac);
	}

	ac = redisAsyncConnect(_host.c_str(), _port);
	if (ac->err) {
		xt_log.error("async redis[%s] host[%s] port[%d]\n", ac->errstr, _host.c_str(), _port);
		return -1;
	}
	xt_log.error("async redis host[%s] port[%d] is connected succ.\n", _host.c_str(), _port);

	redisLibevAttach(ddzsvr.loop, ac);
	redisAsyncSetConnectCallback(ac, connectCallback);
	redisAsyncSetDisconnectCallback(ac, disconnectCallback);
	redisAsyncCommand(ac, NULL, NULL, "auth %s", _pass.c_str());
	redisAsyncCommand(ac, subCallback, (char*)"sub", "SUBSCRIBE zjh_speaker");

	return 0;
}

void RedisSub::deinit()
{
	if (ac) {
		redisAsyncDisconnect(ac);
		redisAsyncFree(ac);
	}
	ac = NULL;
}

//void RedisSub::subCallback(redisAsyncContext *c, void *r, void *priv)
void subCallback(redisAsyncContext *c, void *r, void *priv)
{
	redisReply *reply = (redisReply *)r;
	if (reply == NULL) {
		return;
	}

	if (reply->type == REDIS_REPLY_ARRAY && reply->elements == 3) {
		if (strcmp( reply->element[0]->str, "subscribe" ) != 0) {
			xt_log.error("Received[%s] channel %s: %s\n", (char*)priv, reply->element[1]->str, reply->element[2]->str);
			std::string subdata = reply->element[2]->str;
			ddzsvr.subscribe(subdata);
		}
	}
}

void connectCallback(const redisAsyncContext *c, int status)
{
	if (status != REDIS_OK) {
		xt_log.error("connectCallback Error: %s\n", c->errstr);
		return;
	}
	xt_log.error("async redis Connected...\n");
}

void disconnectCallback(const redisAsyncContext *c, int status)
{
	if (status != REDIS_OK) {
		xt_log.error("disconnectCallback Error: %s\n", c->errstr);
		return;
	}
	xt_log.error("async redis Disconnected...\n");
}
