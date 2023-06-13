# A Universal DNS Resolver of ICANN & Web3 Names

Higher throughput resolving ICANN, Handshake and `.eth` domain names using `bind` pointed to one or more external [Handshake node, `hsd`](https://github.com/handshake-org/hsd)

This is a container that runs a DNS resolver using ISC's `bind` to get higher through-put,
but referring ROOT queries to instance(s) of `hsd`, so it can resolve Handshake names.

When you run this container, pass the environment variable `HSD_MASTERS` to a semi-coloon separated list of the IP Addresses your `hsd` instances.
If `HSD_MASTERS` is not set the Handshake feature is disabled, and only ICANN & `.eth` are supported (see below for `eth` support).

The `hsd` IP Addresses must be the ones that will respond with authritative answers (`--ns-host`).

If you wish to `syslog` to an external syslog server (listening in UDP/514) pass the environment variable `SYSLOG_SERVER` 
with the IP Address of your syslog server, otherwise it will syslog to `stdout`.

Set the environment variable `WITH_BIND_V6` to `Y` to get IPv6 support, otherwise `bind` will run in IPv4 mode only.


# ETH Support

ENS domains (domains ending `.eth`) are supported by using an Alchemy RPC account, which must be passed into the container as the
environment variable `ALCHEMY_API_CODE`.

When you request a DNS record, the first thing it will do is try & find the record you asked for. If this fails, and you asked for 
an IPv4 address, you will be given the IP of a [Universal Web Redirector](https://github.com/james-stevens/universal-web-redirect).
This UWR will then request a URI to send you to.

The ETH support first looks for a `content` record in the domain. If the domain has an IPFS or IPNS content record, you will be redirected to
`https://ipfs.io/....`, if no `content` record exists, you will be rediected to `https://<eth-name>.eth.limo/`

Impervious `forever` domains are handled the same way, except the final redirection is `https://<eth-name>.forever.limo/`, 
which probably doesn't do anything useful.


For this service to work correctly, you must set the environmant variables

`ALCHEMY_API_CODE` - An Alchemy RPC account API code

`UWR_IP_ADDRESSES` - A comma separated list of IPv4 addresses of Universal Web Redirector servers


# DoH Support

From v9.17.10 onwards ISC's `bind` now has native DoH support, which is enabled & available in this container.

However, `bind` DoH has been deliberately set to run in HTTP mode only, so you will need to run an external TLS off-loader/shedder
to get the TLS support. A good choice is `haproxy`. This can also be used to provide load-balalncing & failover
between multiple instances of this container.

Here's a very simple `haproxy` configuration, which should be all you need to get TLS & load-balancing/failover support.

	frontend doh-plain-http1-1
		mode http
		timeout client 10s
		bind <EXTERNAL-IP>:443 v4v6 tfo ssl crt /opt/pems/<YOUR-PEM>.pem alpn h2,http/1.1
		default_backend doh-server-plain-http2

	backend doh-server-plain-http2
		mode http
		timeout connect 10s
		timeout server 10s
		server container-1 <THIS-CONTAINER-1>:80 proto h2
		server container-2 <THIS-CONTAINER-2>:80 proto h2

where

`<EXTERNAL-IP>` is the external (e.g. internet) IP Address you want to access your DoH service from

`<YOUR-PEM>` is the name of your TLS PEM file, i.e. a PEM of both your private key & CA Certificiate

`<THIS-CONTAINER-1>` is the IP Address of instance-1 of this container

`<THIS-CONTAINER-2>` is the IP Address of instance-2 of this container

NOTE: The path of the DoH service can either be `/` or `/doh`, so you'd put `https://some.host.name/` or `https://some.host.name/doh` into your browser.
Where `some.host.name` resolves to `EXTERNAL-IP` and `YOUR-PEM` is valid for that hostname.

Using the path `/doh` can be useful if you want your DoH service to co-exist on the same host name as other web services,
as you can then redirect to the DoH service based on the path.


# docker.com

The lastest build of this container is available from docker.com

https://hub.docker.com/r/jamesstevens/handshake-bind-hns
