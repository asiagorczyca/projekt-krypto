# 🔐 Projekt Krypto – Encrypted Messenger

A lightweight, secure communication tool built with **Boost.Asio** and the **Diffie-Hellman key exchange protocol**. This project demonstrates practical cryptographic principles in a real-time messaging context.

---

## 📦 Features

- ✅ **End-to-End Encryption** — Session keys negotiated via Diffie-Hellman
- ⚙️ **Boost-Powered Networking** — Asynchronous socket handling
- 🧩 **Modular Architecture** — Clear separation of networking, crypto, and interface layers
- 🧪 **Minimal Interface** — Focused on functionality and cryptographic testing

---

## 🎯 Goals

This project aims to:
- Showcase secure key exchange using Diffie-Hellman
- Serve as a base for future extensions like encrypted file transfer or user authentication
- Provide a clean, testable environment for experimenting with cryptographic protocols

---

## 🖼️ Architecture Overview

<p align="center">
  <img src="diffie_hellmanns.jpg" alt="Diffie-Hellman Key Exchange Diagram" width="500"/>
</p>

---

## 🚀 Getting Started

```bash
# Clone the repository
git clone https://github.com/asiagorczyca/projekt-krypto.git

# Build instructions (example)
cd projekt-krypto
mkdir build && cd build
cmake ..
make

