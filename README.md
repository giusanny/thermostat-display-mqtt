# thermostat-display-mqtt
Interfaccia Termostato integrata con HomeAssistant tramite MQTT con ESP32-C3 e display 1.3inch con bottone ed encoder. Link del prodotto **https://github.com/VIEWESMART/UEDX24240013-MD50ESP32_1.3inch-Knob**

![knob](https://github.com/user-attachments/assets/2320dc91-32e5-41c9-b025-90a7331e96b5)

Prima di iniziare è necessario impostare Arduino, Board e relative librerie come descritto quì **https://github.com/VIEWESMART/ESP32-Arduino/tree/main** e più precisamente 
 - SETUP Arduino IDE **https://github.com/VIEWESMART/ESP32-Arduino/blob/main/docs/How_To_Configure_Arduino-esp32.md**
 - SETUP Board ESP32-C3-LCDkit come descritto **https://github.com/VIEWESMART/ESP32-Arduino/blob/main/docs/Board_Instructions.md**
 - SETUP librerie come nell'esempio **https://github.com/VIEWESMART/ESP32-Arduino/tree/main/examples/1.3inch** o copiare quelle presenti in questo repository

Principali funzionalià implementate:
 - lettura da MQTT della temperatura mostrata sull'arco di sfondo con gradiente di colore celeste-arancione 5-35 gradi Celsius
 - lettura/scrttura su MQTT della temperatura di setpoint desiderata mostrata sull'arco un primo piano con gradiente di colore celeste-arancione 5-35 gradi Celsius e modificabile girando l'encoder a destra o sinistra
 - pressione breve del bottone per cambiare Mode (off|cool|heat) tramite MQTT
 - pressione lunga (configurabile) del bottone per cambiare Preset Mode (boost|eco|altri da implementare) tramite MQTT
 - pressione molto lunga (configurabile) del bottone per resettare le configurazioni in EEPROM
 - Disattivazione display dopo 120secondi con riaccensione in conseguenza del movimento del bottone o dell'encoder
 - Setup configurazioni Wifi e id topic MQTT via web in EEPROM se non trovate crea un AP Thermostat_Config con password 12345678 con configurazione tramite http://192.168.4.1
   
