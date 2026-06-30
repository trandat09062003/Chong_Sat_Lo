// config.h - configuration constants for Chong_Sat_Lo project

#ifndef CONFIG_H
#define CONFIG_H

// Wi‑Fi credentials – replace with your network details
#define WIFI_SSID "YourSSID"
#define WIFI_PASSWORD "YourPassword"

// SMTP settings – Gmail example (adjust if using different provider)
#define SMTP_SERVER "smtp.gmail.com"
#define SMTP_PORT 465 // TLS/SSL port
#define EMAIL_USER "your.email@gmail.com"      // Sender email address
#define EMAIL_PASS "your_app_password"        // App password or SMTP password
#define RECIPIENT_EMAIL "receiver@example.com" // Recipient for alerts

// Soil moisture sensor settings
#define MOISTURE_PIN 34               // Analog pin connected to sensor output
#define MOISTURE_THRESHOLD 1500      // Adjust after calibration (0‑4095 range)

// Alert timing
#define ALERT_INTERVAL_MS 1800000    // 30 minutes between alerts

#endif // CONFIG_H
