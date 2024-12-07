set(app_src pb_common.c pb_decode.c pb_encode.c tahu.c tahu.pb.c)
idf_component_register(SRCS ${app_src}
                    INCLUDE_DIRS include
                    PRIV_INCLUDE_DIRS ""
                    PRIV_REQUIRES  ""
                    REQUIRES ""
                    EMBED_FILES "tahu.pb")