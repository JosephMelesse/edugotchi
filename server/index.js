require('dotenv').config();
const express = require('express');
const path    = require('path');
const routes  = require('./routes');

const app  = express();
const PORT = 3000;

app.use(express.json());
app.use(express.static(path.join(__dirname, 'public')));
app.use('/', routes);

app.listen(PORT, '0.0.0.0', () => {
  console.log(`Server running on port ${PORT} (all interfaces)`);
  console.log(`Parent portal: http://<device-ip>:${PORT}/portal`);
});
