#include "ProtocolloEDS.h"

ProtocolloEDS::ProtocolloEDS(int rxPin, int txPin, int stx, int etx, bool debug) {
    DEBUG = debug;
    STX = stx;
    ETX = etx;
    serial = new SoftwareSerial(rxPin, txPin);
    deviceModello = 0;
    deviceVersione = 0;
    bufferStart = 0;
    bufferEnd = 0;

    // Inizializza le callback a nullptr (nessuna callback di default)
    messaggio7Callback = nullptr;
	messaggio21Callback = nullptr;
    messaggio26Callback = nullptr;
	messaggio51Callback = nullptr;
	messaggio53Callback = nullptr;
	messaggio55Callback = nullptr;
    messaggio220Callback = nullptr;

    // Inizializza l'indirizzo del dispositivo dalla EEPROM
    EEPROM.begin(EEPROM_SIZE);  // Inizializza la EEPROM con la dimensione corretta
    deviceIndirizzo = leggiIndirizzoEEPROM();
    if (deviceIndirizzo < 1 || deviceIndirizzo > 255) {
        deviceIndirizzo = DEFAULT_INDIRIZZO;
        salvaIndirizzoEEPROM(deviceIndirizzo);
    }

    ultimoMittente17 = -1;
    ultimoDestinatario17 = -1;
}

void ProtocolloEDS::begin(unsigned long baudRate) {
    serial->begin(baudRate);
    if (DEBUG) {
        Serial.begin(baudRate);
    }
}

void ProtocolloEDS::setDeviceInfo(int modello, int versione) {
    deviceModello = modello;
    deviceVersione = versione;
}

int ProtocolloEDS::getDeviceIndirizzo() {
    return deviceIndirizzo;
}

void ProtocolloEDS::setDeviceIndirizzo(int indirizzo) {
    if (indirizzo >= 1 && indirizzo <= 255) {
        deviceIndirizzo = indirizzo;
        salvaIndirizzoEEPROM(indirizzo);
    }
}

void ProtocolloEDS::salvaIndirizzoEEPROM(int indirizzo) {
    EEPROM.write(EEPROM_INDIRIZZO_ADDR, indirizzo);
    EEPROM.commit();
}

int ProtocolloEDS::leggiIndirizzoEEPROM() {
    return EEPROM.read(EEPROM_INDIRIZZO_ADDR);
}

void ProtocolloEDS::initUscitaDimmer(int pwmPin) {
    pinMode(pwmPin, OUTPUT);
}

void ProtocolloEDS::setMessaggio7Callback(void (*callback)(int destinatario, int &informativo1, int &informativo2)) {
    messaggio7Callback = callback;
}

void ProtocolloEDS::setMessaggio21Callback(void (*callback)(int destinatario, int uscita, int percentuale, int tempoAttivazioneDisattivazione, bool attivazioneDisattivazione)) {
    messaggio21Callback = callback;
}
void ProtocolloEDS::setMessaggio26Callback(void (*callback)(int &outStatus, int &inStatus)) {
    messaggio26Callback = callback;
}

void ProtocolloEDS::setMessaggio51Callback(void (*callback)(int destinatario, int percentuale, int uscita)) {
    messaggio51Callback = callback;
}

void ProtocolloEDS::setMessaggio53Callback(void (*callback)(int destinatario, int &percentualeUscita1, int &percentualeUscita2)) {
    messaggio53Callback = callback;
}

void ProtocolloEDS::setMessaggio55Callback(void (*callback)(int destinatario, int &informativo1, int &informativo2)) {
    messaggio55Callback = callback;
}

void ProtocolloEDS::setMessaggio220Callback(void (*callback)(int &informativo1, int &informativo2)) {
    messaggio220Callback = callback;
}

int ProtocolloEDS::calcolaChecksum(int destinatario, int mittente, int tipoMessaggio, int informativo1, int informativo2) {
    return (STX + destinatario + mittente + tipoMessaggio + informativo1 + informativo2) % 256;
}

bool ProtocolloEDS::verificaChecksum(int ricevutoChecksum, int calcolatoChecksum) {
    return ricevutoChecksum == calcolatoChecksum;
}

bool ProtocolloEDS::inviaMessaggio(int destinatario, int mittente, int tipoMessaggio, int informativo1, int informativo2) {
    int checksum = calcolaChecksum(destinatario, mittente, tipoMessaggio, informativo1, informativo2);

    serial->write(STX);
    serial->write(destinatario);
    serial->write(mittente);
    serial->write(tipoMessaggio);
    serial->write(informativo1);
    serial->write(informativo2);
    serial->write(checksum);
    serial->write(ETX);

    if (DEBUG) {
        Serial.print(" INVIATO ------> STX: "); Serial.print(STX);
        Serial.print(" DESTINATARIO: "); Serial.print(destinatario);
        Serial.print(" MITTENTE: "); Serial.print(mittente);
        Serial.print(" TIPO MESSAGGIO: "); Serial.print(tipoMessaggio);
        Serial.print(" BYTE INFORMATIVO 1: "); Serial.print(informativo1);
        Serial.print(" BYTE INFORMATIVO 2: "); Serial.print(informativo2);
        Serial.print(" CheckSum: "); Serial.print(checksum);
        Serial.print(" ETX: "); Serial.println(ETX);
    }

    return true;
}

void ProtocolloEDS::inviaACK(int destinatario, int mittente, int informativo1, int informativo2) {
    int tipoMessaggio = 6;  // Codice per ACK
    inviaMessaggio(mittente, destinatario, tipoMessaggio, informativo1, informativo2);
}

bool ProtocolloEDS::riceviMessaggio(int &destinatario, int &mittente, int &tipoMessaggio, int &informativo1, int &informativo2) {
    while (serial->available()) {
        buffer[bufferEnd] = serial->read();
        bufferEnd = (bufferEnd + 1) % BUFFER_SIZE;

        if (bufferEnd == bufferStart) {
            bufferStart = (bufferStart + 1) % BUFFER_SIZE;
        }
    }

    return estraiMessaggio(destinatario, mittente, tipoMessaggio, informativo1, informativo2);
}

bool ProtocolloEDS::estraiMessaggio(int &destinatario, int &mittente, int &tipoMessaggio, int &informativo1, int &informativo2) {
    while ((bufferEnd + BUFFER_SIZE - bufferStart) % BUFFER_SIZE >= 8) {
        int index = bufferStart;

        if (buffer[index] == STX && buffer[(index + 7) % BUFFER_SIZE] == ETX) {
            int tempDestinatario = buffer[(index + 1) % BUFFER_SIZE];
            int tempMittente = buffer[(index + 2) % BUFFER_SIZE];
            int tempTipoMessaggio = buffer[(index + 3) % BUFFER_SIZE];
            int tempInformativo1 = buffer[(index + 4) % BUFFER_SIZE];
            int tempInformativo2 = buffer[(index + 5) % BUFFER_SIZE];
            int ricevutoChecksum = buffer[(index + 6) % BUFFER_SIZE];

            int calcolatoChecksum = calcolaChecksum(tempDestinatario, tempMittente, tempTipoMessaggio, tempInformativo1, tempInformativo2);
            if (verificaChecksum(ricevutoChecksum, calcolatoChecksum)) {
                destinatario = tempDestinatario;
                mittente = tempMittente;
                tipoMessaggio = tempTipoMessaggio;
                informativo1 = tempInformativo1;
                informativo2 = tempInformativo2;

                bufferStart = (bufferStart + 8) % BUFFER_SIZE;  // Rimuovi il messaggio dal buffer

                if (DEBUG && (destinatario==deviceIndirizzo || tipoMessaggio == 17)) {
                    Serial.print(" RICEVUTO ------> STX: "); Serial.print(STX);
                    Serial.print(" DESTINATARIO: "); Serial.print(destinatario);
                    Serial.print(" MITTENTE: "); Serial.print(mittente);
                    Serial.print(" TIPO MESSAGGIO: "); Serial.print(tipoMessaggio);
                    Serial.print(" BYTE INFORMATIVO 1: "); Serial.print(informativo1);
                    Serial.print(" BYTE INFORMATIVO 2: "); Serial.print(informativo2);
                    Serial.print(" CheckSum: "); Serial.print(calcolatoChecksum);
                    Serial.print(" ETX: "); Serial.println(ETX);
                }
				
				if (!DEBUG && (destinatario==deviceIndirizzo || tipoMessaggio == 17)) {
                    Serial.print(" TIPO MESSAGGIO: "); Serial.print(tipoMessaggio);
					Serial.print(" BYTE INFORMATIVO 1: "); Serial.print(informativo1);
                    Serial.print(" BYTE INFORMATIVO 2: "); Serial.println(informativo2);
                }

                // Gestione del messaggio 17 (Comando di gruppo)
                if (tipoMessaggio == 17) {
                    if (mittente != ultimoMittente17 || destinatario != ultimoDestinatario17) {
                        ultimoMittente17 = mittente;
                        ultimoDestinatario17 = destinatario;

                        int attivazioneDisattivazione = informativo1 & 0x01;  // Bit 0
                        int gruppo = informativo2 & 0x1F;  // Bit 0-4
                        gestisciMessaggio17(attivazioneDisattivazione, gruppo);
                    }
                }

                if (destinatario == deviceIndirizzo || (isDimmer(deviceModello) && (destinatario == deviceIndirizzo+1 || destinatario == deviceIndirizzo+2 || destinatario == deviceIndirizzo+3))) {
                    switch (tipoMessaggio) {
                        case 0:  // Richiesta modello e versione
                            rispondiConInfo(destinatario, mittente);
                            break;
                         case 2:
                            setUscita(informativo1, informativo2);
                            inviaACK(destinatario, mittente, informativo1, informativo2);
                            break;
						case 5:
                            if (destinatario == deviceIndirizzo) //solo sul dispositivo reale e non su quelli virtuali
								setDeviceIndirizzo(informativo1);
                            inviaACK(destinatario, mittente, informativo1, informativo2);
                            break;
                        case 7:
                            if (messaggio7Callback != nullptr) {
                                rispondiConMessaggio8(destinatario, mittente);
                            }
                            break;
                        case 14:  // Associazione uscita a comando broadcast (messaggio 14)
                            gestisciMessaggio14(destinatario, informativo1, informativo2);
                            inviaACK(destinatario, mittente, informativo1, informativo2);
                            break;
						case 15:
							gestisciMessaggio15(destinatario, mittente, informativo1, informativo2);
							break;
                        case 18:
							gestisciMessaggio18(destinatario, mittente, informativo1, informativo2);
							inviaACK(destinatario, mittente, informativo1, informativo2);
							break;
                        case 19:
							gestisciMessaggio19(destinatario, mittente, informativo1, informativo2);
							break;
                        case 21:  // Attivazione/disattivazione di un'uscita
                            gestisciMessaggio21(destinatario, mittente, informativo1, informativo2);
                            inviaACK(destinatario, mittente, informativo1, informativo2);
                            break;
                        case 25:
							gestisciMessaggio25(destinatario, mittente, informativo1, informativo2);
                            break;
                        case 35:
							if (destinatario == deviceIndirizzo) //solo sul dispositivo reale e non su quelli virtuali
								resetDevice();
                            inviaACK(destinatario, mittente, informativo1, informativo2);
                            break;
						case 51:
                            if (messaggio51Callback != nullptr) {
								gestisciMessaggio51(destinatario, informativo1, informativo2);
							}
                            break;
						case 54:
                            // TODO
                            inviaACK(destinatario, mittente, informativo1, informativo2);
                            break;
                        case 55:
                            if (messaggio55Callback != nullptr) {
                                rispondiConMessaggio56(destinatario, mittente, informativo1, informativo2);
                            }
                            break;
                        case 220:
                            if (messaggio220Callback != nullptr) {
                                rispondiConMessaggio221(destinatario, mittente);
                            }
                            break;
                        case 240:
                            gestisciMessaggio240(informativo1);
                            inviaACK(destinatario, mittente, informativo1, informativo2);
                            break;
                        case 241:
                            gestisciMessaggio241(informativo1, informativo2);
                            inviaACK(destinatario, mittente, informativo1, informativo2);
                            break;
                        case 242:
                            gestisciMessaggio242(destinatario, mittente, informativo1);
                            break;
                        default:
                            break;
                    }
                }

                return true;
            } else {
                bufferStart = (bufferStart + 1) % BUFFER_SIZE;
            }
        } else {
            bufferStart = (bufferStart + 1) % BUFFER_SIZE;
        }
    }

    return false;
}

void ProtocolloEDS::resetDevice() {
    EEPROM.begin(EEPROM_SIZE);
    for (int i = 0; i < EEPROM_SIZE; i++) {
        EEPROM.write(i, 0xFF);  // Cancella la EEPROM impostando tutti i byte a 0xFF
    }
    EEPROM.commit();
}

void ProtocolloEDS::rispondiConInfo(int destinatario, int mittente) {
    inviaMessaggio(mittente, destinatario, 1, deviceModello, deviceVersione);  // Risposta a messaggio 0 con tipoMessaggio = 1
}

void ProtocolloEDS::setUscita(int informativo1, int informativo2) {
    //programma uscita che viene ignorato se è un DIMMER
	if (!isDimmer(deviceModello)) {
		//gestire la programmazione dell'uscita
	}
}

void ProtocolloEDS::rispondiConMessaggio8(int destinatario, int mittente) {
    int informativo1 = 0;
    int informativo2 = 0;

    if (messaggio7Callback != nullptr) {
        messaggio7Callback(destinatario, informativo1, informativo2);
    }

    inviaMessaggio(mittente, destinatario, 8, informativo1, informativo2);
}

void ProtocolloEDS::rispondiConMessaggio56(int destinatario, int mittente, int informativo1, int informativo2) {
    
    if (messaggio55Callback != nullptr) {
        messaggio55Callback(destinatario, informativo1, informativo2);
    }

    inviaMessaggio(mittente, destinatario, 56, informativo1, informativo2);
}

void ProtocolloEDS::rispondiConMessaggio221(int destinatario, int mittente) {
    int informativo1 = 0;
    int informativo2 = 0;

    if (messaggio220Callback != nullptr) {
        messaggio220Callback(informativo1, informativo2);
    }

    inviaMessaggio(mittente, destinatario, 221, informativo1, informativo2);
}

// Gestione del messaggio 14 (Associazione di un'uscita a un comando broadcast)
void ProtocolloEDS::gestisciMessaggio14(int destinatario, int informativo1, int informativo2) {
    int velocita = (informativo1 >> 3) & 0x0F;  // Bit 3-6 (Velocità)
    int uscita = informativo1 & 0x07;           // Bit 0-2 (Numero di uscita)
	
	if(isDimmer(deviceModello) && destinatario - deviceIndirizzo <=3) {  //se è un Dimmer ed è un modello virtuale fino a indirizzo + 3 
		uscita = uscita + destinatario - deviceIndirizzo;
	}
	
    int casellaMultipla = (informativo2 >> 6) & 0x03;  // Bit 6-7 (Numero della casella di attuazione multipla)
    int attivaInAttivazione = (informativo2 >> 5) & 0x01;  // Bit 5 (Attiva/disattiva in attivazione)
	
	if (isDimmer(deviceModello)) { 
		if (casellaMultipla == 0 && attivaInAttivazione == 0) {
			casellaMultipla = 0;
		} else if (casellaMultipla == 0 && attivaInAttivazione == 1) {
			casellaMultipla = 1;
		} else if (casellaMultipla == 1 && attivaInAttivazione == 0) {
			casellaMultipla = 2;
		} else if (casellaMultipla == 1 && attivaInAttivazione == 1) {
			casellaMultipla = 3;
		} else if (casellaMultipla == 2 && attivaInAttivazione == 0) {
			casellaMultipla = 4;
		} else if (casellaMultipla == 2 && attivaInAttivazione == 1) {
			casellaMultipla = 5;
		} else if (casellaMultipla == 3 && attivaInAttivazione == 0) {
			casellaMultipla = 6;
		} else if (casellaMultipla == 3 && attivaInAttivazione == 1) {
			casellaMultipla = 7;
		}
		attivaInAttivazione = 0;
	}
	
    int comandoBroadcast = informativo2 & 0x1F;  // Bit 0-4 (Numero del comando broadcast)

    if (DEBUG) {
        Serial.println("Gestione Messaggio 14:");
        Serial.print("Velocita: "); Serial.println(velocita);
        Serial.print("Uscita: "); Serial.println(uscita);
        Serial.print("Casella Attuazione Multipla: "); Serial.println(casellaMultipla);
        Serial.print("Attivazione: "); Serial.println(attivaInAttivazione ? "Attivazione" : "Disattivazione");
        Serial.print("Comando Broadcast: "); Serial.println(comandoBroadcast);
    }

    // Salva l'associazione nella EEPROM
	salvaAssociazioneUscitaEEPROM(uscita, velocita, casellaMultipla, attivaInAttivazione, comandoBroadcast);
}

void ProtocolloEDS::gestisciMessaggio15(int destinatario, int mittente, int informativo1, int informativo2) {
    int uscita = informativo1 & 0x07;  // Bit 0-2: Numero di uscita
	
	if(isDimmer(deviceModello) && destinatario - deviceIndirizzo <=3) {  //se è un Dimmer ed è un modello virtuale fino a indirizzo + 3 
		uscita = uscita + destinatario - deviceIndirizzo;
	}
	
    int casellaMultipla = informativo2 & 0x07; // Bit 0-2: Numero della casella di attuazione multipla
	
    // Leggi l'associazione dalla EEPROM
    int velocita, attivaInAttivazione, comandoBroadcast;
    leggiAssociazioneUscitaEEPROM(uscita, velocita, casellaMultipla, attivaInAttivazione, comandoBroadcast);

	if (DEBUG) {
		Serial.print("Uscita: "); Serial.println(uscita);
		Serial.print("Casella: "); Serial.println(casellaMultipla);
		Serial.print("attivaInAttivazione: "); Serial.println(attivaInAttivazione);
		Serial.print("comandoBroadcast: "); Serial.println(comandoBroadcast);
	}
	
	if(isDimmer(deviceModello) && destinatario - deviceIndirizzo <=3) {  //se è un Dimmer ed è un modello virtuale fino a indirizzo + 3 ripristina valore uscita
		uscita = informativo1 & 0x07; 
	}
    // Invia la risposta con il messaggio 16
    inviaRispostaMessaggio16(mittente, destinatario, velocita, uscita, casellaMultipla, attivaInAttivazione, comandoBroadcast);
}

void ProtocolloEDS::salvaAssociazioneUscitaEEPROM(int uscita, int velocita, int casellaMultipla, int attivaInAttivazione, int comandoBroadcast) {
    int indirizzoBase = EEPROM_BASE_USCITE + (uscita * EEPROM_SIZE_USCITA) + (casellaMultipla * EEPROM_SIZE_CASELLA);

    int primoByte = (velocita << 3) | (uscita & 0x07);  // Bit 3-6: velocità, Bit 0-2: uscita
    EEPROM.write(indirizzoBase, primoByte);

    int secondoByte = 0;
	if(isDimmer(deviceModello)){
		secondoByte = (casellaMultipla << 5) | (comandoBroadcast & 0x1F);  // Bit 5-6-7: casella, Bit 0-4: broadcast
	} else {
		secondoByte = (casellaMultipla << 6) | (attivaInAttivazione << 5) | (comandoBroadcast & 0x1F);  // Bit 6-7: casella, Bit 5: attivazione, Bit 0-4: broadcast
	}
    EEPROM.write(indirizzoBase + 1, secondoByte);

    EEPROM.commit();
}

void ProtocolloEDS::leggiAssociazioneUscitaEEPROM(int uscita, int &velocita, int casellaMultipla, int &attivaInAttivazione, int &comandoBroadcast) {
    int indirizzoBase = EEPROM_BASE_USCITE + (uscita * EEPROM_SIZE_USCITA) + (casellaMultipla * EEPROM_SIZE_CASELLA);

    int primoByte = EEPROM.read(indirizzoBase);
    velocita = (primoByte >> 3) & 0x0F;  // Bit 3-6: velocità
    //uscita = primoByte & 0x07;  // Bit 0-2: numero di uscita non serve rileggerla dalla EEPROM

    int secondoByte = EEPROM.read(indirizzoBase + 1);
	if(isDimmer(deviceModello)){
		//casellaMultipla = (secondoByte >> 6) & 0x03;  // Bit 6-7: casella multipla
		attivaInAttivazione = 0;  // Bit 5: attivazione/disattivazione valore fittizio in caso di Dimmer
		/*
		in questo caso DIMMER
		
			Bit 7-6 	bit 5 	Descrizione
			00b 		0b 		casella 1
			01b 		0b 		casella 3
			00b 		1b 		casella 2
			01b 		1b 		casella 4
			10b 		0b 		casella 5
			10b 		1b 		casella 6
			11b 		0b 		casella 7
			11b 		1b 		casella 8
		*/
	}else{
		//casellaMultipla = (secondoByte >> 6) & 0x03;  // Bit 6-7: casella multipla non serve rileggerla dalla EEPROM
		attivaInAttivazione = (secondoByte >> 5) & 0x01;  // Bit 5: attivazione/disattivazione
	}
    comandoBroadcast = secondoByte & 0x1F;  // Bit 0-4: comando broadcast
}

void ProtocolloEDS::gestisciMessaggio25(int destinatario, int mittente, int informativo1, int informativo2) {
    if (isDimmer(deviceModello)) {
        // Rispondi con lo stato delle uscite del DIMMER
        int percentualeUscita1 = 0;
        int percentualeUscita2 = 0;

        if (messaggio53Callback != nullptr) {
            messaggio53Callback(destinatario, percentualeUscita1, percentualeUscita2);
        }

        inviaRispostaMessaggio53(mittente, destinatario, percentualeUscita1, percentualeUscita2);
    } else {
        // Rispondi con lo stato delle uscite e ingressi per un dispositivo NON DIMMER
        int outStatus = 0;
        int inStatus = 0;

        if (messaggio26Callback != nullptr) {
            messaggio26Callback(outStatus, inStatus);
        }

        inviaRispostaMessaggio26(mittente, destinatario, outStatus, inStatus);
    }
}

void ProtocolloEDS::gestisciMessaggio51(int destinatario, int informativo1, int informativo2) {
    int percentuale = informativo1;
    int uscita = informativo2;

    if (messaggio51Callback != nullptr) {
        messaggio51Callback(destinatario, percentuale, uscita);
    }
}

void ProtocolloEDS::gestisciMessaggio240(int tipoDescrizione) {
    if (tipoDescrizione >= 0 && tipoDescrizione < NUM_DESCRIZIONI) {
        descrizioneSelezionata = tipoDescrizione;
    }
}

void ProtocolloEDS::gestisciMessaggio241(int indiceCarattere, int carattere) {
    // Assicurati che l'indiceCarattere e carattere siano validi
    if (indiceCarattere >= 0 && indiceCarattere < LUNGHEZZA_DESCRIZIONE) {
        // Modifica la descrizione selezionata nella EEPROM
        char descrizione[LUNGHEZZA_DESCRIZIONE + 1];
        leggiDescrizione(descrizione);

        descrizione[indiceCarattere] = carattere;

        scriviDescrizione(descrizione);
		
		if (DEBUG) {
			Serial.println(descrizione);
		}
    }
}

void ProtocolloEDS::gestisciMessaggio242(int destinatario, int mittente, int indiceCarattere) {
    // Leggi la descrizione selezionata dalla EEPROM
    char descrizione[LUNGHEZZA_DESCRIZIONE + 1];
    leggiDescrizione(descrizione);

	if (DEBUG) {
			Serial.println(descrizione);
	}
	
    if (indiceCarattere >= 0 && indiceCarattere < LUNGHEZZA_DESCRIZIONE) {
        // Invia il carattere richiesto
        inviaMessaggio(mittente, destinatario, 243, indiceCarattere, descrizione[indiceCarattere]);
    }
}

void ProtocolloEDS::leggiDescrizione(char *descrizione) {
    int indirizzoInizio = EEPROM_BASE_DESCRIZIONI + descrizioneSelezionata * LUNGHEZZA_DESCRIZIONE;
    for (int i = 0; i < LUNGHEZZA_DESCRIZIONE; i++) {
        descrizione[i] = EEPROM.read(indirizzoInizio + i);
    }
    descrizione[LUNGHEZZA_DESCRIZIONE] = '\0'; // Assicurati che la stringa sia terminata
}

void ProtocolloEDS::scriviDescrizione(const char *descrizione) {
    int indirizzoInizio = EEPROM_BASE_DESCRIZIONI + descrizioneSelezionata * LUNGHEZZA_DESCRIZIONE;
    for (int i = 0; i < LUNGHEZZA_DESCRIZIONE; i++) {
        EEPROM.write(indirizzoInizio + i, descrizione[i]);
    }
    EEPROM.commit();
}

void ProtocolloEDS::gestisciMessaggio17(int attivazioneDisattivazione, int gruppo) {
    // Gestisci il comando di gruppo (scenario)
    if (DEBUG) {
        Serial.print("Messaggio 17 ricevuto - Attivazione/Disattivazione: ");
        Serial.print(attivazioneDisattivazione);
        Serial.print(", Gruppo: ");
        Serial.println(gruppo);
    }
    // Implementa qui la logica per gestire gli scenari
}

void ProtocolloEDS::gestisciMessaggio18(int destinatario, int mittente, int informativo1, int informativo2) {
    int uscita = informativo1 & 0x07;  // Bit 0-2: Numero di uscita
	
	if(isDimmer(deviceModello) && destinatario - deviceIndirizzo <=3) {  //se è un Dimmer ed è un modello virtuale fino a indirizzo + 3 
		uscita = uscita + destinatario - deviceIndirizzo;
	}
	
    int casellaMultipla = (informativo1 >> 3) & 0x07;  // Bit 3-5: Numero della casella di attuazione multipla
    bool isMinuti = (informativo2 >> 7) & 0x01;  // Bit 7: Secondi o Minuti (non usato per dimmer)
    int timerPercentuale = informativo2 & 0x7F;  // Bit 0-6: Timer o Percentuale

    if (DEBUG) {
        Serial.print("Messaggio 18 ricevuto per uscita: "); Serial.println(uscita);
        Serial.print("Casella attuazione multipla: "); Serial.println(casellaMultipla);
        Serial.print("Timer/Percentuale: "); Serial.println(timerPercentuale);
        Serial.print("Espresso in "); Serial.println(isMinuti ? "minuti" : "secondi");
    }

    // Salva i dati nella EEPROM
    salvaTimerPercentualeEEPROM(uscita, casellaMultipla, isMinuti, timerPercentuale);
}

void ProtocolloEDS::gestisciMessaggio21(int destinatario, int mittente, int informativo1, int informativo2) {
    // Estrazione dei bit da informativo1 e informativo2
    int tempoAttivazioneDisattivazione = (informativo1 >> 3) & 0x0F;  // Bit 3-6
    int uscita = informativo1 & 0x07;                         // Bit 0-2
		
    int percentuale = (informativo2 >> 1) & 0x7F;            // Bit 1-7
    bool attivazioneDisattivazione = informativo2 & 0x01;              // Bit 0

    if (DEBUG) {
        Serial.print("Tempo di attivazione/disattivazione: "); Serial.println(tempoAttivazioneDisattivazione);
        Serial.print("Uscita da comandare: "); Serial.println(uscita);
        Serial.print("Percentuale di accensione: "); Serial.println(percentuale);
        Serial.print("Attivazione/Disattivazione: "); Serial.println(attivazioneDisattivazione ? "Attivazione" : "Disattivazione");
    }

    // Implementa qui la logica per comandare l'uscita
	if (messaggio21Callback != nullptr) {
            messaggio21Callback(destinatario, uscita, percentuale, tempoAttivazioneDisattivazione, attivazioneDisattivazione);
    }
}

// Funzione per determinare se l'uscita è un dimmer
bool ProtocolloEDS::isDimmer(int modello) {
    // Modifica questa logica in base a come definisci un dimmer nel tuo sistema
    return (modello==111);  
}

void ProtocolloEDS::inviaRispostaMessaggio16(int mittente, int destinatario, int velocita, int uscita, int casellaMultipla, int attivaInAttivazione, int comandoBroadcast) {
    int informativo1 = (velocita << 3) | (uscita & 0x07);  // Bit 3-6: velocità, Bit 0-2: uscita
    
	if (isDimmer(deviceModello)) { 
		if (casellaMultipla == 0) {
			casellaMultipla = 0;
			attivaInAttivazione = 0;
		} else if (casellaMultipla == 1) {
			casellaMultipla = 0;
			attivaInAttivazione = 1;
		} else if (casellaMultipla == 2) {
			casellaMultipla = 1;
			attivaInAttivazione = 0;
		} else if (casellaMultipla == 3) {
			casellaMultipla = 1;
			attivaInAttivazione = 1;
		} else if (casellaMultipla == 4) {
			casellaMultipla = 2;
			attivaInAttivazione = 1;
		} else if (casellaMultipla == 5) {
			casellaMultipla = 2;
			attivaInAttivazione = 1;
		} else if (casellaMultipla == 6) {
			casellaMultipla = 3;
			attivaInAttivazione = 1;
		} else if (casellaMultipla == 7) {
			casellaMultipla = 3;
			attivaInAttivazione = 1;
		}
	} 
	
	int informativo2 = (casellaMultipla << 6) | (attivaInAttivazione << 5) | (comandoBroadcast & 0x1F);  // Bit 6-7: casella, Bit 5: attivazione, Bit 0-4: broadcast
	
    inviaMessaggio(mittente, destinatario, 16, informativo1, informativo2);
}

void ProtocolloEDS::inviaRispostaMessaggio26(int mittente, int destinatario, int outStatus, int inStatus) {
    // Informativo 1: stato delle uscite
    int informativo1 = outStatus;
    // Informativo 2: stato degli ingressi
    int informativo2 = inStatus;

    inviaMessaggio(mittente, destinatario, 26, informativo1, informativo2);
}

void ProtocolloEDS::inviaRispostaMessaggio53(int mittente, int destinatario, int percentualeUscita1, int percentualeUscita2) {
    // Informativo 1: Percentuale di accensione uscita 1
    int informativo1 = percentualeUscita1 & 0x7F;  // Percentuale tra 0-127
    // Informativo 2: Percentuale di accensione uscita 2
    int informativo2 = percentualeUscita2 & 0x7F;  // Percentuale tra 0-127

    inviaMessaggio(mittente, destinatario, 53, informativo1, informativo2);
}

void ProtocolloEDS::gestisciMessaggio19(int destinatario, int mittente, int informativo1, int informativo2) {
    int uscita = informativo1 & 0x07;  // Bit 0-2: Numero di uscita
	
	if(isDimmer(deviceModello) && destinatario - deviceIndirizzo <=3) {  //se è un Dimmer ed è un modello virtuale fino a indirizzo + 3 
		uscita = uscita + destinatario - deviceIndirizzo;
	}
	
    int casellaMultipla = informativo2 & 0x07;  // Bit 0-2: Numero della casella di attuazione multipla
	
	// Variabili per il timer/percentuale
    int timerPercentuale = 0;
    bool isMinuti = false;

    // Leggi i valori dalla EEPROM usando la nuova funzione
    leggiTimerPercentualeEEPROM(uscita, casellaMultipla, isMinuti, timerPercentuale);

    if (DEBUG) {
		Serial.print("Uscita: "); Serial.println(uscita);
		Serial.print("Casella: "); Serial.println(casellaMultipla);
        if (isDimmer(uscita)) {
            Serial.print("Dimmer: Percentuale di accensione letta = "); Serial.println(timerPercentuale);
        } else {
            Serial.print("Uscita normale: Tempo di desincronizzazione letto = "); Serial.println(timerPercentuale);
            Serial.print("Espresso in "); Serial.println(isMinuti ? "minuti" : "secondi");
        }
    }

	if(isDimmer(deviceModello) && destinatario - deviceIndirizzo <=3) {  //se è un Dimmer ed è un modello virtuale fino a indirizzo + 3 ripristina valore uscita
		uscita = informativo1 & 0x07; 
	}
	
    // Invia la risposta con il messaggio 20
    inviaRispostaMessaggio20(mittente, destinatario, uscita, timerPercentuale, isMinuti);
}

void ProtocolloEDS::inviaRispostaMessaggio20(int mittente, int destinatario, int uscita, int timerPercentuale, bool isMinuti) {
    int informativo1 = uscita & 0x07;  // Bit 0-2: Numero di uscita
    int informativo2 = ((isMinuti ? 1 : 0) << 7) | (timerPercentuale & 0x7F);  // Bit 7: secondi/minuti, Bit 0-6: timer o percentuale

    inviaMessaggio(mittente, destinatario, 20, informativo1, informativo2);
}

void ProtocolloEDS::salvaTimerPercentualeEEPROM(int uscita, int casellaMultipla, bool isMinuti, int timerPercentuale) {
    int indirizzoBase = EEPROM_BASE_USCITE + (uscita * EEPROM_SIZE_USCITA) + (casellaMultipla * EEPROM_SIZE_CASELLA);

    // Primo byte: bit 7 = secondi/minuti, bit 0-6 = timer/percentuale
    int valore = (isMinuti << 7) | (timerPercentuale & 0x7F);
    EEPROM.write(indirizzoBase+2, valore);

    EEPROM.commit();

    if (DEBUG) {
        Serial.print("Salvato in EEPROM: Uscita "); Serial.print(uscita);
        Serial.print(", Casella "); Serial.print(casellaMultipla);
        Serial.print(", Valore "); Serial.println(valore);
    }
}

void ProtocolloEDS::leggiTimerPercentualeEEPROM(int uscita, int casellaMultipla, bool &isMinuti, int &timerPercentuale) {
    // Calcola l'indirizzo di partenza nella EEPROM per l'uscita e la casella specificata
    int indirizzoBase = EEPROM_BASE_USCITE + (uscita * EEPROM_SIZE_USCITA) + (casellaMultipla * EEPROM_SIZE_CASELLA);

    // Primo byte: Timer o percentuale
    int secondoByte = EEPROM.read(indirizzoBase+2);
	
    isMinuti = (secondoByte >> 7) & 0x01;  // Bit 7: Secondi o Minuti (non usato per dimmer)
    timerPercentuale = secondoByte & 0x7F;  // Bit 0-6: Timer o Percentuale
	
}
