#! /usr/bin/node

import express from 'express';
import fs from "fs";

import { Id } from '@imperviousinc/id';
import { AlchemyProvider, Contract, parseEther } from 'ethers';
import contentHash from "content-hash";

const provider = new AlchemyProvider(null, process.env.ALCHEMY_API_CODE);
const network = await provider.getNetwork() ;

const id = new Id({network, provider});

var app = express();


app.get('/dns/:domain/:type', function (req, res) {
	id.getDns(req.params.domain,req.params.type).then((result) => {
		if (result) res.end(JSON.stringify(result)); else res.end(null);
		}).catch((err) => { res.end(null); });
	});



app.get('/content/:domain', function (req, res) {
		id.getContentHash(req.params.domain).then((result) => {
			if ((result)&&(result.hexBytes)&&(result.hexBytes!="0x")) {
				res.end(JSON.stringify(
					{
					"codec":contentHash.getCodec(result.hexBytes),
					"value":contentHash.decode(result.hexBytes)
					}));
				}
			else res.end(null);
			}).catch((err) => { res.end(null); });
	});


var server = app.listen(8081, function () {
	var host = server.address().address
	var port = server.address().port
});
