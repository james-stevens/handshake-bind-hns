# handshake-bind-hns
Higher throughput resolving Handshake domain names using `bind` and `hsd`

This is a container that runs a DNS resolver using ISC's bind to get higher through-put,
but referring ROOT queries to instance(s) of `hsd`, so it can resolve Handshake names.

When you run this container, pass the environment variable `HSD_MASTERS` to a space separated list of the IP Addresses your `hsd` instances.

The `hsd` IP Addresses must be the ones that will respond with authritative answers (`--ns-host`).

If you wish to `syslog` to an external syslog server (listening in UDP/514) pass the environment variable `SYSLOG_SERVER` 
with the IP Address of your syslog server.

Unless you set the environment variable `WITH_BIND_V6` to anything, except blank, `bind` will run in IPv4 mode only.
