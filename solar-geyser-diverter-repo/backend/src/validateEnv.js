import "dotenv/config";

// Runs before anything else in server.js (must stay the first import there)
// so a misconfigured deployment fails loudly at startup instead of quietly
// running with a weak or placeholder secret.
function requireStrongSecret(name) {
  const value = process.env[name];
  const placeholder = "change-me-to-a-long-random-string";
  if (!value || value === placeholder || value.length < 32) {
    console.error(`[server] ${name} is missing, too short (need 32+ chars), or still the .env.example placeholder.`);
    console.error(
      '[server] Generate one with: node -e "console.log(require(\'crypto\').randomBytes(48).toString(\'hex\'))"'
    );
    process.exit(1);
  }
}

requireStrongSecret("JWT_SECRET");
requireStrongSecret("MANUFACTURING_KEY");

if (process.env.ALLOW_DEV_PROVISIONING === "true") {
  console.warn("[server] ALLOW_DEV_PROVISIONING=true -- POST /api/dev/provision-device is ENABLED.");
  console.warn("[server] This bypasses the manufacturing key entirely. It must never be true in a real deployment.");
}

if (!process.env.ALLOWED_ORIGINS) {
  console.warn("[server] ALLOWED_ORIGINS not set -- allowing requests from any origin (*).");
  console.warn("[server] Set it to your real frontend origin(s) before deploying publicly.");
}
