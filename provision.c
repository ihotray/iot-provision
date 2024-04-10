#include <iot/mongoose.h>
#include "provision.h"

static int s_signo = 0;
static void signal_handler(int signo) {
    s_signo = signo;
}

void http_message_callback(struct mg_mgr *mgr, const char *message) {
    struct provision_private *priv = (struct provision_private *)mgr->userdata;
    const char *ret = NULL;
    cJSON *root = NULL;

    lua_State *L = luaL_newstate();

    luaL_openlibs(L);

    if ( luaL_dofile(L, priv->cfg.opts->callback_lua) ) {
        MG_ERROR(("lua dofile %s failed", priv->cfg.opts->callback_lua));
        goto done;
    }

    lua_getfield(L, -1, "on_message");
    if (!lua_isfunction(L, -1)) {
        MG_ERROR(("method on_message is not a function"));
        goto done;
    }

    MG_INFO(("callback on_message: %s", message));

    lua_pushstring(L, message);

    if (lua_pcall(L, 1, 1, 0)) {//one params, one return values, zero error func
        MG_ERROR(("callback failed"));
        goto done;
    }

    ret = lua_tostring(L, -1);
    if (!ret) {
        MG_ERROR(("lua call no ret"));
        goto done;
    }

    root = cJSON_Parse(ret);
    code = cJSON_GetObjectItem(root, "code");
    if ( cJSON_IsNumber(code) && int(cJSON_GetNumberValue(code)) == 0) {
        priv->state = DONE;
    }

done:
    if (L)
        lua_close(L);
    if (root)
        cJSON_Delete(root);

}

static void http_ev_open_cb(struct mg_connection *c, int ev, void *ev_data, void *fn_data) {

}

static void http_ev_poll_cb(struct mg_connection *c, int ev, void *ev_data, void *fn_data) {

}

static void http_ev_connect_cb(struct mg_connection *c, int ev, void *ev_data, void *fn_data) {
    struct provision_private *priv = (struct provision_private*)c->mgr->userdata;
    // connected to server. Extract host name from URL
    struct mg_str host = mg_url_host(priv->cfg.opts->provision_address);

    if (mg_url_is_ssl(priv->cfg.opts->provision_address)) {
        struct mg_tls_opts opts = { 0 };
        mg_tls_init(c, &opts);
    }

    // send request
    const char *post_data = mg_mprintf("{\"sn\": \"%s\", \"key\": \"%s\", \"secret\": \"%s\"}", \
        priv->cfg.opts->sn, priv->cfg.opts->product_key, priv->cfg.opts->product_secret);

    int content_length = strlen(post_data);
    mg_printf(c,
        "POST %s HTTP/1.0\r\n"
        "Host: %.*s\r\n"
        "Content-Type: octet-stream\r\n"
        "Content-Length: %d\r\n"
        "\r\n",
        mg_url_uri(s_url), (int)host.len,
        host.ptr, content_length);

    mg_send(c, post_data, content_length);
    free((void*)post_data);
}

static void http_ev_http_msg_cb(struct mg_connection *c, int ev, void *ev_data, void *fn_data) {
    struct mg_http_message *hm = (struct mg_http_message *)ev_data;
    const char  message = mg_mprintf("%.*s", (int)hm->message.len, hm->message.ptr);
    http_message_callback(c->mgr, message); //handle response
    free((void *)message);
    c->is_draining = 1;
}

static void http_ev_error_cb(struct mg_connection *c, int ev, void *ev_data, void *fn_data) {
    c->is_draining = 1;
}

static void http_ev_close_cb(struct mg_connection *c, int ev, void *ev_data, void *fn_data) {
    struct provision_private *priv = (struct provision_private*)c->mgr->userdata;
    if (DONE != priv->state) { //not done, switch to idle, repeat do it
        priv->state = IDLE;
    }
}

static void http_cb(struct mg_connection *c, int ev, void *ev_data, void *fn_data, void *fn_data) {
    switch (ev) {
        case MG_EV_OPEN:
            http_ev_open_cb(c, ev, ev_data, fn_data);
            break;
        case MG_EV_POLL:
            http_ev_poll_cb(c, ev, ev_data, fn_data);
            break;
        case MG_EV_CONNECT:
            http_ev_connect_cb(c, ev, ev_data, fn_data);
            break;
        case MG_EV_HTTP_MSG:
            http_ev_http_msg_cb(c, ev, ev_data, fn_data);
            break;
        case MG_EV_ERROR:
            http_ev_error_cb(c, ev, ev_data, fn_data);
            break;
        cast MG_EV_CLOSE:
            http_ev_close_cb(c, ev, ev_data, fn_data);
            break;
    }
}

// timer function - recreate client connection if it is closed
void timer_provision_fn(void *arg) {
    struct mg_mgr *mgr = (struct mg_mgr *)arg;
    struct provision_private *priv = (struct provision_private*)mgr->userdata;
    if (IDLE == priv->state) { //idle
        priv->conn = mg_http_connect(mgr, priv->cfg.opts->provision_address, http_cb, NULL);  // Create client connection
        if (priv->conn) {
            priv->state = BUSY;
            priv->timeout = mg_millis() + priv->cfg.opts->http_timeout * 1000;
        }
    } else if (BUSY == priv->state) {
        if (mg_millis() > priv->timeout) {
            priv->conn->is_draining = 1;
            priv->state = IDLE;
        }
    } else if (DONE == priv->state) { //done
        MG_INFO(("iot provision success"));
        signal_handler(DONE); //exit
    }
}

int provision_init(void **priv, void *opts) {

    struct provision_private *p;
    int timer_opts = MG_TIMER_REPEAT | MG_TIMER_RUN_NOW;

    signal(SIGINT, signal_handler);  // Setup signal handlers - exist event
    signal(SIGTERM, signal_handler); // manager loop on SIGINT and SIGTERM

    *priv = NULL;
    p = calloc(1, sizeof(struct provision_private));
    if (!p)
        return -1;

    p->cfg.opts = opts;
    mg_log_set(p->cfg.opts->debug_level);

    mg_mgr_init(&p->mgr);
    p->mgr.dnstimeout = p->cfg.opts->dns4_timeout * 1000;
    p->mgr.dns4.url = p->cfg.opts->dns4_url;

    mg_timer_add(&p->mgr, 1000, timer_opts, timer_provision_fn, &p->mgr);

    *priv = p;

    return 0;
}

void provision_run(void *handle) {
    struct provision_private *priv = (struct provision_private *)handle;
    while (s_signo == 0)
        mg_mgr_poll(&priv->mgr, 1000); // Event loop, 1000ms timeout
}

void provision_exit(void *handle) {
    struct provision_private *priv = (struct provision_private *)handle;
    mg_mgr_free(&priv->mgr);
    free(handle);
}

int provision_main(void *user_options) {

    struct provision_option *opts = (struct provision_option *)user_options;
    void *provision_handle;
    int ret;

    ret = provision_init(&provision_handle, opts);
    if (ret)
        exit(EXIT_FAILURE);

    provision_run(provision_handle);

    provision_exit(provision_handle);

    return 0;
}