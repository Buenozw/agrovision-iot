# 🛰️ AgroVision IoT — Monitoramento de Campo com ESP32

> Protótipo IoT para detecção de condições que favorecem pragas em lavouras,
> integrando sensores físicos de campo com a plataforma AgroVision.

**Global Solution 2026/1 — FIAP | 2º Ano ADS**  
**Disciplina:** Disruptive Architectures: IoT, IoB & Generative IA

🎥 **Vídeo demonstração (≤ 3 min):** _[link YouTube]_  
🔗 **Repositório GitHub:** https://github.com/Buenozw/agrovision-iot.git
🌐 **Simulação Wokwi:** _[link do projeto Wokwi]_

---

## 👥 Integrantes

| RM | Nome | Turma |
|----|------|-------|
| RM 562074 | João Victor Caetano Alves da Silva | 2TDSPF |
| RM 564115 | João Victor Bueno Castelini da Silva | 2TDSPF |
| RM 565667 | Ryan Vetoriano | 2TDSPF |
| RM 562766 | Felipe Furlanetto | 2TDSPF |
| RM 564002 | Raul Rezende Iemini Aguiar | 2TDSPF |

---

## 💡 Contexto da Solução

O **AgroVision IoT** é o módulo físico da plataforma AgroVision.
Enquanto os satélites (ESA Sentinel-2) fornecem análises de índice NDVI em
escala regional, o sensor ESP32 instalado no campo confirma e complementa os
dados em tempo real — medindo temperatura, umidade do ar e umidade do solo,
e calculando uma **estimativa local de NDVI** para detecção imediata de
condições que indicam infestação de pragas.

---

## 🔌 Hardware / Componentes

### Entradas (2)
| Componente | Pino ESP32 | Dado coletado |
|-----------|-----------|---------------|
| **DHT22** | GPIO 4 | Temperatura (°C) e Umidade do Ar (%) |
| **Potenciômetro** (simula sensor de solo) | GPIO 34 (ADC) | Umidade do Solo (0–100%) |

### Saídas (2)
| Componente | Pino ESP32 | Comportamento |
|-----------|-----------|---------------|
| **LED Verde** | GPIO 26 | Aceso = campo saudável |
| **LED Vermelho** | GPIO 27 | Aceso = alerta crítico · Pisca = observação |

### Interface Local (1)
| Componente | Pinos | Exibição |
|-----------|-------|----------|
| **LCD 16x2 I2C** | SDA=GPIO 21, SCL=GPIO 22 | 3 telas rotativas |

---

## 📡 Endpoints REST

| Método | Rota | Descrição |
|--------|------|-----------|
| `GET` | `/api/status` | JSON completo com todos os dados dos sensores |
| `GET` | `/api/ndvi` | JSON com NDVI estimado e recomendação |
| `GET` | `/api/alerta` | JSON com status do alerta atual e causas |
| `POST` | `/api/alerta/reset` | Reseta o alerta manualmente |

---

## 🖥️ Dashboard Externo (VS Code / Navegador)

O arquivo **`dashboard.html`** é um dashboard standalone que abre direto no navegador.

### Como usar:
1. Abra `dashboard.html` no navegador (clique duplo ou `Go Live` no VS Code)
2. Inicie a simulação no Wokwi
3. Copie o IP do ESP32 do Serial Monitor do Wokwi
4. Cole o IP no campo do dashboard e clique **Conectar**
5. O dashboard consultará `/api/status` e `/api/alerta` a cada 2 segundos automaticamente

---

## 🗂️ Estrutura de Arquivos

```
agrovision-iot/
├── agrovision_iot.ino   ← Código principal ESP32
├── dashboard.html        ← Dashboard externo (abre no navegador/VS Code)
├── diagram.json          ← Circuito Wokwi
├── libraries.txt         ← Bibliotecas necessárias
└── README.md             ← Esta documentação
```

---

## 📦 Bibliotecas

| Biblioteca | Versão | Finalidade |
|-----------|--------|------------|
| DHT sensor library | 1.4.6 | Leitura do DHT22 |
| LiquidCrystal I2C | 1.1.2 | Controle do LCD 16x2 |
| ArduinoJson | 6.21.5 | Serialização JSON das respostas |
