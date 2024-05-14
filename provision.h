#ifndef __PROVISION_H__
#define __PROVISION_H__

#include <iot/mongoose.h>

enum provision_state {
    IDLE = 0,
    BUSY = 1,
    DONE = 2,
};

struct provision_option {
    const char *sn;
    const char *ca;
    const char *product_key;
    const char *product_secret;
    const char *provision_address;
    const char *dns4_url;
    const char *callback_lua;
    const char *salt;
    int dns4_timeout;
    int http_timeout;        //http connection timeout
    int debug_level;
};

struct provision_config {
    struct provision_option *opts;
};

struct provision_private {
    enum provision_state state;
    struct provision_config cfg;
    struct mg_mgr mgr;
    struct mg_connection *conn;
    uint64_t timeout; 
};

int provision_main(void *user_options);

#endif // __PROVISION_H__