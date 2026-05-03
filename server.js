require('dotenv').config();

const express = require('express');
const mqtt = require('mqtt');
const mongoose = require('mongoose');
const cors = require('cors');
const jwt = require('jsonwebtoken');

const app = express();
app.use(cors());
app.use(express.json());

const PORT = process.env.PORT || 3000;
const SECRET = "smartwater_secret";

// ===== FILTER ANTI DOUBLE =====
let lastSource = "";
let lastTime = 0;

// ===== MONGODB =====
mongoose.connect(process.env.MONGO_URL)
.then(() => console.log("✅ MongoDB Connected"))
.catch(err => console.log("❌ MongoDB Error:", err.message));

// ===== SCHEMA =====
const Data = mongoose.model('Data', {
  flow: Number,
  total: Number,
  alarm: Number,
  source: String,
  time: { type: Date, default: Date.now }
});

// ===== MQTT =====
const client = mqtt.connect('mqtt://broker.emqx.io');

let lastData = { flow: 0, total: 0, alarm: 0, source: "MQTT" };

client.on('connect', () => {
  console.log("✅ MQTT Connected");
  client.subscribe('smartwater/#');
});

client.on('message', async (topic, message) => {
  try {

    if (lastSource === "GSM" && Date.now() - lastTime < 5000) return;

    const val = message.toString();

    if (topic === 'smartwater/flow') lastData.flow = Number(val);
    if (topic === 'smartwater/total') lastData.total = Number(val);
    if (topic === 'smartwater/alarm') lastData.alarm = Number(val);

    lastSource = "MQTT";
    lastTime = Date.now();

    lastData.source = "MQTT";

    console.log("📡 MQTT:", lastData);

    await Data.create(lastData);

  } catch (err) {
    console.log("❌ MQTT Error:", err.message);
  }
});

// ===== GSM HTTP =====
app.get('/update', async (req, res) => {
  try {

    if (lastSource === "MQTT" && Date.now() - lastTime < 5000) {
      return res.send("SKIP");
    }

    const flow = Number(req.query.flow) || 0;
    const total = Number(req.query.total) || 0;
    const alarm = Number(req.query.alarm) || 0;

    const data = {
      flow,
      total,
      alarm,
      source: "GSM"
    };

    lastData = data;

    lastSource = "GSM";
    lastTime = Date.now();

    console.log("📲 GSM:", data);

    await Data.create(data);

    res.send("OK");

  } catch (err) {
    console.log("❌ GSM Error:", err.message);
    res.status(500).send("ERROR");
  }
});

// ===== AUTH =====
function auth(req, res, next) {
  const token = req.headers.authorization;
  if (!token) return res.status(401).json({ msg: "No token" });

  try {
    jwt.verify(token, SECRET);
    next();
  } catch {
    res.status(403).json({ msg: "Invalid token" });
  }
}

// ===== LOGIN =====
app.post('/login', (req, res) => {
  const { username, password } = req.body;

  if (username === "admin" && password === "1234") {
    const token = jwt.sign({ user: username }, SECRET);
    return res.json({ token });
  }

  res.status(401).json({ msg: "Login gagal" });
});

// ===== API =====
app.get('/', (req, res) => {
  res.send("API Smart Water 🚀");
});

app.get('/latest', (req, res) => {
  res.json(lastData);
});

app.get('/history', auth, async (req, res) => {
  const data = await Data.find().sort({ time: -1 }).limit(200);
  res.json(data);
});

app.get('/export', async (req, res) => {
  const data = await Data.find().limit(200);

  let csv = "flow,total,alarm,source,time\n";
  data.forEach(d => {
    csv += `${d.flow},${d.total},${d.alarm},${d.source},${d.time}\n`;
  });

  res.header('Content-Type', 'text/csv');
  res.attachment('data.csv');
  res.send(csv);
});

// ===== START =====
app.listen(PORT, () => {
  console.log(`🚀 Server running on port ${PORT}`);
});