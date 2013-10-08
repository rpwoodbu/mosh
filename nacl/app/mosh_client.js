// Copyright 2013 Richard Woodbury
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

'use strict';

window.onload = function() {
  var connectButton = document.querySelector('#connect');
  connectButton.onclick = onConnectClick;
};

function execMosh() {
  var args = {}
  args.addr = document.querySelector('#addr').value;
  args.port = document.querySelector('#port').value;
  args.key = document.querySelector('#key').value;

  var form = document.querySelector('#args');
  form.parentNode.removeChild(form);

  var terminal = new hterm.Terminal("mosh");
  terminal.decorate(document.querySelector('#terminal'));
  terminal.onTerminalReady = function() {
    terminal.setCursorPosition(0, 0);
    terminal.setCursorVisible(true);
    terminal.runCommandClass(mosh.CommandInstance, args);
  };

  // Useful for console debugging.
  window.term_ = terminal;
};

function onConnectClick(e) {
  lib.init(execMosh, console.log.bind(console));
};

var mosh = {};

mosh.CommandInstance = function(argv) {
  // Command arguments.
  this.argv_ = argv;

  // Command environment.
  this.environment_ = argv.environment || {};

  // hterm.Terminal.IO instance.
  this.io = null;
};

mosh.CommandInstance.run = function(argv) {
  return new nassh.CommandInstance(argv);
};

mosh.CommandInstance.prototype.run = function() {
  // Useful for console debugging.
  window.mosh_client_ = this;

  this.io = this.argv_.io.push();
  this.io.onVTKeystroke = this.sendString_.bind(this);
  this.io.sendString = this.sendString_.bind(this);
  this.io.onTerminalResize = this.onTerminalResize_.bind(this);

  this.moshNaCl_ = window.document.createElement('embed');
  this.moshNaCl_.style.cssText = (
      'position: absolute;' +
      'top: -99px' +
      'width: 0;' +
      'height: 0;');
  this.moshNaCl_.setAttribute('src', 'mosh_client.nmf');
  this.moshNaCl_.setAttribute('type', 'application/x-nacl');
  this.moshNaCl_.setAttribute('key', this.argv_.argString.key);
  this.moshNaCl_.setAttribute('addr', this.argv_.argString.addr);
  this.moshNaCl_.setAttribute('port', this.argv_.argString.port);
  this.moshNaCl_.addEventListener('load', function(e) {
    console.log('Mosh NaCl loaded.');
  });
  this.moshNaCl_.addEventListener('message', this.onMessage_.bind(this));
  this.moshNaCl_.addEventListener('crash', function(e) {
    console.log('Mosh NaCl crashed.');
    self.exit(-1);
  });

  document.body.insertBefore(this.moshNaCl_, document.body.firstChild);
};

mosh.CommandInstance.prototype.onMessage_ = function(e) {
  this.io.print(e.data);
};

mosh.CommandInstance.prototype.sendString_ = function(string) {
  this.moshNaCl_.postMessage(string);
};

mosh.CommandInstance.prototype.onTerminalResize_ = function(w, h) {
  // Send new size as an int, with the width as the high 16 bits.
  this.moshNaCl_.postMessage((w << 16) + h);
};
