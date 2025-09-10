#! /usr/bin/python3
# (c) Copyright 2022-2025, James Stevens ... see LICENSE for details
# Alternative license arrangements possible, contact me for more information
""" module to resolve ETH & impervious domains """

from syslog import syslog
import argparse
import os
import json
import socket
import requests
import time
import threading
from dns import message, rrset, rdatatype

AUTH_IP = "127.1.0.1"

ETH_LINK_BASE_URL = "http://127.0.0.1:8081/"
DNS_MAX_QUERY = 1024
DEFAULT_TTL = 3600
MAX_THREADS = 20

KNOWN_CODECS = {
    "ipns-ns": "https://ipfs.io/ipns/{value}",
    "ipfs-ns": "https://ipfs.io/ipfs/{value}"
    }

LIMO_URL = "https://{eth_name}.limo/"

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.bind((AUTH_IP, 53))

queries = {}
to_syslog = False
with_logging = False


def log(line):
    if not with_logging:
        return
    if to_syslog:
        syslog(line)
    else:
        print(line)


def run_resolver(idx):
    me = resolvers[idx]
    while True:
        while me.query is None:
            time.sleep(0.1)
        log(f">>>>   GIVEN: {me.idx}, {me.query.asker}")
        me.answer()
        me.clean_up()


class Resolver:

    def __init__(self, idx):
        self.eth_link = requests.Session()
        self.eth_link.headers.update({"content-type": "application/dns-json"})
        self.query = None
        self.idx = idx
        self.thread = None
        self.restart()

    def restart(self):
        self.thread = threading.Thread(target=run_resolver,
                                       args=(self.idx, ),
                                       daemon=True)

    def add_soa(self,reply):
        reply.authority.append(
            rrset.from_text(self.query.tld+".", DEFAULT_TTL, "IN", "SOA",
                            f"{self.query.tld}. {self.query.tld}. 1 10800 3600 604800 3600"))
    def run(self):
        self.thread.start()

    def error(self, rcode):
        reply = message.make_response(self.query.line_query)
        reply.flags = self.query.line_query.flags | 0x8400
        reply.set_rcode(rcode)
        reply.id = self.query.asker[0]
        self.add_soa(reply)
        sock.sendto(reply.to_wire(), self.query.asker[1])

    def url_from_data(self, js_ans):
        base = "1 1 " if self.query.rdtype == rdatatype.URI else " "
        if "codec" in js_ans and js_ans["codec"] in KNOWN_CODECS:
            return base + KNOWN_CODECS[js_ans["codec"]].format(value=js_ans["value"])
        return base + LIMO_URL.format(eth_name=self.query.eth_name)

    def answer(self):
        if self.query is None:
            return
        qry = self.query

        if qry.rdtype == rdatatype.URI or qry.rdtype == rdatatype.TXT:
            url = f"{ETH_LINK_BASE_URL}content/{qry.eth_name}"
        else:
            url = f"{ETH_LINK_BASE_URL}dns/{qry.eth_name}/{qry.qtype}"

        log(f"ASKING: {url}")
        try:
            answer = self.eth_link.get(url)
        except requests.exceptions.RequestException:
            return

        log(f"ANSWER >>>>> {len(answer.content)} : {answer.content}")

        reply = message.make_response(qry.line_query)
        reply.index = None
        reply.flags = 0x8400
        reply.id = qry.asker[0]
        self.add_soa(reply)

        if len(answer.content) == 0:
            if qry.rdtype == rdatatype.A:
                for ip in uwr_ips:
                    reply.answer.append(
                        rrset.from_text(qry.qname + ".", DEFAULT_TTL, "IN", "A",
                                        ip))
            if qry.rdtype == rdatatype.URI or qry.rdtype == rdatatype.TXT:
                reply.answer.append(rrset.from_text(f"{qry.qname}.", DEFAULT_TTL, "IN", qry.qtype, self.url_from_data({})))
            sock.sendto(reply.to_wire(), qry.asker[1])
            return

        try:
            js_ans = json.loads(answer.content)
        except json.JSONDecodeError:
            return

        if qry.rdtype == rdatatype.URI or qry.rdtype == rdatatype.TXT:
            reply.answer.append(rrset.from_text(f"{qry.qname}.", DEFAULT_TTL, "IN", qry.qtype, self.url_from_data(js_ans)))
        else:
            reply.answer.append(rrset.from_text(js_ans["name"]+".",js_ans["ttl"],js_ans["class"],js_ans["type"],js_ans["data"]))

        sock.sendto(reply.to_wire(), qry.asker[1])

    def clean_up(self):
        global queries
        if self.query.key in queries:
            del queries[self.query.key]
        self.query = None


class Query:

    def __init__(self, qry, sender):
        self.line_query = qry
        self.qname = str(qry.question[0].name)[:-1]
        self.eth_name = self.qname[11:] if self.qname[:11] == "_http._tcp." else self.qname
        self.rdtype = qry.question[0].rdtype
        self.qtype = rdatatype.to_text(self.rdtype)
        self.key = f"{self.rdtype}|{self.qname}"
        self.asker = (qry.id, sender)
        self.tld = self.eth_name
        if (pos := self.eth_name.find(".")) >= 0:
            self.tld = self.eth_name[pos+1:]


def find_next():
    for r in resolvers:
        if not r.thread.is_alive():
            r.restart()
            r.query = None

        if r.query is None:
            return r.idx

    return None


parser = argparse.ArgumentParser(description='EPP Jobs Runner')
parser.add_argument("-t", '--threads', default=2)
parser.add_argument("-s", '--syslog', action="store_true", default=False)
parser.add_argument("-l", '--logging', action="store_true")
parser.add_argument("-u", '--uwr', required=True)
args = parser.parse_args()

uwr_ips = args.uwr.split(",")

to_syslog = args.syslog
with_logging = args.logging

num_threads = int(os.environ["ETH_RESOLV_THREADS"]
                  ) if "ETH_RESOLV_THREADS" in os.environ else args.threads

resolvers = [Resolver(idx) for idx in range(0, int(args.threads))]
for r in resolvers:
    r.run()

while True:
    bin_qry, sender = sock.recvfrom(DNS_MAX_QUERY)
    log(f">>>>  PACKET: {sender}")
    if bin_qry[2] & 0x80:
        continue

    new_query = Query(message.from_wire(bin_qry), sender)

    if new_query.key in queries:
        resolv = resolvers[queries[new_query.key]]
        log(f">>>> ALREADY: {new_query.asker} to {resolv.idx}")
        resolv.query.asker = new_query.asker
    else:
        if (idx := find_next()) is not None:
            log(f">>>>  GIVING: {new_query.asker} to {idx}")
            queries[new_query.key] = idx
            resolvers[idx].query = new_query

sock.close()
