#! /usr/bin/python3
# (c) Copyright 2022-2025, James Stevens ... see LICENSE for details
# Alternative license arrangements possible, contact me for more information
""" module to resolve ETH domain via https://eth.link/ """

from syslog import syslog
import argparse
import socket
from dns import message, rrset, rdatatype

AUTH_IP = "127.1.0.1"

ETH_LINK_BASE_URL = "https://eth.link/dns-query"
DNS_MAX_QUERY = 1024
DEFAULT_TTL = 86400
MAX_THREADS = 20

uwr_ips = []

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.bind((AUTH_IP, 53))

to_syslog = False
with_logging = False


def log(line):
    if not with_logging:
        return
    if to_syslog:
        syslog(line)
    else:
        print(line)


def add_soa(reply):
    reply.authority.append(
        rrset.from_text("eth.", DEFAULT_TTL, "IN", "SOA",
                        "eth. eth. 1 10800 3600 604800 3600"))


class Resolver:
    def __init__(self):
        self.query = None

    def error(self, rcode):
        reply = message.make_response(self.query.line_query)
        reply.flags = self.query.line_query.flags | 0x8400
        reply.set_rcode(rcode)
        reply.id = self.query.asker[0]
        add_soa(reply)
        sock.sendto(reply.to_wire(), self.query.asker[1])

    def url_name(self):
        if self.query.qname[:11] == "_http._tcp.":
            return self.query.qname[11:]
        return self.query.qname

    def make_txt(self):
        return rrset.from_text(self.query.qname + ".", DEFAULT_TTL, "IN",
                               "TXT", f'"http://{self.url_name()}.limo/"')

    def make_uri(self):
        return rrset.from_text(f"{self.query.qname}.", DEFAULT_TTL, "IN",
                               "URI", f'1 1 "http://{self.url_name()}.limo/"')

    def answer(self):
        if self.query is None:
            return
        qry = self.query

        if qry.qname[-4:] != ".eth" and qry.qname != "eth":
            return self.error(5)

        reply = message.make_response(qry.line_query)
        reply.index = None
        reply.flags = 0x8400
        reply.id = qry.asker[0]
        add_soa(reply)

        if qry.rdtype not in [rdatatype.A, rdatatype.URI, rdatatype.TXT]:
            return sock.sendto(reply.to_wire(), qry.asker[1])

        if qry.rdtype == rdatatype.A:
            for ip in uwr_ips:
                reply.answer.append(
                    rrset.from_text(qry.qname + ".", DEFAULT_TTL, "IN", "A",
                                    ip))

        elif qry.rdtype == rdatatype.URI:
            reply.answer.append(self.make_uri())

        elif qry.rdtype == rdatatype.TXT:
            reply.answer.append(self.make_txt())

        sock.sendto(reply.to_wire(), qry.asker[1])


class Query:
    def __init__(self, qry, sender):
        self.line_query = qry
        self.qname = str(qry.question[0].name)[:-1]
        self.rdtype = qry.question[0].rdtype
        self.qtype = rdatatype.to_text(self.rdtype)
        self.asker = (qry.id, sender)
        log(f"ASK: {self.asker} KEY:{self.qtype}|{self.qname}")


parser = argparse.ArgumentParser(description='EPP Jobs Runner')
parser.add_argument("-s", '--syslog', action="store_true", default=False)
parser.add_argument("-l", '--logging', action="store_true")
parser.add_argument("-u", '--uwr', required=True)
args = parser.parse_args()

uwr_ips = args.uwr.split(",")

to_syslog = args.syslog
with_logging = args.logging

resolver = Resolver()
while True:
    bin_qry, sender = sock.recvfrom(DNS_MAX_QUERY)
    new_query = Query(message.from_wire(bin_qry), sender)
    resolver.query = new_query
    resolver.answer()
