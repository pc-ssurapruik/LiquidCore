'use strict';
const common = require('../common');
const assert = require('assert');
const dgram = require('dgram');

var socket = dgram.createSocket('udp4');

socket.on('listening', common.mustCall(() => {
  assert.throws(() => {
    socket.bind();
  }, /^Error: Socket is already bound$/);

  socket.close();
}));

var result = socket.bind(); // should not throw

assert.strictEqual(result, socket); // should have returned itself
