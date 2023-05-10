#! /usr/bin/python3
# (c) Copyright 2022-2023, James Stevens ... see LICENSE for details
# Alternative license arrangements possible, contact me for more information
""" module to resolve ETH domain via https://eth.link/ """

import json
import socket
import select
import requests
import time
import threading
from dns import message, rrset, rdatatype, flags

AUTH_IP = "127.1.0.1"

ETH_LINK_BASE_URL = "https://eth.link/dns-query"
DNS_MAX_RESP = 4096

DEFAULT_TTL = 3600
TIMEOUT = 5

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.bind((AUTH_IP, 53))

eth_link = requests.Session()
eth_link.headers.update({"content-type": "application/dns-json"})

recent = {}
qrys = {}


def del_old_qrys():
    now = int(time.time())
    for x in list(recent):
        if recent[x] < now:
            del recent[x]


def reply_to_qry(qry):

    qname = str(qry.question[0].name)[:-1]
    qtype = rdatatype.to_text(qry.question[0].rdtype)

    this_qry = qname + "|" + qtype

    params = {"name": qname, "type": qtype}
    answer = eth_link.get(ETH_LINK_BASE_URL, params=params)

    reply = message.make_response(qry)
    reply.index = None
    reply.flags = 0x8400
    reply.id = qrys[this_qry][0]
    reply.authority.append(
        rrset.from_text("eth.", DEFAULT_TTL, "IN", "SOA",
                        "eth. eth. 1 10800 3600 604800 3600"))

    try:
        js_ans = json.loads(answer.content)
        for rr in js_ans["Answer"]:
            ttl = rr["ttl"] if "ttl" in rr else DEFAULT_TTL
            ttl = rr["TTL"] if "TTL" in rr else ttl
            reply.answer.append(
                rrset.from_text(rr["name"] + ".", ttl, "IN", rr["type"],
                                rr["data"]))
    except json.JSONDecodeError:
        pass

    sender = qrys[this_qry][1]
    sock.sendto(reply.to_wire(), sender)
    del qrys[this_qry]


def reply_error(qry, sender):
    reply = message.make_response(qry)
    reply.flags = qry.flags | 0x8000
    reply.set_rcode(5)
    sock.sendto(reply.to_wire(), sender)


def main_loop():
    bin_qry, sender = sock.recvfrom(DNS_MAX_RESP)
    if bin_qry[2] & 0x80:
        return

    now = int(time.time())
    qry = message.from_wire(bin_qry)

    qname = str(qry.question[0].name)[:-1]
    if qname[-4:] != ".eth" and qname != "eth":
        return reply_error(qry, sender)

    qtype = rdatatype.to_text(qry.question[0].rdtype)

    this_qry = qname + "|" + qtype
    qrys[this_qry] = (qry.id,sender)

    if this_qry in recent and recent[this_qry] > now:
        return

    recent[this_qry] = now + TIMEOUT

    x = threading.Thread(target=reply_to_qry, args=(
        qry,
    ))
    x.start()

    del_old_qrys()


while True:
    main_loop()
sock.close()
