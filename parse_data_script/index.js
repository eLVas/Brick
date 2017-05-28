'use strict'

const fs = require('fs');
const pth = require('path');
const sh = require('shelljs');
const _ = require('lodash');
const moment = require('moment');
const csvWriter = require('csv-write-stream');
const commandLineArgs = require('command-line-args');

const now = moment();

function syncData(path='./data/', bucket='brick-iot-home-data') {
 const res = sh.exec(`boto-rsync s3://${bucket} ${path}`/*, {silent:true}*/)
 if (res.stderr && res.stderr.length > 0) {
   console.log(res.stderr);
 }
}

function loadData(path) {
 return fs.readdirSync(path).map(item => {
  return require(pth.join(__dirname, path, item))
 });
}

function parseData(data) {
 return data.map(r => ({
   timestamp: moment.unix(r.timestamp).format(),
   light: r.state.reported.light,
   temp: r.state.reported.temp,
   humidity: r.state.reported.hum
  }));
}

function writeToCSV(dataFlat, file='./data.csv') {
 const writer = csvWriter();
 writer.pipe(fs.createWriteStream(file));
 for (let row of dataFlat) {
   writer.write(row);
 }
 writer.end();
}

function main() {
  const path = '../data';

  syncData(path)
  const rawData = loadData(path);
  const flatData = parseData(rawData);
  writeToCSV(flatData, '../data.csv');
}

main();
