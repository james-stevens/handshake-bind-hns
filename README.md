# handshake-bind-hns

Higher throughput resolving ICANN, Handshake and `.eth` domain names using `bind` using an external `hsd`

This is a container that runs a DNS resolver using ISC's `bind` to get higher through-put,
but referring ROOT queries to instance(s) of `hsd`, so it can resolve Handshake names.

It also runs a proxy to `eth.link` to resolve `.eth` domains.

When you run this container, pass the environment variable `HSD_MASTERS` to a semi-coloon separated list of the IP Addresses your `hsd` instances.
If `HSD_MASTERS` is not set the Handshake feature is disabled, and only ICANN & `.eth` are supported.

The `hsd` IP Addresses must be the ones that will respond with authritative answers (`--ns-host`).

If you wish to `syslog` to an external syslog server (listening in UDP/514) pass the environment variable `SYSLOG_SERVER` 
with the IP Address of your syslog server, otherwise it will syslog to `stdout`.

Set the environment variable `WITH_BIND_V6` to `Y` to get IPv6 support, otherwise `bind` will run in IPv4 mode only.


## ETH Support

This resolver can also resolve `eth` names via `https://eth.link/` - this is pretty slow, but answers will be held in local cache.


## DoH Support

From v9.17.10 onwards ISC's `bind` now has native DoH support, which is enabled & available in this container.

However, `bind` DoH has been deliberately set to run in HTTP mode only, so you will need to run an external TLS off-loader
to get TLS support. A good choice is `haproxy`. This can also be used to provide load-balalncing & failover
between multiple instances of this container.

Here's a very simple `haproxy` configuration, which should be all you need to get TLS & load-balancing/failover support.

	frontend doh-plain-http1-1
		mode http
		timeout client 10s
		bind `EXTERNAL-IP`:443 v4v6 tfo ssl crt /opt/pems/<YOUR-PEM>.pem alpn h2,http/1.1
		default_backend doh-server-plain-http2

	backend doh-server-plain-http2
		mode http
		timeout connect 10s
		timeout server 10s
		server container-1 <THIS-CONTAINER-1>:80 proto h2
		server container-2 <THIS-CONTAINER-2>:80 proto h2

where

`EXTERNAL-IP` is the external (e.g. internet) IP Address you want to access your DoH service from

`YOUR-PEM` is the name of your TLS PEM file, i.e. your private key signed by a CA

`THIS-CONTAINER-1` is the IP Address of instance-1 of this container

`THIS-CONTAINER-2` is the IP Address of instance-2 of this container

NOTE: the path of the doh service is `/doh`, so you will need to put `https://some.host.name/doh` into your browser.
where `some.host.name` resolves to `EXTERNAL-IP` and `YOUR-PEM` is valid for that hostname.
