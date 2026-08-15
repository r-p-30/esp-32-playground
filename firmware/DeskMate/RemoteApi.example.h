#pragma once

// Copy this file to RemoteApi.h (same folder) and fill in real values.
// RemoteApi.h is gitignored - it never gets committed.
// See docs/remote-api-spec.md for the exact JSON contract your hosted
// site needs to serve at this URL.

// Full URL of your hosted state endpoint, e.g.:
// https://your-site.example/api/state
#define REMOTE_API_URL   "https://your-site.example/api/state"

// Shared secret sent as the X-Device-Key header on every request - keeps
// a stranger who finds the URL from remotely buzzing the device or
// changing what it shows. Make this up yourself; your site should check
// for this exact value and reject anything else.
#define REMOTE_API_KEY   "change-me"

// Optional - where the device POSTs a small "I'm alive" status ping each
// poll cycle. Leave as the placeholder below to skip heartbeats entirely.
#define REMOTE_HEARTBEAT_URL   "https://your-site.example/api/heartbeat"
