chrome.app.runtime.onLaunched.addListener(function() {
  chrome.app.window.create('mosh_client.html', {
    'bounds': {
      'width': 400,
      'height': 500
    }
  });
});
