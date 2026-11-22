// server.js - Dual-Portal MUN Management Server with Role-Based Authentication
// Faces:
//   1. Delegate Portal   (/delegate-portal.html) -> CV upload, eligibility-filtered committees, allocation & delete requests
//   2. Programmer Portal (/dashboard.html, /delegates.html, /committees.html) -> full CRUD, eligibility rules, request approval
const express = require("express");
const multer = require("multer");
const mysql = require("mysql2/promise");
const crypto = require("crypto");
const path = require("path");
const fs = require("fs");
const os = require("os");
const { execFile } = require("child_process");

const app = express();
app.use(express.json());

// ---------------------------------------------------------------
// Database Configuration (shared with C++ Committee & Delegate CLI)
// ---------------------------------------------------------------
const pool = mysql.createPool({
  host: process.env.DB_HOST || "127.0.0.1",
  port: Number(process.env.DB_PORT || 3306),
  user: process.env.DB_USER || "root",
  password: process.env.DB_PASSWORD || "admin",
  database: process.env.DB_NAME || "mun_management",
  connectionLimit: 10,
});

// ---------------------------------------------------------------
// Auth Helpers (Node built-in crypto: scrypt + signed HttpOnly cookie)
// ---------------------------------------------------------------
const SESSION_SECRET = process.env.SESSION_SECRET || "mun-portal-secret-key-change-in-prod";
const COOKIE_NAME = "mun_session";

function hashPassword(password) {
  const salt = crypto.randomBytes(16).toString("hex");
  const key = crypto.scryptSync(String(password), salt, 64).toString("hex");
  return `${salt}:${key}`;
}

function verifyPassword(password, stored) {
  if (!stored || !stored.includes(":")) return false;
  const [salt, key] = stored.split(":");
  const derived = crypto.scryptSync(String(password), salt, 64);
  const keyBuf = Buffer.from(key, "hex");
  if (derived.length !== keyBuf.length) return false;
  return crypto.timingSafeEqual(derived, keyBuf);
}

function signToken(payload) {
  const body = Buffer.from(JSON.stringify(payload)).toString("base64url");
  const sig = crypto.createHmac("sha256", SESSION_SECRET).update(body).digest("base64url");
  return `${body}.${sig}`;
}

function verifyToken(token) {
  if (!token || !token.includes(".")) return null;
  const [body, sig] = token.split(".");
  const expected = crypto.createHmac("sha256", SESSION_SECRET).update(body).digest("base64url");
  if (sig.length !== expected.length) return null;
  if (!crypto.timingSafeEqual(Buffer.from(sig), Buffer.from(expected))) return null;
  try {
    const data = JSON.parse(Buffer.from(body, "base64url").toString("utf8"));
    if (data.exp && Date.now() > data.exp) return null;
    return data;
  } catch {
    return null;
  }
}

function parseCookies(req) {
  const out = {};
  const raw = req.headers.cookie || "";
  for (const part of raw.split(";")) {
    const idx = part.indexOf("=");
    if (idx > 0) {
      const k = part.slice(0, idx).trim();
      const v = decodeURIComponent(part.slice(idx + 1).trim());
      out[k] = v;
    }
  }
  return out;
}

function setSessionCookie(res, user) {
  const token = signToken({
    user_id: user.user_id,
    username: user.username,
    role: user.role,
    full_name: user.full_name,
    delegate_id: user.delegate_id || null,
    exp: Date.now() + 7 * 24 * 60 * 60 * 1000, // 7 days
  });
  res.setHeader("Set-Cookie", `${COOKIE_NAME}=${encodeURIComponent(token)}; HttpOnly; Path=/; SameSite=Lax; Max-Age=604800`);
}

function clearSessionCookie(res) {
  res.setHeader("Set-Cookie", `${COOKIE_NAME}=; HttpOnly; Path=/; SameSite=Lax; Max-Age=0`);
}

function getSession(req) {
  const cookies = parseCookies(req);
  return verifyToken(cookies[COOKIE_NAME]);
}

function requireAuth(req, res, next) {
  const s = getSession(req);
  if (!s) return res.status(401).json({ error: "Please log in to continue." });
  req.user = s;
  next();
}

function requireAdmin(req, res, next) {
  const s = getSession(req);
  if (!s) return res.status(401).json({ error: "Please log in to continue." });
  if (s.role !== "admin") return res.status(403).json({ error: "Programmer/Admin access required." });
  req.user = s;
  next();
}

function requireDelegate(req, res, next) {
  const s = getSession(req);
  if (!s) return res.status(401).json({ error: "Please log in to continue." });
  if (s.role !== "delegate") return res.status(403).json({ error: "Delegate account required." });
  req.user = s;
  next();
}

// ---------------------------------------------------------------
// Auto-initialize Auth / Request Tables & min_muns column on startup
// ---------------------------------------------------------------
async function ensureSchema() {
  try {
    // Ensure min_muns column exists on committees (safe for MySQL 8.0)
    const [cols] = await pool.query("SHOW COLUMNS FROM committees LIKE 'min_muns'");
    if (!cols.length) {
      await pool.query("ALTER TABLE committees ADD COLUMN min_muns INT NOT NULL DEFAULT 0");
      console.log("Added min_muns eligibility column to committees table.");
    }

    // Ensure cv_verified column exists on delegates (gates committee requests until CV upload)
    const [delCols] = await pool.query("SHOW COLUMNS FROM delegates LIKE 'cv_verified'");
    if (!delCols.length) {
      await pool.query("ALTER TABLE delegates ADD COLUMN cv_verified TINYINT(1) NOT NULL DEFAULT 0");
      console.log("Added cv_verified column to delegates table.");
    }

    await pool.query(`
      CREATE TABLE IF NOT EXISTS users (
        user_id INT NOT NULL AUTO_INCREMENT,
        username VARCHAR(60) NOT NULL,
        password_hash VARCHAR(255) NOT NULL,
        role ENUM('admin', 'delegate') NOT NULL DEFAULT 'delegate',
        full_name VARCHAR(100) NOT NULL,
        delegate_id INT NULL,
        created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
        PRIMARY KEY (user_id),
        UNIQUE KEY uq_users_username (username),
        CONSTRAINT fk_users_delegate FOREIGN KEY (delegate_id) REFERENCES delegates (delegate_id) ON DELETE SET NULL
      ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4
    `);

    await pool.query(`
      CREATE TABLE IF NOT EXISTS allocation_requests (
        request_id INT NOT NULL AUTO_INCREMENT,
        user_id INT NOT NULL,
        delegate_id INT NULL,
        delegate_name VARCHAR(100) NOT NULL,
        portfolio_id INT NOT NULL,
        allocation_id INT NULL,
        request_type ENUM('allocate', 'deallocate') NOT NULL,
        status ENUM('pending', 'approved', 'rejected') NOT NULL DEFAULT 'pending',
        note VARCHAR(255) DEFAULT '',
        created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
        reviewed_at TIMESTAMP NULL,
        PRIMARY KEY (request_id),
        CONSTRAINT fk_req_user FOREIGN KEY (user_id) REFERENCES users (user_id) ON DELETE CASCADE,
        CONSTRAINT fk_req_portfolio FOREIGN KEY (portfolio_id) REFERENCES portfolios (id) ON DELETE CASCADE
      ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4
    `);

    // Seed default Programmer account (admin / admin123) if absent
    const [admins] = await pool.query("SELECT user_id FROM users WHERE username = 'admin'");
    if (!admins.length) {
      await pool.query(
        "INSERT INTO users (username, password_hash, role, full_name, delegate_id) VALUES (?, ?, 'admin', ?, NULL)",
        ["admin", hashPassword("admin123"), "MUN Programmer"]
      );
      console.log("Seeded default Programmer account -> username: admin | password: admin123");
    }

    // Seed demo Delegate accounts linked to existing delegates if present
    const demoDelegates = [
      { username: "aniket", pass: "user123", delegateId: 1, fallbackName: "Aniket KAushal" },
      { username: "ayush", pass: "user123", delegateId: 2, fallbackName: "Ayush" },
    ];
    for (const d of demoDelegates) {
      const [exists] = await pool.query("SELECT user_id FROM users WHERE username = ?", [d.username]);
      if (!exists.length) {
        const [dRow] = await pool.query("SELECT delegate_id, name FROM delegates WHERE delegate_id = ?", [d.delegateId]);
        if (dRow.length) {
          await pool.query(
            "INSERT INTO users (username, password_hash, role, full_name, delegate_id) VALUES (?, ?, 'delegate', ?, ?)",
            [d.username, hashPassword(d.pass), dRow[0].name, dRow[0].delegate_id]
          );
        }
      }
    }
  } catch (e) {
    console.error("Schema init warning:", e.message);
  }
}

// ---------------------------------------------------------------
// Server-Side HTML Page Route Guards (Two Faces Enforcement)
// ---------------------------------------------------------------
app.get("/", (req, res) => {
  res.sendFile(path.join(__dirname, "public", "home.html"));
});

const ADMIN_PAGES = ["/dashboard.html", "/delegates.html", "/committees.html", "/index.html"];
app.get(ADMIN_PAGES, (req, res, next) => {
  const s = getSession(req);
  if (!s) return res.redirect("/login.html");
  if (s.role !== "admin") return res.redirect("/delegate-portal.html");
  next();
});

app.get("/delegate-portal.html", (req, res, next) => {
  const s = getSession(req);
  if (!s) return res.redirect("/login.html");
  if (s.role === "admin") return res.redirect("/dashboard.html");
  next();
});

app.use(express.static(path.join(__dirname, "public")));

// ---------------------------------------------------------------
// Shared Helpers & C++ Binary Runners
// ---------------------------------------------------------------
const ext = process.platform === "win32" ? ".exe" : "";
const PARSER = path.join(__dirname, "..", "backend", "cv_parser" + ext);
const ALLOCATOR = path.join(__dirname, "..", "backend", "allocator" + ext);

function run(cmd, args) {
  return new Promise((resolve, reject) => {
    execFile(cmd, args, { maxBuffer: 10 * 1024 * 1024 }, (err, stdout, stderr) => {
      if (err) return reject(new Error(stderr || err.message));
      resolve(stdout);
    });
  });
}

// JS fallback CV parser matching backend/cv_parser.cpp if binary is not compiled yet
function fallbackParseCvText(text) {
  const lines = String(text || "").split(/\r?\n/);
  const seen = new Set();
  const matched = [];
  let entries = 0;
  let claimed = 0;

  const yearRe = /\b(19|20)\d\d\b|'\d\d\b/i;
  const roleRe = /\b(delegate|chair|co-chair|vice[- ]chair|director|rapporteur|secretary[- ]general|deputy|executive board|eb member|participated|participant|attended|represented|award|best|special mention|verbal mention|high commendation|organi[sz]ed|organizing committee|usg|dias)\b/i;
  const claimRe = /(\d{1,3})\s*\+?\s*(?:[a-z]+\s+){0,2}(?:muns?|model united nations|model un)\b/gi;
  const labelRe = /^\s*(?:location|address|city|based in|current location|residence)\s*[:\-]\s*(.+)$/i;

  function mentionsMun(lower) {
    if (lower.includes("model united nations")) return true;
    const tokRe = /\b([a-z]*)mun(s)?\b/g;
    let m;
    while ((m = tokRe.exec(lower)) !== null) {
      if (["com", "im", "ra"].includes(m[1])) continue;
      return true;
    }
    return false;
  }

  let location = "";
  for (const raw of lines) {
    const line = raw.trim();
    if (!line) continue;
    if (!location) {
      const lm = line.match(labelRe);
      if (lm && lm[1].trim()) location = lm[1].trim();
    }
    const lower = line.toLowerCase();
    if (!mentionsMun(lower)) continue;

    let bestClaim = 0;
    let cm;
    claimRe.lastIndex = 0;
    while ((cm = claimRe.exec(lower)) !== null) {
      bestClaim = Math.max(bestClaim, parseInt(cm[1], 10));
    }
    if (bestClaim > 0) {
      claimed = Math.max(claimed, bestClaim);
      continue;
    }
    if (!yearRe.test(lower) && !roleRe.test(lower)) continue;
    const key = lower.replace(/[^a-z0-9]+/g, " ").trim();
    if (!seen.has(key)) {
      seen.add(key);
      entries++;
      matched.push(line);
    }
  }
  return {
    mun_count: Math.max(entries, claimed),
    entries_found: entries,
    claimed,
    location,
    matched,
  };
}

async function withTransaction(fn) {
  const conn = await pool.getConnection();
  try {
    await conn.beginTransaction();
    const out = await fn(conn);
    await conn.commit();
    return out;
  } catch (e) {
    await conn.rollback();
    throw e;
  } finally {
    conn.release();
  }
}

function dbError(res, e) {
  console.error("DB error:", e.message);
  if (e.code === "ER_DUP_ENTRY") {
    return res.status(409).json({ error: "A record with that value already exists." });
  }
  const hint =
    e.code === "ER_ACCESS_DENIED_ERROR" ? "Database login failed (check DB_PASSWORD)." :
    e.code === "ECONNREFUSED" ? "Cannot reach MySQL. Is MySQL running?" :
    "Database error: " + e.message;
  res.status(500).json({ error: hint });
}

function cleanInt(v) {
  const n = parseInt(v, 10);
  return Number.isFinite(n) && n >= 0 && n < 100000 ? n : null;
}

function cleanList(arr) {
  if (!Array.isArray(arr)) return [];
  return arr.map((s) => String(s ?? "").trim().slice(0, 150)).filter(Boolean).slice(0, 50);
}

// ===============================================================
// 0. PUBLIC LANDING PAGE STATS (/api/public/stats)
// ===============================================================
app.get("/api/public/stats", async (req, res) => {
  try {
    const [[d]] = await pool.query("SELECT COUNT(*) AS delegates FROM delegates");
    const [[c]] = await pool.query("SELECT COUNT(*) AS committees FROM committees");
    const [[p]] = await pool.query(`
      SELECT COUNT(*) AS portfolios,
             COALESCE(SUM(capacity), 0) AS total_seats,
             (SELECT COUNT(*) FROM allocations) AS filled_seats
      FROM portfolios
    `);
    const [comms] = await pool.query(`
      SELECT c.name, c.min_muns,
             GREATEST(0, COALESCE(SUM(p.capacity), 0) - COUNT(a.id)) AS seats_left
      FROM committees c
      LEFT JOIN portfolios p ON p.committee_id = c.id
      LEFT JOIN allocations a ON a.portfolio_id = p.id
      GROUP BY c.id, c.name, c.min_muns
      ORDER BY c.min_muns ASC, c.id ASC
      LIMIT 6
    `);
    res.json({
      delegates: Number(d.delegates || 0),
      committees: Number(c.committees || 0),
      portfolios: Number(p.portfolios || 0),
      seats_left: Math.max(0, Number(p.total_seats || 0) - Number(p.filled_seats || 0)),
      featured_committees: comms.map((row) => ({
        name: row.name,
        min_muns: Number(row.min_muns || 0),
        seats_left: Number(row.seats_left || 0),
      })),
    });
  } catch (e) {
    res.json({ delegates: 0, committees: 0, portfolios: 0, seats_left: 0, featured_committees: [] });
  }
});

// ===============================================================
// 1. AUTHENTICATION API (/api/auth/*)
// ===============================================================

app.get("/api/auth/me", async (req, res) => {
  const s = getSession(req);
  if (!s) return res.status(401).json({ authenticated: false });
  try {
    let delegate = null;
    if (s.role === "delegate" && s.delegate_id) {
      const [rows] = await pool.query(
        "SELECT delegate_id, name, age, country, mun_attend, cv_verified FROM delegates WHERE delegate_id = ?",
        [s.delegate_id]
      );
      if (rows.length) delegate = rows[0];
    }
    res.json({ authenticated: true, user: s, delegate });
  } catch (e) {
    dbError(res, e);
  }
});

app.post("/api/auth/login", async (req, res) => {
  const username = String(req.body?.username ?? "").trim().toLowerCase();
  const password = String(req.body?.password ?? "");
  if (!username || !password) {
    return res.status(400).json({ error: "Username and password are required." });
  }
  try {
    const [rows] = await pool.query("SELECT * FROM users WHERE LOWER(username) = ?", [username]);
    if (!rows.length || !verifyPassword(password, rows[0].password_hash)) {
      return res.status(401).json({ error: "Invalid username or password." });
    }
    const user = rows[0];
    setSessionCookie(res, user);
    res.json({
      ok: true,
      role: user.role,
      full_name: user.full_name,
      redirect: user.role === "admin" ? "/dashboard.html" : "/delegate-portal.html",
    });
  } catch (e) {
    dbError(res, e);
  }
});

app.post("/api/auth/register", async (req, res) => {
  const username = String(req.body?.username ?? "").trim().toLowerCase().slice(0, 60);
  const password = String(req.body?.password ?? "");
  const full_name = String(req.body?.full_name ?? "").trim().slice(0, 100);
  const country = String(req.body?.country ?? "").trim().slice(0, 100);
  const age = cleanInt(req.body?.age);

  if (!/^[a-z0-9_.-]{3,60}$/.test(username)) {
    return res.status(400).json({ error: "Username must be 3-60 characters (letters, numbers, _, ., -)." });
  }
  if (password.length < 4) return res.status(400).json({ error: "Password must be at least 4 characters." });
  if (!full_name) return res.status(400).json({ error: "Full name is required." });
  if (age === null || age < 10 || age > 100) return res.status(400).json({ error: "Enter a valid age (10-100)." });
  if (!country) return res.status(400).json({ error: "Country is required." });

  try {
    const user = await withTransaction(async (conn) => {
      const [dup] = await conn.query("SELECT 1 FROM users WHERE LOWER(username) = ?", [username]);
      if (dup.length) throw Object.assign(new Error("Username is already taken."), { status: 409 });

      const [delRes] = await conn.query(
        "INSERT INTO delegates (name, age, country, mun_attend, cv_verified) VALUES (?, ?, ?, 0, 0)",
        [full_name, age, country]
      );
      const delegateId = delRes.insertId;

      const [userRes] = await conn.query(
        "INSERT INTO users (username, password_hash, role, full_name, delegate_id) VALUES (?, ?, 'delegate', ?, ?)",
        [username, hashPassword(password), full_name, delegateId]
      );

      return {
        user_id: userRes.insertId,
        username,
        role: "delegate",
        full_name,
        delegate_id: delegateId,
      };
    });

    setSessionCookie(res, user);
    res.status(201).json({ ok: true, role: "delegate", redirect: "/delegate-portal.html" });
  } catch (e) {
    if (e.status === 409) return res.status(409).json({ error: e.message });
    dbError(res, e);
  }
});

app.post("/api/auth/logout", (req, res) => {
  clearSessionCookie(res);
  res.json({ ok: true, redirect: "/login.html" });
});

// ===============================================================
// 2. CV ANALYZER (Both Admin & Delegate; updates Delegate eligibility!)
// ===============================================================
const upload = multer({ dest: os.tmpdir(), limits: { fileSize: 5 * 1024 * 1024 } });

app.post("/api/analyze", requireAuth, upload.single("cv"), async (req, res) => {
  if (!req.file) return res.status(400).json({ error: "No file uploaded." });
  const tmpFiles = [req.file.path];
  try {
    let textPath = req.file.path;
    const isPdf = /\.pdf$/i.test(req.file.originalname) || req.file.mimetype === "application/pdf";
    if (isPdf) {
      const text = await run("pdftotext", ["-layout", req.file.path, "-"]);
      textPath = req.file.path + ".txt";
      fs.writeFileSync(textPath, text, "utf8");
      tmpFiles.push(textPath);
    }

    let result;
    if (fs.existsSync(PARSER)) {
      const out = await run(PARSER, [textPath, "--json"]);
      result = JSON.parse(out);
    } else {
      const rawText = fs.readFileSync(textPath, "utf8");
      result = fallbackParseCvText(rawText);
    }

    // If a delegate uploaded their CV, set their verified MUN score and unlock applications
    let updatedMunCount = null;
    let cvVerified = false;
    if (req.user.role === "delegate" && req.user.delegate_id) {
      const parsedCount = Number(result.mun_count || 0);
      await pool.query(
        "UPDATE delegates SET mun_attend = ?, cv_verified = 1 WHERE delegate_id = ?",
        [parsedCount, req.user.delegate_id]
      );
      updatedMunCount = parsedCount;
      cvVerified = true;
    }

    res.json({ ...result, updated_mun_attend: updatedMunCount, cv_verified: cvVerified });
  } catch (e) {
    res.status(500).json({ error: e.message.trim() });
  } finally {
    tmpFiles.forEach((f) => fs.unlink(f, () => {}));
  }
});

// ===============================================================
// 3. DELEGATE-FACE API (/api/delegate/*)
// ===============================================================

app.get("/api/delegate/overview", requireDelegate, async (req, res) => {
  try {
    // Load delegate profile
    let profile = { delegate_id: req.user.delegate_id, name: req.user.full_name, age: 0, country: "-", mun_attend: 0, cv_verified: 0 };
    if (req.user.delegate_id) {
      const [dRows] = await pool.query(
        "SELECT delegate_id, name, age, country, mun_attend, cv_verified FROM delegates WHERE delegate_id = ?",
        [req.user.delegate_id]
      );
      if (dRows.length) profile = dRows[0];
    }

    const cvVerified = Boolean(profile.cv_verified);
    profile.cv_verified = cvVerified;
    const userMuns = Number(profile.mun_attend || 0);
    const myNameLower = profile.name.toLowerCase();

    const [comms] = await pool.query("SELECT id, name, min_muns FROM committees ORDER BY min_muns ASC, id ASC");
    const [ports] = await pool.query("SELECT id, committee_id, name, capacity FROM portfolios ORDER BY id ASC");
    const [allocs] = await pool.query("SELECT id, portfolio_id, delegate_name FROM allocations ORDER BY id ASC");
    const [myReqs] = await pool.query(
      `SELECT r.request_id, r.portfolio_id, r.allocation_id, r.request_type, r.status, r.note, r.created_at,
              p.name AS portfolio_name, c.name AS committee_name
       FROM allocation_requests r
       JOIN portfolios p ON p.id = r.portfolio_id
       JOIN committees c ON c.id = p.committee_id
       WHERE r.user_id = ?
       ORDER BY r.request_id DESC`,
      [req.user.user_id]
    );

    const allocByPort = new Map();
    const myAllocations = [];
    const portLookup = new Map(ports.map((p) => [p.id, p]));
    const commLookup = new Map(comms.map((c) => [c.id, c]));

    for (const a of allocs) {
      if (!allocByPort.has(a.portfolio_id)) allocByPort.set(a.portfolio_id, []);
      allocByPort.get(a.portfolio_id).push(a);

      if (a.delegate_name.toLowerCase() === myNameLower) {
        const p = portLookup.get(a.portfolio_id);
        const c = p ? commLookup.get(p.committee_id) : null;
        const pendingDel = myReqs.some(
          (r) => r.request_type === "deallocate" && r.status === "pending" &&
                 (r.allocation_id === a.id || r.portfolio_id === a.portfolio_id)
        );
        myAllocations.push({
          allocation_id: a.id,
          portfolio_id: a.portfolio_id,
          committee_name: c ? c.name : "-",
          portfolio_name: p ? p.name : "-",
          delete_requested: pendingDel,
        });
      }
    }

    const committees = comms.map((c) => {
      const eligible = cvVerified && userMuns >= Number(c.min_muns || 0);
      const cPorts = ports
        .filter((p) => p.committee_id === c.id)
        .map((p) => {
          const list = allocByPort.get(p.id) || [];
          const filled = list.length;
          const available = Math.max(0, p.capacity - filled);
          const alreadyAllocated = list.some((a) => a.delegate_name.toLowerCase() === myNameLower);
          const pendingRequest = myReqs.some(
            (r) => r.portfolio_id === p.id && r.request_type === "allocate" && r.status === "pending"
          );
          return {
            id: p.id,
            name: p.name,
            capacity: p.capacity,
            filled,
            available,
            already_allocated: alreadyAllocated,
            pending_request: pendingRequest,
          };
        });

      return {
        id: c.id,
        name: c.name,
        min_muns: Number(c.min_muns || 0),
        eligible,
        seats_left: cPorts.reduce((s, p) => s + p.available, 0),
        portfolios: cPorts,
      };
    });

    res.json({
      profile,
      committees,
      my_allocations: myAllocations,
      my_requests: myReqs,
    });
  } catch (e) {
    dbError(res, e);
  }
});

// Delegate requests an allocation to an eligible portfolio
app.post("/api/requests/allocate", requireDelegate, async (req, res) => {
  const portfolio_id = Number(req.body?.portfolio_id);
  const note = String(req.body?.note ?? "").trim().slice(0, 255);
  if (!portfolio_id) return res.status(400).json({ error: "Portfolio is required." });

  try {
    const [[del]] = await pool.query(
      "SELECT delegate_id, name, mun_attend, cv_verified FROM delegates WHERE delegate_id = ?",
      [req.user.delegate_id]
    );
    if (!del || !del.cv_verified) {
      return res.status(403).json({
        error: "Please upload your CV first to verify your MUN score before applying for any committee.",
      });
    }
    const delegateName = del.name;
    const delegateMuns = Number(del.mun_attend || 0);

    const [[port]] = await pool.query(
      `SELECT p.id, p.name AS portfolio_name, p.capacity, c.name AS committee_name, c.min_muns
       FROM portfolios p JOIN committees c ON c.id = p.committee_id
       WHERE p.id = ?`,
      [portfolio_id]
    );
    if (!port) return res.status(404).json({ error: "Portfolio not found." });

    if (delegateMuns < Number(port.min_muns || 0)) {
      return res.status(403).json({
        error: `Not eligible for ${port.committee_name}. Requires ${port.min_muns}+ MUNs (your verified CV score: ${delegateMuns}).`,
      });
    }

    const [allocs] = await pool.query("SELECT delegate_name FROM allocations WHERE portfolio_id = ?", [portfolio_id]);
    if (allocs.length >= port.capacity) {
      return res.status(400).json({ error: "This portfolio is currently full." });
    }
    if (allocs.some((a) => a.delegate_name.toLowerCase() === delegateName.toLowerCase())) {
      return res.status(409).json({ error: "You are already allocated to this portfolio." });
    }

    const [dupReq] = await pool.query(
      "SELECT 1 FROM allocation_requests WHERE user_id = ? AND portfolio_id = ? AND request_type = 'allocate' AND status = 'pending'",
      [req.user.user_id, portfolio_id]
    );
    if (dupReq.length) {
      return res.status(409).json({ error: "You already have a pending allocation request for this portfolio." });
    }

    const [r] = await pool.query(
      `INSERT INTO allocation_requests (user_id, delegate_id, delegate_name, portfolio_id, request_type, status, note)
       VALUES (?, ?, ?, ?, 'allocate', 'pending', ?)`,
      [req.user.user_id, req.user.delegate_id, delegateName, portfolio_id, note]
    );
    res.status(201).json({ request_id: r.insertId, message: "Allocation request submitted for programmer approval." });
  } catch (e) {
    dbError(res, e);
  }
});

// Delegate requests deletion of one of their own allocations
app.post("/api/requests/deallocate", requireDelegate, async (req, res) => {
  const allocation_id = Number(req.body?.allocation_id);
  const note = String(req.body?.note ?? "").trim().slice(0, 255);
  if (!allocation_id) return res.status(400).json({ error: "Allocation ID is required." });

  try {
    const [[del]] = await pool.query("SELECT name FROM delegates WHERE delegate_id = ?", [req.user.delegate_id]);
    const myName = del ? del.name : req.user.full_name;

    const [[alloc]] = await pool.query("SELECT id, portfolio_id, delegate_name FROM allocations WHERE id = ?", [allocation_id]);
    if (!alloc) return res.status(404).json({ error: "Allocation not found." });
    if (alloc.delegate_name.toLowerCase() !== myName.toLowerCase()) {
      return res.status(403).json({ error: "You can only request deletion of your own allocation." });
    }

    const [dup] = await pool.query(
      "SELECT 1 FROM allocation_requests WHERE user_id = ? AND allocation_id = ? AND request_type = 'deallocate' AND status = 'pending'",
      [req.user.user_id, allocation_id]
    );
    if (dup.length) {
      return res.status(409).json({ error: "A delete request for this allocation is already pending." });
    }

    const [r] = await pool.query(
      `INSERT INTO allocation_requests (user_id, delegate_id, delegate_name, portfolio_id, allocation_id, request_type, status, note)
       VALUES (?, ?, ?, ?, ?, 'deallocate', 'pending', ?)`,
      [req.user.user_id, req.user.delegate_id, alloc.delegate_name, alloc.portfolio_id, alloc.id, note]
    );
    res.status(201).json({ request_id: r.insertId, message: "Delete allocation request sent to programmer." });
  } catch (e) {
    dbError(res, e);
  }
});

// ===============================================================
// 4. PROGRAMMER / ADMIN API (Protected by requireAdmin)
// ===============================================================

// Dashboard analytics
app.get("/api/dashboard", requireAdmin, async (req, res) => {
  try {
    const [[tot]] = await pool.query(`
      SELECT
        COUNT(delegate_id) AS delegates,
        COUNT(DISTINCT country) AS countries,
        ROUND(COALESCE(AVG(mun_attend), 0), 1) AS avg_muns,
        COALESCE(SUM(mun_attend), 0) AS total_muns,
        ROUND(COALESCE(AVG(age), 0), 1) AS avg_age
      FROM delegates
    `);
    const [[aw]] = await pool.query("SELECT COUNT(*) AS awards FROM award");
    const [[commTot]] = await pool.query("SELECT COUNT(*) AS committees FROM committees");
    const [[seatTot]] = await pool.query(`
      SELECT COALESCE(SUM(capacity), 0) AS total_seats,
             (SELECT COUNT(*) FROM allocations) AS filled_seats
      FROM portfolios
    `);
    const [[pendReq]] = await pool.query("SELECT COUNT(*) AS pending_requests FROM allocation_requests WHERE status = 'pending'");

    const [[exp]] = await pool.query(`
      SELECT
        SUM(CASE WHEN mun_attend <= 2 THEN 1 ELSE 0 END) AS beginners,
        SUM(CASE WHEN mun_attend BETWEEN 3 AND 6 THEN 1 ELSE 0 END) AS intermediate,
        SUM(CASE WHEN mun_attend >= 7 THEN 1 ELSE 0 END) AS experienced
      FROM delegates
    `);

    const [top_countries] = await pool.query("SELECT country AS label, COUNT(*) AS value FROM delegates GROUP BY country ORDER BY value DESC LIMIT 5");
    const [top_committees] = await pool.query("SELECT committee_name AS label, COUNT(*) AS value FROM previous_committees GROUP BY committee_name ORDER BY value DESC LIMIT 5");
    const [most_experienced] = await pool.query("SELECT name, country, mun_attend FROM delegates ORDER BY mun_attend DESC, delegate_id ASC LIMIT 5");
    const [recent_delegates] = await pool.query("SELECT name, country, mun_attend FROM delegates ORDER BY delegate_id DESC LIMIT 5");

    res.json({
      totals: {
        delegates: Number(tot.delegates || 0),
        countries: Number(tot.countries || 0),
        avg_muns: Number(tot.avg_muns || 0),
        total_muns: Number(tot.total_muns || 0),
        awards: Number(aw.awards || 0),
        avg_age: Number(tot.avg_age || 0),
        committees: Number(commTot.committees || 0),
        seats_left: Math.max(0, Number(seatTot.total_seats || 0) - Number(seatTot.filled_seats || 0)),
        pending_requests: Number(pendReq.pending_requests || 0),
      },
      experience: {
        beginners: Number(exp.beginners || 0),
        intermediate: Number(exp.intermediate || 0),
        experienced: Number(exp.experienced || 0),
      },
      top_countries,
      top_committees,
      most_experienced,
      recent_delegates,
    });
  } catch (e) { dbError(res, e); }
});

// Delegates CRUD (Admin only)
function parseDelegate(body) {
  const name = String(body.name ?? "").trim().slice(0, 100);
  const country = String(body.country ?? "").trim().slice(0, 100);
  const age = cleanInt(body.age);
  const mun = cleanInt(body.mun_attend);
  if (!name) return { error: "Name is required." };
  if (age === null || age < 1 || age > 120) return { error: "Enter a valid age." };
  if (!country) return { error: "Country is required." };
  if (mun === null) return { error: "Enter a valid number of MUNs attended." };
  return { name, age, country, mun_attend: mun, committees: cleanList(body.committees), awards: cleanList(body.awards) };
}

async function attachChildren(rows) {
  if (!rows.length) return rows;
  const ids = rows.map((r) => r.delegate_id);
  const [cs] = await pool.query("SELECT delegate_id, committee_name FROM previous_committees WHERE delegate_id IN (?)", [ids]);
  const [as] = await pool.query("SELECT delegate_id, award_name FROM award WHERE delegate_id IN (?)", [ids]);
  return rows.map((r) => ({
    id: r.delegate_id, name: r.name, age: r.age, country: r.country, mun_attend: r.mun_attend,
    committees: cs.filter((c) => c.delegate_id === r.delegate_id).map((c) => c.committee_name),
    awards: as.filter((a) => a.delegate_id === r.delegate_id).map((a) => a.award_name),
  }));
}

async function writeChildren(conn, id, committees, awards) {
  for (const c of committees)
    await conn.query("INSERT INTO previous_committees (delegate_id, committee_name) VALUES (?, ?)", [id, c]);
  for (const a of awards)
    await conn.query("INSERT INTO award (delegate_id, award_name) VALUES (?, ?)", [id, a]);
}

app.get("/api/delegates", requireAdmin, async (req, res) => {
  try {
    const q = String(req.query.q || "").trim();
    let rows;
    if (q) {
      const like = "%" + q + "%";
      [rows] = await pool.query(
        `SELECT * FROM delegates d WHERE d.name LIKE ? OR d.country LIKE ? OR CAST(d.age AS CHAR) LIKE ?
           OR EXISTS (SELECT 1 FROM previous_committees pc WHERE pc.delegate_id = d.delegate_id AND pc.committee_name LIKE ?)
           OR EXISTS (SELECT 1 FROM award a WHERE a.delegate_id = d.delegate_id AND a.award_name LIKE ?)
         ORDER BY d.delegate_id`,
        [like, like, like, like, like]
      );
    } else {
      [rows] = await pool.query("SELECT * FROM delegates ORDER BY delegate_id");
    }
    res.json(await attachChildren(rows));
  } catch (e) { dbError(res, e); }
});

app.post("/api/delegates", requireAdmin, async (req, res) => {
  const d = parseDelegate(req.body || {});
  if (d.error) return res.status(400).json({ error: d.error });
  try {
    const id = await withTransaction(async (conn) => {
      const [r] = await conn.query(
        "INSERT INTO delegates (name, age, country, mun_attend) VALUES (?, ?, ?, ?)",
        [d.name, d.age, d.country, d.mun_attend]
      );
      await writeChildren(conn, r.insertId, d.committees, d.awards);
      return r.insertId;
    });
    res.status(201).json({ id });
  } catch (e) { dbError(res, e); }
});

app.put("/api/delegates/:id", requireAdmin, async (req, res) => {
  const id = Number(req.params.id);
  const d = parseDelegate(req.body || {});
  if (d.error) return res.status(400).json({ error: d.error });
  try {
    const found = await withTransaction(async (conn) => {
      const [r] = await conn.query(
        "UPDATE delegates SET name=?, age=?, country=?, mun_attend=? WHERE delegate_id=?",
        [d.name, d.age, d.country, d.mun_attend, id]
      );
      if (r.affectedRows === 0) {
        const [chk] = await conn.query("SELECT 1 FROM delegates WHERE delegate_id=?", [id]);
        if (!chk.length) return false;
      }
      await conn.query("DELETE FROM previous_committees WHERE delegate_id=?", [id]);
      await conn.query("DELETE FROM award WHERE delegate_id=?", [id]);
      await writeChildren(conn, id, d.committees, d.awards);
      return true;
    });
    if (!found) return res.status(404).json({ error: "Delegate not found." });
    res.json({ ok: true });
  } catch (e) { dbError(res, e); }
});

app.delete("/api/delegates/:id", requireAdmin, async (req, res) => {
  const id = Number(req.params.id);
  try {
    const n = await withTransaction(async (conn) => {
      await conn.query("DELETE FROM previous_committees WHERE delegate_id=?", [id]);
      await conn.query("DELETE FROM award WHERE delegate_id=?", [id]);
      const [r] = await conn.query("DELETE FROM delegates WHERE delegate_id=?", [id]);
      return r.affectedRows;
    });
    if (!n) return res.status(404).json({ error: "Delegate not found." });
    res.json({ ok: true });
  } catch (e) { dbError(res, e); }
});

// Committees & Portfolios CRUD (Admin only, includes min_muns eligibility)
app.get("/api/committees", requireAdmin, async (req, res) => {
  try {
    const [comms] = await pool.query("SELECT id, name, min_muns FROM committees ORDER BY id");
    const [ports] = await pool.query("SELECT id, committee_id, name, capacity FROM portfolios ORDER BY id");
    const [allocs] = await pool.query("SELECT id, portfolio_id, delegate_name FROM allocations ORDER BY id");

    const allocByPort = new Map();
    for (const a of allocs) {
      if (!allocByPort.has(a.portfolio_id)) allocByPort.set(a.portfolio_id, []);
      allocByPort.get(a.portfolio_id).push({ id: a.id, delegate_name: a.delegate_name });
    }

    const portByComm = new Map();
    for (const p of ports) {
      const list = allocByPort.get(p.id) || [];
      const filled = list.length;
      const item = {
        id: p.id, committee_id: p.committee_id, name: p.name, capacity: p.capacity,
        filled, available: Math.max(0, p.capacity - filled), allocations: list,
      };
      if (!portByComm.has(p.committee_id)) portByComm.set(p.committee_id, []);
      portByComm.get(p.committee_id).push(item);
    }

    let totalSeats = 0, filledSeats = 0;
    const committees = comms.map((c) => {
      const pList = portByComm.get(c.id) || [];
      const cTotal = pList.reduce((sum, p) => sum + p.capacity, 0);
      const cFilled = pList.reduce((sum, p) => sum + p.filled, 0);
      totalSeats += cTotal;
      filledSeats += cFilled;
      return {
        id: c.id, name: c.name, min_muns: Number(c.min_muns || 0),
        total_seats: cTotal, filled_seats: cFilled, seats_left: Math.max(0, cTotal - cFilled),
        portfolios: pList,
      };
    });

    res.json({
      summary: {
        committees: committees.length,
        portfolios: ports.length,
        total_seats: totalSeats,
        filled_seats: filledSeats,
        seats_left: Math.max(0, totalSeats - filledSeats),
      },
      committees,
    });
  } catch (e) { dbError(res, e); }
});

app.post("/api/committees", requireAdmin, async (req, res) => {
  const name = String(req.body?.name ?? "").trim().slice(0, 100);
  const min_muns = cleanInt(req.body?.min_muns ?? 0);
  if (!name) return res.status(400).json({ error: "Committee name cannot be empty." });
  if (min_muns === null) return res.status(400).json({ error: "Minimum MUNs must be 0 or greater." });
  try {
    const [r] = await pool.query("INSERT INTO committees (name, min_muns) VALUES (?, ?)", [name, min_muns]);
    res.status(201).json({ id: r.insertId, name, min_muns });
  } catch (e) { dbError(res, e); }
});

app.put("/api/committees/:id", requireAdmin, async (req, res) => {
  const id = Number(req.params.id);
  const name = String(req.body?.name ?? "").trim().slice(0, 100);
  const min_muns = cleanInt(req.body?.min_muns ?? 0);
  if (!name) return res.status(400).json({ error: "Committee name cannot be empty." });
  if (min_muns === null) return res.status(400).json({ error: "Minimum MUNs must be 0 or greater." });
  try {
    const [r] = await pool.query("UPDATE committees SET name = ?, min_muns = ? WHERE id = ?", [name, min_muns, id]);
    if (!r.affectedRows) return res.status(404).json({ error: "Committee not found." });
    res.json({ ok: true });
  } catch (e) { dbError(res, e); }
});

app.delete("/api/committees/:id", requireAdmin, async (req, res) => {
  const id = Number(req.params.id);
  try {
    const [r] = await pool.query("DELETE FROM committees WHERE id = ?", [id]);
    if (!r.affectedRows) return res.status(404).json({ error: "Committee not found." });
    res.json({ ok: true });
  } catch (e) { dbError(res, e); }
});

app.post("/api/committees/:id/portfolios", requireAdmin, async (req, res) => {
  const committee_id = Number(req.params.id);
  const name = String(req.body?.name ?? "").trim().slice(0, 100);
  const capacity = cleanInt(req.body?.capacity);
  if (!name) return res.status(400).json({ error: "Portfolio name cannot be empty." });
  if (capacity === null || capacity <= 0) return res.status(400).json({ error: "Capacity must be greater than 0." });
  try {
    const [r] = await pool.query("INSERT INTO portfolios (committee_id, name, capacity) VALUES (?, ?, ?)", [committee_id, name, capacity]);
    res.status(201).json({ id: r.insertId });
  } catch (e) { dbError(res, e); }
});

app.put("/api/portfolios/:id", requireAdmin, async (req, res) => {
  const id = Number(req.params.id);
  const name = String(req.body?.name ?? "").trim().slice(0, 100);
  const capacity = cleanInt(req.body?.capacity);
  if (!name) return res.status(400).json({ error: "Portfolio name cannot be empty." });
  if (capacity === null || capacity <= 0) return res.status(400).json({ error: "Capacity must be greater than 0." });
  try {
    const [[cnt]] = await pool.query("SELECT COUNT(*) AS filled FROM allocations WHERE portfolio_id = ?", [id]);
    if (capacity < cnt.filled) {
      return res.status(400).json({ error: `Capacity cannot be less than allocated delegates (${cnt.filled}).` });
    }
    const [r] = await pool.query("UPDATE portfolios SET name = ?, capacity = ? WHERE id = ?", [name, capacity, id]);
    if (!r.affectedRows) return res.status(404).json({ error: "Portfolio not found." });
    res.json({ ok: true });
  } catch (e) { dbError(res, e); }
});

app.delete("/api/portfolios/:id", requireAdmin, async (req, res) => {
  const id = Number(req.params.id);
  try {
    const [r] = await pool.query("DELETE FROM portfolios WHERE id = ?", [id]);
    if (!r.affectedRows) return res.status(404).json({ error: "Portfolio not found." });
    res.json({ ok: true });
  } catch (e) { dbError(res, e); }
});

// Direct Admin Manual Allocation / Deallocation & Auto-Allocation
app.post("/api/allocations", requireAdmin, async (req, res) => {
  const portfolio_id = Number(req.body?.portfolio_id);
  const delegate_name = String(req.body?.delegate_name ?? "").trim().slice(0, 100);
  if (!portfolio_id || !delegate_name) return res.status(400).json({ error: "Portfolio ID and delegate name are required." });
  try {
    const [[port]] = await pool.query("SELECT id, capacity FROM portfolios WHERE id = ?", [portfolio_id]);
    if (!port) return res.status(404).json({ error: "Portfolio not found." });
    const [existing] = await pool.query("SELECT delegate_name FROM allocations WHERE portfolio_id = ?", [portfolio_id]);
    if (existing.length >= port.capacity) return res.status(400).json({ error: "Portfolio is already full." });
    if (existing.some((e) => e.delegate_name.toLowerCase() === delegate_name.toLowerCase())) {
      return res.status(409).json({ error: "Delegate is already allocated to this portfolio." });
    }
    const [r] = await pool.query("INSERT INTO allocations (portfolio_id, delegate_name) VALUES (?, ?)", [portfolio_id, delegate_name]);
    res.status(201).json({ id: r.insertId });
  } catch (e) { dbError(res, e); }
});

app.delete("/api/allocations/:id", requireAdmin, async (req, res) => {
  const id = Number(req.params.id);
  try {
    const [r] = await pool.query("DELETE FROM allocations WHERE id = ?", [id]);
    if (!r.affectedRows) return res.status(404).json({ error: "Allocation not found." });
    res.json({ ok: true });
  } catch (e) { dbError(res, e); }
});

app.post("/api/allocations/auto", requireAdmin, async (req, res) => {
  try {
    const [delegates] = await pool.query("SELECT name, mun_attend FROM delegates ORDER BY delegate_id");
    const [existingAllocs] = await pool.query("SELECT portfolio_id, delegate_name FROM allocations");
    const [ports] = await pool.query(`
      SELECT p.id, p.name AS portfolio, p.capacity, c.name AS committee, c.min_muns
      FROM portfolios p
      JOIN committees c ON c.id = p.committee_id
      ORDER BY c.min_muns DESC, c.id ASC, p.id ASC
    `);

    const allocatedNames = new Set(existingAllocs.map((a) => a.delegate_name.toLowerCase()));
    const filledCount = new Map();
    for (const a of existingAllocs) filledCount.set(a.portfolio_id, (filledCount.get(a.portfolio_id) || 0) + 1);

    const unallocated = delegates.filter((d) => !allocatedNames.has(d.name.toLowerCase()));
    const openPorts = ports
      .map((p) => ({ ...p, seats_left: p.capacity - (filledCount.get(p.id) || 0) }))
      .filter((p) => p.seats_left > 0);

    if (!unallocated.length || !openPorts.length) {
      return res.json({ engine: "js", allocated_count: 0, allocations: [], message: "No eligible unallocated delegates or vacant seats remaining." });
    }

    let plan = [];
    let engine = "js";

    if (fs.existsSync(ALLOCATOR)) {
      const tmpIn = path.join(os.tmpdir(), `mun_alloc_${Date.now()}.txt`);
      const sanitize = (s) => String(s).replace(/[\r\n|]/g, " ");
      const lines = [
        ...unallocated.map((d) => `DELEGATE|${sanitize(d.name)}|${d.mun_attend}`),
        ...openPorts.map((p) => `PORTFOLIO|${p.id}|${sanitize(p.committee)}|${sanitize(p.portfolio)}|${p.seats_left}|${Number(p.min_muns || 0)}`),
      ];
      fs.writeFileSync(tmpIn, lines.join("\n") + "\n", "utf8");
      try {
        const out = await run(ALLOCATOR, [tmpIn, "--json"]);
        const parsed = JSON.parse(out);
        plan = parsed.allocations || [];
        engine = "cpp";
      } finally {
        fs.unlink(tmpIn, () => {});
      }
    } else {
      // Same eligibility-aware descending-experience greedy logic as backend/allocator.cpp
      const sorted = [...unallocated].sort((a, b) => b.mun_attend - a.mun_attend);
      for (const d of sorted) {
        const target = openPorts.find((p) => p.seats_left > 0 && d.mun_attend >= Number(p.min_muns || 0));
        if (!target) continue;
        plan.push({ portfolio_id: target.id, delegate_name: d.name, committee: target.committee, portfolio: target.portfolio });
        target.seats_left--;
      }
    }

    if (plan.length) {
      await withTransaction(async (conn) => {
        for (const item of plan) {
          await conn.query("INSERT IGNORE INTO allocations (portfolio_id, delegate_name) VALUES (?, ?)", [item.portfolio_id, item.delegate_name]);
        }
      });
    }
    res.json({ engine, allocated_count: plan.length, allocations: plan });
  } catch (e) { dbError(res, e); }
});

// ===============================================================
// 5. PROGRAMMER REQUEST APPROVAL QUEUE (/api/requests/*)
// ===============================================================

app.get("/api/requests", requireAdmin, async (req, res) => {
  try {
    const [rows] = await pool.query(`
      SELECT r.request_id, r.user_id, r.delegate_id, r.delegate_name, r.portfolio_id, r.allocation_id,
             r.request_type, r.status, r.note, r.created_at, r.reviewed_at,
             p.name AS portfolio_name, p.capacity,
             c.name AS committee_name, c.min_muns,
             COALESCE(d.mun_attend, 0) AS delegate_muns
      FROM allocation_requests r
      JOIN portfolios p ON p.id = r.portfolio_id
      JOIN committees c ON c.id = p.committee_id
      LEFT JOIN delegates d ON d.delegate_id = r.delegate_id
      ORDER BY FIELD(r.status, 'pending', 'approved', 'rejected'), r.request_id DESC
    `);
    res.json(rows);
  } catch (e) { dbError(res, e); }
});

app.post("/api/requests/:id/approve", requireAdmin, async (req, res) => {
  const reqId = Number(req.params.id);
  try {
    const msg = await withTransaction(async (conn) => {
      const [rows] = await conn.query(
        `SELECT r.*, p.capacity, p.name AS portfolio_name, c.name AS committee_name
         FROM allocation_requests r
         JOIN portfolios p ON p.id = r.portfolio_id
         JOIN committees c ON c.id = p.committee_id
         WHERE r.request_id = ? FOR UPDATE`,
        [reqId]
      );
      if (!rows.length) throw Object.assign(new Error("Request not found."), { status: 404 });
      const r = rows[0];
      if (r.status !== "pending") throw Object.assign(new Error("Request has already been reviewed."), { status: 400 });

      if (r.request_type === "allocate") {
        const [existing] = await conn.query("SELECT delegate_name FROM allocations WHERE portfolio_id = ?", [r.portfolio_id]);
        if (existing.length >= r.capacity) {
          throw Object.assign(new Error("Cannot approve: portfolio is already full."), { status: 400 });
        }
        if (existing.some((e) => e.delegate_name.toLowerCase() === r.delegate_name.toLowerCase())) {
          throw Object.assign(new Error("Delegate is already allocated to this portfolio."), { status: 409 });
        }
        await conn.query("INSERT INTO allocations (portfolio_id, delegate_name) VALUES (?, ?)", [r.portfolio_id, r.delegate_name]);
        await conn.query("UPDATE allocation_requests SET status = 'approved', reviewed_at = NOW() WHERE request_id = ?", [reqId]);
        return `Approved allocation: ${r.delegate_name} -> ${r.committee_name} (${r.portfolio_name}).`;
      } else {
        if (r.allocation_id) {
          await conn.query("DELETE FROM allocations WHERE id = ?", [r.allocation_id]);
        } else {
          await conn.query("DELETE FROM allocations WHERE portfolio_id = ? AND delegate_name = ?", [r.portfolio_id, r.delegate_name]);
        }
        await conn.query("UPDATE allocation_requests SET status = 'approved', reviewed_at = NOW() WHERE request_id = ?", [reqId]);
        return `Approved deletion: removed ${r.delegate_name} from ${r.committee_name} (${r.portfolio_name}).`;
      }
    });
    res.json({ ok: true, message: msg });
  } catch (e) {
    if (e.status) return res.status(e.status).json({ error: e.message });
    dbError(res, e);
  }
});

app.post("/api/requests/:id/reject", requireAdmin, async (req, res) => {
  const reqId = Number(req.params.id);
  try {
    const [r] = await pool.query(
      "UPDATE allocation_requests SET status = 'rejected', reviewed_at = NOW() WHERE request_id = ? AND status = 'pending'",
      [reqId]
    );
    if (!r.affectedRows) return res.status(404).json({ error: "Pending request not found." });
    res.json({ ok: true, message: "Request rejected." });
  } catch (e) { dbError(res, e); }
});

// ---------------------------------------------------------------
// Start Server
// ---------------------------------------------------------------
const PORT = process.env.PORT || 3000;
ensureSchema().finally(() => {
  app.listen(PORT, () => {
    console.log(`MUN Management System running at http://localhost:${PORT}`);
  });
});
