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

MoshClientModule = null;  // Global application object.
statusText = 'NO-STATUS';

// Indicate load success.
function moduleDidLoad() {
  MoshClientModule = document.getElementById('mosh_client');
  updateStatus('SUCCESS');
  // Send a message to the NaCl module.
  MoshClientModule.postMessage('hello');
}

// The 'message' event handler.  This handler is fired when the NaCl module
// posts a message to the browser by calling PPB_Messaging.PostMessage()
// (in C) or pp::Instance.PostMessage() (in C++).  This implementation
// simply displays the content of the message in an alert panel.
function handleMessage(message_event) {
  //alert(message_event.data);
  updateStatus(message_event.data);
}

// If the page loads before the Native Client module loads, then set the
// status message indicating that the module is still loading.  Otherwise,
// do not change the status message.
onload = function() {
  var listener = document.getElementById('listener');
  listener.addEventListener('load', moduleDidLoad, true);
  listener.addEventListener('message', handleMessage, true);

  if (MoshClientModule == null) {
    updateStatus('LOADING...');
  } else {
    // It's possible that the Native Client module onload event fired
    // before the page's onload event.  In this case, the status message
    // will reflect 'SUCCESS', but won't be displayed.  This call will
    // display the current message.
    updateStatus();
  }
}

// Set the global status message.  If the element with id 'statusField'
// exists, then set its HTML to the status message as well.
// opt_message The message test.  If this is null or undefined, then
// attempt to set the element with id 'statusField' to the value of
// |statusText|.
function updateStatus(opt_message) {
  if (opt_message)
    statusText = opt_message;
  var statusField = document.getElementById('status_field');
  if (statusField) {
    statusField.innerHTML = statusText;
  }
}

/*
var socketId = -1;

var handleDataEvent = function(r) {
  console.log(r);
  chrome.socket.read(socketId, 1500, handleDataEvent);
}

onload = function() {
  var buf = new ArrayBuffer(2);
  var bufView = new Uint8Array(buf);
  bufView[0] = 52;
  bufView[1] = 50;
  chrome.socket.create('udp', {}, function(socketInfo) {
    socketId = socketInfo.socketId;
    chrome.socket.connect(socketId, '127.0.0.1', 1337, function(result) {
      chrome.socket.read(socketId, 1500, handleDataEvent);
      button = document.getElementById('my-button');
      button.onclick = function(e) {
        chrome.socket.write(socketId, buf, function(sendInfo) {
          button.textContent = "wrote " + sendInfo.bytesWritten;
        });
      };
    });
  });

};
*/
