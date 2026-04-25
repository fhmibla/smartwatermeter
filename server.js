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
const API = "https://sharksmartwatermeterbyami.netlify.app/";
const SECRET = "smartwater_secret";

// ===== MONGODB =====
mongoose.connect(process.env.MONGO_URL)
.then(() => console.log("✅ MongoDB Connected"))
.catch(err => console.log("❌ MongoDB Error:", err.message));

// ===== SCHEMA =====
const Data = mongoose.model('Data', {
  flow: Number,
  total: Number,
  alarm: Number,
  time: { type: Date, default: Date.now }
});

// ===== MQTT =====
const client = mqtt.connect('mqtt://broker.emqx.io');

let lastData = { flow: 0, total: 0, alarm: 0 };

client.on('connect', () => {
  console.log("✅ MQTT Connected");
  client.subscribe('smartwater/#');
});

client.on('message', async (topic, message) => {
  try {
    const val = message.toString();

    if (topic === 'smartwater/flow') lastData.flow = Number(val);
    if (topic === 'smartwater/total') lastData.total = Number(val);
    if (topic === 'smartwater/alarm') lastData.alarm = Number(val);

    console.log("📡", lastData);

    await Data.create({ ...lastData });

  } catch (err) {
    console.log("❌ Error:", err.message);
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
  const data = await Data.find().sort({ time: -1 }).limit(50);
  res.json(data);
});

app.get('/export', async (req, res) => {
  const data = await Data.find().limit(100);

  let csv = "flow,total,alarm,time\n";
  data.forEach(d => {
    csv += `${d.flow},${d.total},${d.alarm},${d.time}\n`;
  });

  res.header('Content-Type', 'text/csv');
  res.attachment('data.csv');
  res.send(csv);
});

// ===== START =====
app.listen(PORT, () => {
  console.log(`🚀 Server running on http://localhost:${PORT}`);
});