## 基础docker镜像
FROM debian:11

WORKDIR /app

## 基础路径为../${PROJECT_ROOT}
## 复制必要的动态库
COPY liburing-2.2/src/liburing.so.2.2                  /lib/liburing.so.2
COPY libsodium-stable/src/libsodium/.libs/libsodium.so /lib/libsodium.so.23
COPY RomeSocket/build/release/librocket.so             /lib/
COPY RomeSocket/build/release/echo_server              /bin/

USER root

EXPOSE 8000

ENTRYPOINT [ "echo_server" ]