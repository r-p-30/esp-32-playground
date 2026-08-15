#pragma once

// Copy this file to RemoteApi.h (same folder) and fill in real values.
// RemoteApi.h is gitignored - it never gets committed.
// See docs/remote-api-spec.md for the exact JSON contract your hosted
// site needs to serve at this URL.

// WebSocket URL of your hosted site's device endpoint (wss://, not
// https://) - the device holds this connection open for its entire
// runtime and the site pushes state updates the instant they happen, e.g.:
// wss://your-site.example/ws/device
#define REMOTE_API_URL   "wss://your-site.example/ws/device"

// Shared secret sent as the X-Device-Key header on the WebSocket handshake
// - keeps a stranger who finds the URL from remotely buzzing the device or
// changing what it shows. Make this up yourself; your site should check
// for this exact value and reject anything else.
#define REMOTE_API_KEY   "change-me"
