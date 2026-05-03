const express = require('express');
const mqtt = require('mqtt');
const mongoose = require('mongoose');
const cors = require('cors');

const app = express();
app.use(cors());
app.use(express.json());

// ===== MONGODB =====
mongoose.connect('mongodb+srv://sswmk:10092004@cluster0.mongodb.net/smartwater')
.then(() => console.log("✅ MongoDB Connected"))
.catch(err => console.log("❌ MongoDB Error:", err));

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
    let val = message.toString();

    console.log("📡", topic, val);

    if (topic === 'smartwater/flow') lastData.flow = Number(val);
    if (topic === 'smartwater/total') lastData.total = Number(val);
    if (topic === 'smartwater/alarm') lastData.alarm = Number(val);

    // simpan ke database
    await Data.create({ ...lastData });

  } catch (err) {
    console.log("❌ Error simpan data:", err);
  }
});

// ===== API =====
app.get('/', (req, res) => {
  res.send("API Smart Water Jalan");
});

app.get('/data', async (req, res) => {
  const data = await Data.find().sort({ time: -1 }).limit(20);
  res.json(data);
});

app.get('/latest', (req, res) => {
  res.json(lastData);
});

// ===== START SERVER =====
app.listen(3000, () => {
  console.log("🚀 Server running on http://localhost:3000");
});