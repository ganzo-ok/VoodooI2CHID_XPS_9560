# VoodooI2CHID per Dell XPS 15 9560 (macOS Tahoe)

Fork di [VoodooI2CHID](https://github.com/VoodooI2C/VoodooI2CHID), il satellite HID di
[VoodooI2C](https://github.com/VoodooI2C/VoodooI2C) 2.9.1, corretto per il **touchscreen del Dell XPS 15 9560**
su un Hackintosh con **macOS Tahoe 26** (testato su 26.7.1, build 25G241).

Problema risolto: con il touchscreen abilitato il Mac **non si spegneva**. Allo spegnimento andava in kernel panic
oppure si bloccava.

## Stato attuale: versione 1.1 (Rev 16) ✅

| Funzione | 1.0 (Rev 9) | **1.1 (Rev 16)** |
|---|---|---|
| Tocco con un dito, trascinamento, clic destro con pressione prolungata | ✅ | ✅ |
| Scroll a due dita, zoom | ❌ | ✅ |
| Spegnimento e riavvio senza panic né blocchi | ✅ | ✅ |
| Touchpad I2C e tastiera PS2 | ✅ invariati | ✅ invariati |

La versione 1.1 è composta da **due kext**: questo VoodooI2CHID e un **VoodooInput 1.1.7 modificato**
(patch nella cartella [`VoodooInput_patch`](VoodooInput_patch)). Entrambi sono nella [release 1.1](../../releases/tag/v1.1).
Chi non vuole toccare VoodooInput può restare sulla [1.0](../../releases/tag/v1.0) (senza gesti sul touchscreen).

## Configurazione di prova

- Dell XPS 15 9560, OpenCore, SMBIOS MacBookPro16,4
- Touchscreen collegato via **USB**, gestito dal driver Apple DriverKit `AppleUserUSBHostHIDDevice`
- Touchpad **I2C** (TPD1), tastiera PS2
- VoodooI2C 2.9.1, VoodooPS2Controller 2.3.8 (con plugin VoodooInput 1.1.7; il VoodooInput dentro VoodooI2C.kext è
  disabilitato nel config.plist)
- boot-args usati per la diagnosi: `-v debug=0x100 keepsyms=1 -vi2c-force-polling`

## Il problema

Allo spegnimento si presentava uno di questi due casi:

- **kernel panic** in `IOHIDDevice::didTerminate + 0x1e` (`CR2 = 0x48`, `RAX = 0`, thread
  `terminateThread` → `actionDidTerminate`);
- oppure un **blocco** subito dopo
  `DK: IOUserServer(com.apple.driverkit.AppleUserHIDDrivers)::systemHalt()` / `terminate(AppleUserUSBHostHIDDevice…)`.

## Diagnosi

- Disassemblando `IOHIDFamily` di Tahoe, `didTerminate + 0x1e` è `movq 0x48(%rax)` con `rax = this->_reserved`.
  `_reserved` viene azzerato solo in `IOHIDDevice::free()`, quindi un IOHIDDevice viene **liberato prima del suo
  `didTerminate`** (use-after-free). In un test `_reserved` conteneva addirittura memoria riutilizzata (`RAX = 0x30004`).
- Sotto il touchscreen **USB** VoodooI2CHID costruisce un intero sottoalbero:

  ```
  AppleUserUSBHostHIDDevice → IOHIDInterface → VoodooI2CTouchscreenHIDEventDriver
    → VoodooI2CMultitouchInterface → VoodooI2CNativeEngine → VoodooInput
      → VoodooInputSimulatorDevice / VoodooInputActuatorDevice (IOHIDDevice)
      → TrackpointDevice → IOHIDPointingEventDevice + IOHIDSystem
  ```

  Il panic arriva sempre subito dopo `VoodooInputActuatorDevice … close` e `VoodooInputSimulatorDevice … close`.
- Senza quel sottoalbero (Rev 9) lo spegnimento è pulito, ma si perdono i gesti.
- Prove successive (Rev 10–15, vedi cronologia):
  - correggere `VoodooInput::stop`, trattenere lo shim del trackpoint, togliere del tutto il trackpoint **non basta**;
  - anche terminando il sottoalbero **in anticipo**, prima che DriverKit inizi lo spegnimento, il panic resta.
  
  Conclusione: su Tahoe **è la terminazione stessa** del sottoalbero VoodooInput a mandare in panic, chiunque la avvii.
  Il touchpad I2C ha lo stesso identico sottoalbero e non dà problemi solo perché allo spegnimento nessuno lo termina.
- Soluzione (Rev 16): all'avviso di spegnimento il sottoalbero **non viene terminato ma staccato** dal driver
  touchscreen e lasciato vivo, come quello del touchpad. Quando DriverKit termina il touchscreen trova il driver senza
  figli (come nella Rev 9) e lo spegnimento è pulito.

## Modifiche rispetto all'upstream

### VoodooI2CHID (questo repository)

`VoodooI2CHIDDevice.cpp/.hpp`
- override di `didTerminate`, che chiama direttamente `IOService::didTerminate` (Rev 8).

`VoodooI2CMultitouchHIDEventDriver.cpp/.hpp`
- power management solo per i trasporti non USB (Rev 8);
- in `handleStop` l'interfaccia multitouch viene fermata e staccata a mano solo se non è già in terminazione
  (`isInactive()`) (Rev 9);
- l'interfaccia multitouch viene creata anche su USB, quindi i gesti funzionano (Rev 10);
- **Rev 15/16:** su USB il driver si registra con `registerInterest(gIOPriorityPowerStateInterest, …)`. Alla prima
  notifica di spegnimento o riavvio (`kIOMessageSystemPagingOff`, `…WillPowerOff`, `…WillRestart`) smette di inoltrare
  i tocchi e fa `detach` dell'interfaccia multitouch **senza terminarla**, tenendone il riferimento;
- log di diagnostica `R13 …` nel percorso di terminazione (visibili solo con `-v`).

`VoodooI2CTouchscreenHIDEventDriver.cpp`
- `handleStop` rimuove e cancella tutti e 3 i timer, mentre prima ne rimuoveva solo 1 (Rev 9).

`Info.plist`
- rimossa la personality "VoodooI2CHIDDevice Generic Mouse HID Event Driver".

### VoodooInput 1.1.7 ([`VoodooInput_patch`](VoodooInput_patch))

- `stop()` non fa stop/detach a mano dei figli già in terminazione (Rev 10);
- `TrackpointDevice` trattiene i propri figli durante la terminazione (Rev 12);
- **Rev 14:** se VoodooInput si trova sotto un dispositivo con `Transport = USB` **non crea il `TrackpointDevice`**,
  inutile per un touchscreen. Touchpad I2C e PS2 restano invariati.

## Cronologia delle revisioni

| Revisione | Risultato |
|---|---|
| upstream / Rev ≤ 7 | panic `IOHIDDevice::didTerminate` allo spegnimento |
| Rev 8 (override `didTerminate`, PM solo non-USB) | a volte blocco, a volte ancora panic |
| Rev 9 (niente VoodooInput sotto USB, stop sicuro, 3 timer) | ✅ spegnimento pulito, ma niente gesti → **release 1.0** |
| Rev 10 (interfaccia di nuovo su USB + `VoodooInput::stop` corretto) | gesti ok, panic identico |
| Rev 11 (come la 10 + molti log in VoodooInput) | 3 spegnimenti puliti, ma solo perché i log rallentano: una gara di tempi |
| Rev 12 (Trackpoint trattiene lo shim) | a volte blocco, a volte panic |
| Rev 13 (log nel percorso di terminazione) | blocco |
| Rev 14 (niente Trackpoint su USB) | panic identico: il trackpoint non era la causa |
| Rev 15 (terminazione anticipata all'avviso di spegnimento) | panic: è la terminazione in sé il problema |
| **Rev 16** (stacco senza terminazione all'avviso di spegnimento) | ✅ **gesti + spegnimento pulito** → **release 1.1** |

## Installazione (1.1)

1. Scarica `VoodooI2CHID-1.1-Rev16.zip` dalla [release 1.1](../../releases/tag/v1.1).
2. Sostituisci `EFI/OC/Kexts/VoodooI2CHID.kext` con quello dello zip (VoodooI2C.kext 2.9.1 resta quello ufficiale).
3. Sostituisci il **VoodooInput.kext abilitato nel tuo config.plist** con quello dello zip. Nel config di prova è
   `EFI/OC/Kexts/VoodooPS2Controller.kext/Contents/PlugIns/VoodooInput.kext`. Deve essere caricato un solo VoodooInput.
4. Il config.plist non va modificato: i percorsi dei kext restano gli stessi.
5. Riavvia e controlla che siano caricati:
   ```bash
   kmutil showloaded --list-only | grep -E "VoodooI2CHID|VoodooInput"
   ```
   UUID attesi: VoodooI2CHID `B836914F-583B-3C94-B43C-AF460525EA15`,
   VoodooInput `5FB48ADD-FA34-34DC-83B2-E297DA26FC35`.

## Compilazione

- **VoodooI2CHID:** il workflow GitHub Actions (`Actions → Compila VoodooI2CHID XPS 9560 → Run workflow`) clona
  VoodooI2C 2.9.1, inserisce questi sorgenti al posto del satellite VoodooI2CHID e compila tutti i kext.
- **VoodooInput:** vedi [`VoodooInput_patch/README.md`](VoodooInput_patch/README.md). Il workflow **non** compila
  VoodooInput patchato: il VoodooInput che produce è quello originale.

## Prossimi passi

- Altri cicli di test della 1.1, anche dalla EFI su disco (senza `-v`).
- Capire con certezza chi libera `VoodooInputSimulatorDevice`/`VoodooInputActuatorDevice` durante la terminazione
  su Tahoe, per poter riparare VoodooInput invece di evitarne la terminazione.

## Crediti

VoodooI2C e VoodooI2CHID: Alexandre Daoud e i contributori di VoodooI2C. VoodooInput: Kishor Prins, Leonard Kleinhans
e acidanthera.
