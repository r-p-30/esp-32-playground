// Polls /dashboard/status and patches the connection line + Night/Game
// mode toggles in place - without this, those only ever reflected
// whatever was true at the last full page load, so a physical-button
// change on the device (see forceHeartbeatNow() in RemoteControl.cpp)
// wouldn't show up until someone manually refreshed the tab.
(function () {
  var POLL_MS = 4000;

  function setConnection(connected) {
    var el = document.getElementById('conn-indicator');
    if (!el) return;
    el.textContent = '● ' + (connected ? 'Connected' : 'Not connected');
    el.className = connected ? 'conn-on' : 'conn-off';
  }

  function setHint(data) {
    var el = document.getElementById('heartbeat-hint');
    if (!el) return;
    var hb = data.heartbeat;
    if (!hb) {
      el.textContent = "No heartbeat yet - waiting on the device's first one after connecting.";
      return;
    }
    var text = 'Last seen ' + data.lastSeenSecAgo + 's ago · card ' + hb.currentCard;
    if (hb.nightMode) text += ' · night';
    if (hb.gameMode) text += ' · game';
    text += ' · up ' + hb.uptimeSec + 's';
    el.textContent = text;
  }

  // Skips the currently-focused checkbox - avoids the poll stomping on a
  // click that's mid-flight (the click's own form submit is what should
  // win, not a stale poll response landing right after).
  function setToggle(id, value) {
    var el = document.getElementById(id);
    if (!el || document.activeElement === el) return;
    el.checked = !!value;
  }

  // Highlights whichever game button matches the device's own reported
  // truth (heartbeat.activeGame while heartbeat.gameMode is true) - same
  // "current truth, not last-sent value" reasoning as setToggle() above,
  // but for the game picker's .active class instead of a checkbox.
  function setActiveGame(hb) {
    var activeIndex = hb && hb.gameMode ? hb.activeGame : -1;
    var buttons = document.querySelectorAll('.game-picker-btn[data-game-index]');
    for (var i = 0; i < buttons.length; i++) {
      var btn = buttons[i];
      var isActive = Number(btn.getAttribute('data-game-index')) === activeIndex;
      btn.classList.toggle('active', isActive);
    }
  }

  function poll() {
    fetch('/dashboard/status', { headers: { Accept: 'application/json' } })
      .then(function (res) { return res.ok ? res.json() : null; })
      .then(function (data) {
        if (!data) return;
        setConnection(data.connected);
        setHint(data);
        setToggle('night-mode-checkbox', data.nightModeEnabled);
        setToggle('game-mode-checkbox', data.gameModeEnabled);
        setActiveGame(data.heartbeat);
      })
      .catch(function () {});
  }

  setInterval(poll, POLL_MS);
})();
