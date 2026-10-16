// server.js - bridges the web page and the C++ cv_parser binary
const express = require("express");
const multer = require("multer");
const path = require("path");
const fs = require("fs");
const os = require("os");
const { execFile } = require("child_process");

const app = express();
const upload = multer({ dest: os.tmpdir(), limits: { fileSize: 5 * 1024 * 1024 } });

// Path to the compiled C++ binary. Adjust if your folders differ.
const PARSER = path.join(__dirname, "..", "backend", "cv_parser");

app.use(express.static(path.join(__dirname, "public")));

function run(cmd, args) {
  return new Promise((resolve, reject) => {
    execFile(cmd, args, { maxBuffer: 10 * 1024 * 1024 }, (err, stdout, stderr) => {
      if (err) return reject(new Error(stderr || err.message));
      resolve(stdout);
    });
  });
}

app.post("/api/analyze", upload.single("cv"), async (req, res) => {
  if (!req.file) return res.status(400).json({ error: "No file uploaded." });
  const tmpFiles = [req.file.path];
  try {
    let textPath = req.file.path;
    const isPdf = /\.pdf$/i.test(req.file.originalname) || req.file.mimetype === "application/pdf";

    if (isPdf) {
      // needs poppler: brew install poppler
      const text = await run("pdftotext", ["-layout", req.file.path, "-"]);
      textPath = req.file.path + ".txt";
      fs.writeFileSync(textPath, text);
      tmpFiles.push(textPath);
    }

    const out = await run(PARSER, [textPath, "--json"]);
    res.json(JSON.parse(out));
  } catch (e) {
    res.status(500).json({ error: e.message.trim() });
  } finally {
    tmpFiles.forEach((f) => fs.unlink(f, () => {}));
  }
});

const PORT = process.env.PORT || 3000;
app.listen(PORT, () => console.log(`Running at http://localhost:${PORT}`));
