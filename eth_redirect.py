#! /usr/bin/python3
# (c) Copyright 2022-2023, James Stevens ... see LICENSE for details
# Alternative license arrangements possible, contact me for more information
""" module to resolve ETH domain via https://eth.link/ """

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

ETH_LINK_BASE_URL = "https://eth.link/dns-query"
DNS_MAX_QUERY = 1024
DEFAULT_TTL = 3600
MAX_THREADS = 20

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


def add_soa(reply):
    reply.authority.append(
        rrset.from_text("eth.", DEFAULT_TTL, "IN", "SOA",
                        "eth. eth. 1 10800 3600 604800 3600"))


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

    def run(self):
        self.thread.start()

    def error(self, rcode):
        reply = message.make_response(self.query.line_query)
        reply.flags = self.query.line_query.flags | 0x8400
        reply.set_rcode(rcode)
        reply.id = self.query.asker[0]
        add_soa(reply)
        sock.sendto(reply.to_wire(), self.query.asker[1])

    def answer(self):
        if self.query is None:
            return
        qry = self.query

        if qry.qname[-4:] != ".eth" and qry.qname != "eth":
            return self.error(5)

        params = {"name": qry.qname, "type": qry.qtype}
        try:
            answer = self.eth_link.get(ETH_LINK_BASE_URL, params=params)
        except requests.exceptions.RequestException:
            return

        reply = message.make_response(qry.line_query)
        reply.index = None
        reply.flags = 0x8400
        reply.id = qry.asker[0]
        add_soa(reply)

        try:
            js_ans = json.loads(answer.content)
        except json.JSONDecodeError:
            return

        if "Status" in js_ans and js_ans["Status"] != 0:
            return self.error(js_ans["Status"])

        for rr in js_ans["Answer"]:
            ttl = rr["ttl"] if "ttl" in rr else DEFAULT_TTL
            ttl = rr["TTL"] if "TTL" in rr else ttl
            reply.answer.append(
                rrset.from_text(rr["name"] + ".", ttl, "IN", rr["type"],
                                rr["data"]))

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
        self.rdtype = qry.question[0].rdtype
        self.qtype = rdatatype.to_text(self.rdtype)
        self.key = f"{self.rdtype}|{self.qname}"
        self.asker = (qry.id, sender)


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
args = parser.parse_args()

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
