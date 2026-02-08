DROP DATABASE IF EXISTS purs;
CREATE DATABASE purs;
USE purs;


CREATE TABLE sensor (
  id INT AUTO_INCREMENT PRIMARY KEY,
  temperature FLOAT,
  humidity FLOAT,
  gas INT,
  lux FLOAT,
  ir_state TINYINT,
  hall_state INT,
  ventilator TINYINT,
  created_at DATETIME DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE authorized_cards (
  id INT AUTO_INCREMENT PRIMARY KEY,
  uid VARCHAR(20) NOT NULL UNIQUE,
  owner_name VARCHAR(20)
);

CREATE TABLE entry_logs (
  id INT AUTO_INCREMENT PRIMARY KEY,
  uid VARCHAR(20),
  created_at DATETIME DEFAULT CURRENT_TIMESTAMP
);

INSERT INTO sensor (temperature, humidity, gas, lux, ir_state,hall_state)
VALUES
(22.5, 45.0, 350, 120.5, 1,1),
(26.8, 50.2, 420, 300.0, 0,1),
(31.2, 55.8, 1800, 50.3, 1,1),
(36.4, 60.1, 2300, 20.0, 0,1);

CREATE TABLE parking_log (
  id INT AUTO_INCREMENT PRIMARY KEY,
  zauzeto TINYINT(1) NOT NULL,
  created_at DATETIME DEFAULT CURRENT_TIMESTAMP
);


INSERT INTO sensor
(temperature, humidity, gas, lux, ir_state, hall_state, ventilator)
VALUES
(22.5, 45.2, 0, 300.5, 1, 0, 0),
(25.1, 50.0, 1, 150.3, 0, 1, 1),
(20.8, 40.7, 0, 500.0, 1, 0, 0);


INSERT INTO parking_log 
(zauzeto)
VALUES
(1),
(0),
(1);

SELECT id, temperature, humidity, gas, lux, ir_state, hall_state, ventilator, created_at
FROM sensor
ORDER BY id DESC
LIMIT 5;

SELECT id, zauzeto, created_at
FROM parking_log
ORDER BY id DESC
LIMIT 5;

INSERT INTO authorized_cards (uid, owner_name)
VALUES ('1AADAD89', 'Admin');
INSERT INTO authorized_cards (uid, owner_name)
VALUES ('1AADAD89', 'Korisnik');