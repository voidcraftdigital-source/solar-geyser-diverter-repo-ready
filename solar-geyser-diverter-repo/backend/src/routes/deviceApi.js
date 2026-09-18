import { Router } from "express";
import { db } from "../db.js";
import { requireDevice } from "../middleware/requireDevice.js";

export const deviceApiRouter = Router();
deviceApiRouter.use(requireDevice);

// Combined push+pull in one round trip: the device posts its current
// telemetry (the same fields already exposed by the firmware's own
// /api/status — see ../README.md) and gets back any commands still waiting
// for it, which it should apply and then ack. One request instead of two
// matters on a device paying for its own radio time.
deviceApiRouter.post("/telemetry", (req, res) => {
  const payload = req.body || {};
  db.prepare("INSERT INTO telemetry (device_id, payload) VALUES (?, ?)").run(
    req.device.device_id,
    JSON.stringify(payload)
  );

  const pending = db
    .prepare(
      "SELECT id, command_type, payload FROM commands WHERE device_id = ? AND status = 'pending' ORDER BY created_at ASC"
    )
    .all(req.device.device_id);

  res.status(201).json({
    ok: true,
    commands: pending.map((c) => ({
      command_id: c.id,
      command_type: c.command_type,
      payload: c.payload ? JSON.parse(c.payload) : {},
    })),
  });
});

deviceApiRouter.post("/commands/:commandId/ack", (req, res) => {
  const info = db
    .prepare(
      "UPDATE commands SET status = 'delivered', delivered_at = datetime('now') WHERE id = ? AND device_id = ? AND status = 'pending'"
    )
    .run(req.params.commandId, req.device.device_id);
  if (info.changes === 0) {
    return res.status(404).json({ error: "no such pending command for this device" });
  }
  res.json({ ok: true });
});
