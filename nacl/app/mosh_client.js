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

var MoshClientModule = null;  // Global application object.
var statusText = 'NO-STATUS';

// Indicate load success.
function moduleDidLoad() {
  updateStatus('SUCCESS');
}

// The 'message' event handler.  This handler is fired when the NaCl module
// posts a message to the browser by calling PPB_Messaging.PostMessage()
// (in C) or pp::Instance.PostMessage() (in C++).  This implementation
// simply displays the content of the message in an alert panel.
function handleMessage(message_event) {
  updateStatus(message_event.data);
}

// If the page loads before the Native Client module loads, then set the
// status message indicating that the module is still loading.  Otherwise,
// do not change the status message.
window.onload = function() {
  var listener = document.getElementById('listener');
  listener.addEventListener('load', moduleDidLoad, true);

  if (MoshClientModule == null) {
    updateStatus('LOADING...');
  } else {
    // It's possible that the Native Client module onload event fired
    // before the page's onload event.  In this case, the status message
    // will reflect 'SUCCESS', but won't be displayed.  This call will
    // display the current message.
    updateStatus();
  }

  function execMosh() {
    var terminal = new hterm.Terminal("mosh");
    terminal.decorate(document.querySelector('#terminal'));
    terminal.onTerminalReady = function() {
      updateStatus('Terminal ready');
      terminal.setCursorPosition(0, 0);
      terminal.setCursorVisible(true);
      terminal.runCommandClass(mosh.CommandInstance, '');
    };

    // Useful for console debugging.
    window.term_ = terminal;
  };

  lib.init(execMosh, console.log.bind(console));
}

// Set the global status message.  If the element with id 'statusField'
// exists, then set its HTML to the status message as well.
// opt_message The message test.  If this is null or undefined, then
// attempt to set the element with id 'statusField' to the value of
// |statusText|.
function updateStatus(opt_message) {
  if (opt_message)
    statusText = opt_message;
}

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

  this.moshNaCl_ = document.querySelector("#mosh_client");
  this.moshNaCl_.addEventListener('message', this.onMessage_.bind(this));

  this.io = this.argv_.io.push();

  this.io.println("It works!");
  this.io.onVTKeystroke = this.onKeystroke_.bind(this);
};

mosh.CommandInstance.prototype.onMessage_ = function(e) {
  this.io.print(e.data);
};

mosh.CommandInstance.prototype.onKeystroke_ = function (string) {
  this.moshNaCl_.postMessage(string);
}
