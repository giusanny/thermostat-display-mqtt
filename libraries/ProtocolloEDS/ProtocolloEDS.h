#ifndef PROTOCOLLO_EDS_H 
#define PROTOCOLLO_EDS_H

#include <Arduino.h>
#include <SoftwareSerial.h>
#include <EEPROM.h>

#define DEFAULT_INDIRIZZO 255
#define EEPROM_INDIRIZZO_ADDR 0  // Indirizzo EEPROM per l'indirizzo del modulo
#define NUM_DESCRIZIONI 4  // Numero di descrizioni
#define LUNGHEZZA_DESCRIZIONE 15  // Lunghezza di ogni descrizione
#define EEPROM_BASE_DESCRIZIONI 1  // Indirizzo di inizio delle descrizioni
// Ogni descrizione occupa NUM_DESCRIZIONI * LUNGHEZZA_DESCRIZIONE byte
#define EEPROM_BASE_USCITE (EEPROM_BASE_DESCRIZIONI + (NUM_DESCRIZIONI * LUNGHEZZA_DESCRIZIONE))  // Indirizzo di inizio delle uscite
#define NUM_USCITE 8  // Numero di uscite
// Ogni casella occupa 2 byte (timer o percentuale + secondi/minuti)
#define EEPROM_SIZE_CASELLA 3  // Ogni casella occupa 3 byte byte5 e byte6 del msg16, byte6 del msg18
// Ogni uscita ha 8 caselle, quindi ogni uscita occupa 8 * 3 = 24 byte
#define EEPROM_SIZE_USCITA (EEPROM_SIZE_CASELLA * 8)  // 8 caselle per ogni uscita, ciascuna occupa 3 byte
// Dimensione totale della EEPROM
#define EEPROM_SIZE (EEPROM_BASE_USCITE + (NUM_USCITE * EEPROM_SIZE_USCITA))  // Dimensione totale EEPROM

#define DEFAULT_BAUD_RATE 9600
#define BUFFER_SIZE 128  // Dimensione del buffer circolare

class ProtocolloEDS {
public:
    // Costruttore della classe
    ProtocolloEDS(int rxPin, int txPin, int stx = 2, int etx = 3, bool debug = false);

    // Funzione di inizializzazione
    void begin(unsigned long baudRate = DEFAULT_BAUD_RATE);

    // Funzioni per inviare e ricevere messaggi
    bool inviaMessaggio(int destinatario, int mittente, int tipoMessaggio, int informativo1, int informativo2);
    bool riceviMessaggio(int &destinatario, int &mittente, int &tipoMessaggio, int &informativo1, int &informativo2);

    // Funzione per impostare le informazioni del dispositivo
    void setDeviceInfo(int modello, int versione);
	
	// Funzione per impostare una Uscita
    void setUscita(int informativo1, int informativo2);
	
    // Funzioni per gestire l'indirizzo del dispositivo
    int getDeviceIndirizzo();  // Restituisce l'indirizzo del dispositivo
    void setDeviceIndirizzo(int indirizzo);  // Imposta l'indirizzo del dispositivo e lo salva nella EEPROM

    // Funzione per l'inizializzazione dell'uscita dimmer
    void initUscitaDimmer(int pwmPin);

    // Funzione per inviare un ACK
    void inviaACK(int destinatario, int mittente, int informativo1, int informativo2);  // Funzione per inviare un ACK

    // Funzione per impostare il callback del messaggio 7
    void setMessaggio7Callback(void (*callback)(int destinatario, int &informativo1, int &informativo2));

	// Funzione per impostare il callback del messaggio 21
    void setMessaggio21Callback(void (*callback)(int destinatario, int uscita, int percentuale, int tempoAttivazioneDisattivazione, bool attivazioneDisattivazione));
	
	// Funzione per impostare il callback del messaggio 25
    void setMessaggio26Callback(void (*callback)(int &outStatus, int &inStatus));
	
	// Funzione per impostare il callback del messaggio 55
    void setMessaggio51Callback(void (*callback)(int destinatario, int percentuale, int uscita));
	
	// Funzione per impostare il callback del messaggio 25
    void setMessaggio53Callback(void (*callback)(int destinatario, int &percentualeUscita1, int &percentualeUscita2));
	
	// Funzione per impostare il callback del messaggio 55
    void setMessaggio55Callback(void (*callback)(int destinatario, int &informativo1, int &informativo2));
	
	// Funzione per impostare il callback del messaggio 220
    void setMessaggio220Callback(void (*callback)(int &informativo1, int &informativo2));
	
	// Funzione per definire se è un dimmer
    boolean isDimmer(int modello);

private:
    int STX;
    int ETX;
    int DEBUG;  // Flag di debug
    int deviceModello;  // Modello del dispositivo
    int deviceVersione;  // Versione del dispositivo
    int deviceIndirizzo;  // Indirizzo del dispositivo
    int descrizioneSelezionata;  // Variabile per tracciare la descrizione selezionata

    SoftwareSerial *serial;  // Comunicazione seriale software

    byte buffer[BUFFER_SIZE];  // Buffer circolare per la ricezione dei messaggi
    int bufferStart;  // Inizio del buffer
    int bufferEnd;  // Fine del buffer

    void (*messaggio7Callback)(int destinatario, int &informativo1, int &informativo2);  // Callback per il messaggio 7
	void (*messaggio21Callback)(int destinatario, int uscita, int percentuale, int tempoAttivazioneDisattivazione, bool attivazioneDisattivazione);  // Callback per il messaggio 21
	void (*messaggio26Callback)(int &outStatus, int &inStatus);  // Callback per il messaggio 25 NON DIMMER
	void (*messaggio51Callback)(int destinatario, int percentuale, int uscita);  // Callback per il messaggio 51 DIMMER
	void (*messaggio53Callback)(int destinatario, int &percentualeUscita1, int &percentualeUscita2);  // Callback per il messaggio 25 DIMMER
	void (*messaggio55Callback)(int destinatario, int &informativo1, int &informativo2);  // Callback per il messaggio 55
	void (*messaggio220Callback)(int &informativo1, int &informativo2);  // Callback per il messaggio 220
		
    // Funzioni per il calcolo del checksum e la verifica
    int calcolaChecksum(int destinatario, int mittente, int tipoMessaggio, int informativo1, int informativo2);
    bool verificaChecksum(int ricevutoChecksum, int calcolatoChecksum);

    // Funzione per estrarre un messaggio dal buffer
    bool estraiMessaggio(int &destinatario, int &mittente, int &tipoMessaggio, int &informativo1, int &informativo2);

    // Funzioni per rispondere a messaggi specifici
    void rispondiConInfo(int destinatario, int mittente);
    void rispondiConMessaggio8(int destinatario, int mittente);
	
	void rispondiConMessaggio56(int destinatario, int mittente, int informativo1, int informativo2);
	void rispondiConMessaggio221(int destinatario, int mittente);
	
    // Funzioni per la gestione della EEPROM
    void leggiDescrizione(char *descrizione);
    void scriviDescrizione(const char *descrizione);
    void salvaIndirizzoEEPROM(int indirizzo);
    int leggiIndirizzoEEPROM();
	void resetDevice();

    // Funzioni per gestire specifici tipi di messaggi
    void gestisciMessaggio14(int destinatario, int informativo1, int informativo2);
    void gestisciMessaggio15(int destinatario, int mittente, int informativo1, int informativo2);
    void gestisciMessaggio17(int attivazioneDisattivazione, int gruppo);
	void gestisciMessaggio18(int destinatario, int mittente, int informativo1, int informativo2);
	void gestisciMessaggio19(int destinatario, int mittente, int informativo1, int informativo2);
	void gestisciMessaggio21(int destinatario, int mittente, int informativo1, int informativo2);
	void gestisciMessaggio25(int destinatario, int mittente, int informativo1, int informativo2);
	void gestisciMessaggio51(int destinatario, int informativo1, int informativo2);
	void gestisciMessaggio240(int tipoDescrizione);
    void gestisciMessaggio241(int indiceCarattere, int carattere);
    void gestisciMessaggio242(int destinatario, int mittente, int indiceCarattere);
    
    // Variabili per gestire il messaggio 17 (timestamp per evitare duplicati)
    int ultimoMittente17;
    int ultimoDestinatario17;

    // Funzioni per salvare e leggere le associazioni delle uscite (messaggio 14)
    void salvaAssociazioneUscitaEEPROM(int uscita, int velocita, int casellaMultipla, int attivaInAttivazione, int comandoBroadcast);
    void leggiAssociazioneUscitaEEPROM(int uscita, int &velocita, int casellaMultipla, int &attivaInAttivazione, int &comandoBroadcast);
	
	// Funzione per salvere e leggere il timer o la percentuale dalla EEPROM
	void salvaTimerPercentualeEEPROM(int uscita, int casellaMultipla, bool isMinuti, int timerPercentuale);
	void leggiTimerPercentualeEEPROM(int uscita, int casellaMultipla, bool &isMinuti, int &timerPercentuale);
	
	//risposta a messaggio 15 con 16
	void inviaRispostaMessaggio16(int mittente, int destinatario, int velocita, int uscita, int casellaMultipla, int attivaInAttivazione, int comandoBroadcast);
	
	// Funzione per inviare la risposta con il messaggio 20
	void inviaRispostaMessaggio20(int mittente, int destinatario, int uscita, int timerPercentuale, bool isMinuti);
	
	void inviaRispostaMessaggio26(int mittente, int destinatario, int outStatus, int inStatus);
	void inviaRispostaMessaggio53(int mittente, int destinatario, int percentualeUscita1, int percentualeUscita2);

};

#endif
