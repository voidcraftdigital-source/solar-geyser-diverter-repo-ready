import { verifyUserToken } from "../auth.js";

// Gates the user-facing API — Specification §13: "Remote control requires
// an authenticated user session ... no anonymous or public control endpoint."
export function requireUser(req, res, next) {
  const header = req.get("authorization") || "";
  const [scheme, token] = header.split(" ");
  if (scheme !== "Bearer" || !token) {
    return res.status(401).json({ error: "missing or malformed Authorization header" });
  }
  const userId = verifyUserToken(token);
  if (!userId) {
    return res.status(401).json({ error: "invalid or expired token" });
  }
  req.userId = userId;
  next();
}
