set(app_src  
actions.c
callbacks.c
connect.c
handle_auth.c
handle_connack.c
handle_disconnect.c
handle_ping.c
handle_pubackcomp.c
handle_publish.c
handle_pubrec.c
handle_pubrel.c
handle_suback.c
handle_unsuback.c
helpers.c
logging_mosq.c
loop.c
memory_mosq.c
messages_mosq.c
misc_mosq.c 
mosquitto.c 
net_mosq_ocsp.c net_mosq.c 
options.c
packet_datatypes.c
packet_mosq.c
property_mosq.c
pselect.c
read_handle.c
send_connect.c
send_disconnect.c
send_publish.c
send_subscribe.c
send_unsubscribe.c
send_mosq.c 
socks_mosq.c 
srv_mosq.c
strings_mosq.c
thread_mosq.c
time_mosq.c
tls_mosq.c
utf8_mosq.c
util_mosq.c util_topic.c 
will_mosq.c )
idf_component_register(SRCS ${app_src}
                    INCLUDE_DIRS include
                    PRIV_INCLUDE_DIRS ""
                    PRIV_REQUIRES  ""
                    REQUIRES ""
                    EMBED_FILES "")