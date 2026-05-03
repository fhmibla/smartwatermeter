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

// ======================
// MONGODB
// ======================
mongoose.connect(process.env.MONGO_URL)
.then(() => console.log("✅ MongoDB Connected"))
.catch(err => console.log("❌ MongoDB Error:", err.message));

// ======================
// SCHEMA
// ======================
const Data = mongoose.model('Data', {
  flow: Number,
  total: Number,
  alarm: Number,
  source: String,
  time: { type: Date, default: Date.now }
});

// ======================
// LAST DATA
// ======================
let lastData = {
  flow: 0,
  total: 0,
  alarm: 0,
  source: "MQTT"
};

// ======================
// MQTT
// ======================
const client = mqtt.connect('mqtt://broker.emqx.io');

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

    lastData.source = "MQTT";

    console.log("📡 MQTT:", lastData);

    await Data.create({ ...lastData });

  } catch (err) {
    console.log("❌ MQTT Error:", err.message);
  }
});

// ======================
// AUTH
// ======================
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

// ======================
// LOGIN
// ======================
app.post('/login', (req, res) => {
  const { username, password } = req.body;

  if (username === "admin" && password === "1234") {
    const token = jwt.sign({ user: username }, SECRET);
    return res.json({ token });
  }

  res.status(401).json({ msg: "Login gagal" });
});

// ======================
// ROOT
// ======================
app.get('/', (req, res) => {
  res.send("🚀 Smart Water API Running");
});

// ======================
// LATEST
// ======================
app.get('/latest', (req, res) => {
  res.json(lastData);
});

// ======================
// HISTORY
// ======================
app.get('/history', auth, async (req, res) => {
  const data = await Data.find().sort({ time: -1 }).limit(50);
  res.json(data);
});

// ======================
// GSM ENDPOINT
// ======================
app.get('/update', async (req, res) => {
  try {
    const { flow, total, alarm } = req.query;

    const data = {
      flow: Number(flow),
      total: Number(total),
      alarm: Number(alarm),
      source: "GSM"
    };

    lastData = data;

    console.log("📡 GSM:", data);

    await Data.create(data);

    res.json({ status: "OK GSM" });

  } catch (err) {
    res.status(500).json({ error: err.message });
  }
});

// ======================
// EXPORT CSV
// ======================
app.get('/export', async (req, res) => {
  const data = await Data.find().limit(100);

  let csv = "flow,total,alarm,source,time\n";

  data.forEach(d => {
    csv += `${d.flow},${d.total},${d.alarm},${d.source},${d.time}\n`;
  });

  res.header('Content-Type', 'text/csv');
  res.attachment('data.csv');
  res.send(csv);
});

// ======================
// WEEKLY (LITER)
// ======================
app.get('/weekly', async (req, res) => {

  const data = await Data.find({
    time: { $gte: new Date(Date.now() - 7*24*60*60*1000) }
  });

  let result = {};

  data.forEach(d => {
    const day = new Date(d.time).toLocaleDateString();

    if (!result[day]) result[day] = 0;

    result[day] += d.flow;
  });

  res.json(result);
});

// ======================
// STATS (MQTT vs GSM)
// ======================
app.get('/stats', async (req, res) => {

  const data = await Data.find();

  let mqtt = 0;
  let gsm = 0;

  data.forEach(d => {
    if (d.source === "MQTT") mqtt++;
    if (d.source === "GSM") gsm++;
  });

  res.json({ mqtt, gsm });
});

// ======================
// START SERVER
// ======================
app.listen(PORT, () => {
  console.log(`🚀 Server running on port ${PORT}`);
});