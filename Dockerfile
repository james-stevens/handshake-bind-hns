# (c) Copyright 2019-2020, James Stevens ... see LICENSE for details
# Alternative license arrangements are possible, contact me for more information

FROM alpine:3.18

RUN apk add bind python3 py3-dnspython py3-requests
RUN rm -f /etc/periodic/monthly/dns-root-hints

RUN apk add tcpdump

RUN rm -rf /run /tmp
RUN ln -s /dev/shm /run
RUN ln -s /dev/shm /tmp

COPY inittab /etc/inittab
COPY named.conf /usr/local/etc/

COPY start_eth_redirect start_bind start_syslogd /usr/local/bin/
COPY eth_redirect.py /usr/local/bin/
RUN python3 -m compileall /usr/local/bin/

CMD [ "/sbin/init" ]
